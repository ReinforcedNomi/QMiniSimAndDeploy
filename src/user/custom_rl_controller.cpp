#include "user/custom_rl_controller.h"
#include <iostream>
#include <algorithm>

void CustomRLController::init(const std::string& model_path) {
    // 初始化ONNX环境
    env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "custom_rl");
    Ort::SessionOptions session_options;
    
    try {
        motion_session = new Ort::Session(*env, model_path.c_str(), session_options);
        onnxInference.init(OBS_DIM, ACTION_DIM, OBS_HISTORY);
        std::cout << "Custom RL model loaded: " << model_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load custom RL model: " << e.what() << std::endl;
        throw;
    }

    // 初始化观测历史
    obs_history.resize(OBS_HISTORY);
    for (int i = 0; i < OBS_HISTORY; ++i) {
        obs_history[i].resize(OBS_DIM);
        obs_history[i].setZero();
    }
    observation.resize(TOTAL_OBS_DIM);
    observation.setZero();

    // 初始化状态
    joint_pos.setZero();
    joint_vel.setZero();
    joint_act.setZero();
    base_rpy.setZero();
    base_rpy_rate.setZero();
    base_quat << 1, 0, 0, 0;
    last_action.setZero();
    velocity_command.setZero();

    // 初始化限位（可以从config读取，这里使用默认值）
    act_pos_low << -0.1, -0.3, -2.1, 0.0, -2.5, -0.7, -0.6, 0.0, -2.1, 0.0;
    act_pos_high << 0.7, 0.6, 0.0, 2.1, 0.0, 0.1, 0.3, 2.1, 0.0, 2.5;
    _ref_joint_act << 0.55, 0.25, -1.35, 1.2, -1.1, -0.55, -0.22, 1.35, -1.2, 1.1;

    // 初始化刚度阻尼
    _kp << 1, 0.5, 1.5, 1, 1, 1, 0.5, 1.5, 1, 1;
    _kd << 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05;

    _is_first_run = true;
    counter_rl = 0;
}

void CustomRLController::reset() {
    convert_dds_state2rl_state();
    joint_act = joint_pos;
    last_action.setZero();
    velocity_command.setZero();
    
    // 重置观测历史
    for (int i = 0; i < OBS_HISTORY; ++i) {
        obs_history[i].setZero();
    }
    _is_first_run = true;
    counter_rl = 0;
}

void CustomRLController::convert_dds_state2rl_state() {
    if (dds_motor_state->GetData()) {
        for (int i = 0; i < NUM_JOINTS; ++i) {
            joint_pos[i] = dds_motor_state->GetData()->q[i];
            joint_vel[i] = dds_motor_state->GetData()->dq[i];
        }
    }

    if (dds_base_state->GetData()) {
        // 从std::array转换为Eigen向量
        for (int i = 0; i < 3; ++i) {
            base_rpy[i] = dds_base_state->GetData()->rpy.at(i);
            base_rpy_rate[i] = dds_base_state->GetData()->omega.at(i);
        }
        for (int i = 0; i < 4; ++i) {
            base_quat[i] = dds_base_state->GetData()->quat.at(i);
        }
    }
}

Matrix<float, Dynamic, 1> CustomRLController::build_single_frame_obs() {
    Matrix<float, Dynamic, 1> obs;
    obs.resize(OBS_DIM);
    obs.setZero();

    // 1. base_ang_vel: 基座角速度 (3维) ×0.2
    obs.segment(0, 3) = base_rpy_rate * 0.2f;

    // 2. projected_gravity: 投影重力向量 (3维)
    // 使用四元数计算重力在基座坐标系中的投影
    Vec3<float> gravity_world(0, 0, -1);  // 世界坐标系重力方向
    Vec3<float> gravity_base = this->quat_rotate_inverse(base_quat, gravity_world);
    obs.segment(3, 3) = gravity_base;

    // 3. velocity_commands: 速度命令 (3维) - 来自摇杆
    obs.segment(6, 3) = velocity_command;

    // 4. joint_pos_rel: 关节位置偏差 (10维) - 相对于参考位置
    obs.segment(9, 10) = joint_pos - _ref_joint_act;

    // 5. joint_vel_rel: 关节速度 (10维) ×0.05
    obs.segment(19, 10) = joint_vel * 0.05f;

    // 6. last_action: 上一步动作 (10维)
    obs.segment(29, 10) = last_action;

    return obs;
}

void CustomRLController::process_joystick_input() {
    if (!jsreader) return;

    // 左摇杆X/Y方向 -> velocity_command的x和y
    velocity_command[0] = jsreader->Axis[0];  // LaxiX -> vx
    velocity_command[1] = jsreader->Axis[1];  // LaxiY -> vy

    // 右摇杆Z自转方向 -> velocity_command的z（角速度）
    velocity_command[2] = jsreader->Axis[2];  // RaxiX -> yaw_rate
}

void CustomRLController::custom_rl_control() {
    counter_rl++;
    
    // 处理摇杆输入
    process_joystick_input();

    // 构建当前帧观测
    Matrix<float, Dynamic, 1> current_obs = build_single_frame_obs();

    // 更新观测历史
    if (_is_first_run) {
        // 首次运行，用当前观测填充所有历史帧
        for (int i = 0; i < OBS_HISTORY; ++i) {
            obs_history[i] = current_obs;
        }
        _is_first_run = false;
    } else {
        // 滑动窗口：移除最旧的一帧，添加最新的一帧
        obs_history.erase(obs_history.begin());
        obs_history.push_back(current_obs);
    }

    // 构建195维观测向量（5帧历史堆叠）
    for (int i = 0; i < OBS_HISTORY; ++i) {
        for (int j = 0; j < OBS_DIM; ++j) {
            observation[OBS_DIM * i + j] = obs_history[i][j];
        }
    }

    // ONNX推理
    Matrix<float, Dynamic, 1> action = onnxInference.inference(motion_session, observation);
    
    // 动作限制到[-1, 1]（onnxInference已经做了，这里确保）
    action = action.cwiseMax(-1.0f).cwiseMin(1.0f);

    // 位置增量控制：将动作转换为关节位置增量
    // 假设动作范围是[-1, 1]，需要映射到实际的关节增量范围
    // 这里使用简单的线性映射，可以根据需要调整
    float max_increment = 0.1f;  // 最大增量（rad），可以根据需要调整
    Vec10<float> joint_increment;
    for (int i = 0; i < ACTION_DIM; ++i) {
        joint_increment[i] = action[i] * max_increment * _rl_time_step;
    }

    // 更新关节目标位置（增量更新）
    joint_act += joint_increment;
    
    // 限位保护
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);

    // 保存当前动作用于下一帧
    last_action = action;

    _rl_time_step = 0.01f;  // 可以根据实际循环周期调整
}

void CustomRLController::set_joint_act2dds_motor_command() {
    MotorCommand motor_command_tmp;
    for (int i = 0; i < NUM_JOINTS; ++i) {
        motor_command_tmp.q_target[i] = joint_act[i];
        motor_command_tmp.kp[i] = _kp[i];
        motor_command_tmp.kd[i] = _kd[i];
        motor_command_tmp.tau_ff[i] = 0.;
        motor_command_tmp.dq_target[i] = 0.;
    }
    dds_motor_command->SetData(motor_command_tmp);
}

// 辅助函数：四元数旋转逆变换
Vec3<float> CustomRLController::quat_rotate_inverse(Vec4<float> q, Vec3<float> v) {
    Vec3<float> a, b, c;
    // q={w,x,y,z}
    float q_w = q[0];
    Vec3<float> q_vec(q[1], q[2], q[3]);
    a = v * (2.0f * q_w * q_w - 1.0f);
    b = q_vec.cross(v) * q_w * 2.0f;
    c = q_vec * q_vec.transpose() * v * 2.0f;
    return a - b + c;
}

