#include "user/rl_controller.h"
#include <algorithm>

/**
 * @brief 初始化强化学习控制器，包括ONNX模型的加载和所有状态变量的初始化
 * 
 * 此函数在程序启动时被调用一次，负责：
 * 1. 加载训练好的ONNX模型文件 (policy.onnx)
 * 2. 初始化ONNX推理器，设置输入/输出维度
 * 3. 初始化所有状态变量和配置参数
 * 
 * ONNX模型部署流程：
 * - 创建ONNX Runtime环境 (Ort::Env)
 * - 创建会话选项 (Ort::SessionOptions)
 * - 从文件加载模型并创建会话 (Ort::Session)
 * - 模型文件路径: "policy.onnx" (相对于可执行文件运行目录)
 */
void RLController::init() {
    // ========== ONNX模型加载部分 ==========
    // 创建ONNX Runtime环境，日志级别为WARNING，环境名称为"q1"
    // 注意：Ort::Env应该在整个程序生命周期内保持存在
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "q1");
    
    // 创建会话选项，可以在这里配置执行提供者(CPU/GPU)、线程数等
    Ort::SessionOptions session_options;
    
    // 加载ONNX模型文件并创建推理会话
    // 参数说明：
    //   - env: ONNX Runtime环境
    //   - configParams.onnx_model_path: 模型文件路径（从config.yaml读取）
    //   - session_options: 会话配置选项
    // 注意：模型在程序启动时一次性加载到内存，后续推理直接使用此会话
    motion_session = new Ort::Session(env, configParams.onnx_model_path.c_str(), session_options);
    
    // ========== 初始化ONNX推理器 ==========
    // 初始化推理器，设置输入/输出维度（从config.yaml读取）
    // 参数说明：
    //   - num_observations: 单帧观测维度 (43)
    //   - num_actions: 动作输出维度 (12)
    //   - num_stacks: 观测堆叠帧数 (3)
    // 实际输入维度 = num_observations * num_stacks = 43 * 3 = 129
    onnxInference.init(configParams.num_observations, configParams.num_actions, configParams.num_stacks);
    
    // ========== 初始化状态变量 ==========
    _offset_joint_act.setZero();
    jointIndex2Sim << 0, 1, 2, 3, 4, 5, 6, 7, 8, 9;  // 关节索引映射
    base_rpy.setZero();          // 基座姿态角 (roll, pitch, yaw)
    base_vel.setZero();          // 基座速度
    joint_pos.setZero();          // 关节位置
    joint_vel.setZero();         // 关节速度
    joint_tau.setZero();         // 关节力矩
    joint_acc.setZero();         // 关节加速度
    base_acc.setZero();          // 基座加速度
    base_quat << 1, 0, 0, 0;      // 基座四元数 (w, x, y, z)，初始为单位四元数
    base_rpy_rate.setZero();     // 基座角速度
    target_command.setZero();    // 目标速度命令 (vx, yr)
    joint_pos_error.setZero();   // 关节位置误差
    pm_f.setConstant(0.5f);      // 相位频率，初始化为0.5Hz
    _pm_phase << 0, 0;           // 相位角，初始化为0
    pm_phase_sin_cos.setZero();  // 相位的sin和cos值
    joint_vel_target.setZero();  // 目标关节速度（用于回零时的速度前馈）
    
    // 初始化动作增量向量，大小为模型输出维度
    action_increment.resize(onnxInference.output_dim);
    action_increment.setZero();

    // ========== 从配置文件加载参数 ==========
    for (int i = 0; i < NUM_JOINTS; ++i) {
        act_pos_low[i] = configParams.act_pos_low.at(i);      // 关节位置下限
        act_pos_high[i] = configParams.act_pos_high.at(i);     // 关节位置上限
        _ref_joint_act[i] = configParams.ref_joint_act.at(i);  // 参考关节位置（站立姿态）
        _kp[i] = configParams.kp.at(i);                        // 位置刚度系数
        _kd[i] = configParams.kd.at(i);                        // 速度阻尼系数
        _kp_soft[i] = configParams.kp_soft.at(i);              // 软控制位置刚度
        _kd_soft[i] = configParams.kd_soft.at(i);              // 软控制速度阻尼
        _torque_limit[i] = configParams.torque_limit.at(i);    // 力矩上限
    }
    torque_exceed_duration.setZero();  // 初始化力矩超过持续时间
    _torque_protection_active = false;  // 初始化力矩保护状态
    joint_act = _ref_joint_act;  // 初始关节动作设为参考位置
    
    // ========== 初始化观测向量和堆叠缓冲区 ==========
    // 观测向量总大小 = 单帧观测维度 × 堆叠帧数 = 43 × 3 = 129
    observation.resize(onnxInference.input_dim * onnxInference.stack_dim);
    observation.setZero();
    
    // 初始化观测堆叠缓冲区，用于存储历史观测帧
    // obs_stack是一个队列，存储最近num_stacks帧的观测
    obs_stack.resize(onnxInference.stack_dim);
    for (int i(0); i < onnxInference.stack_dim; i++)obs_stack.at(i).resize(onnxInference.input_dim);
    for (int i(0); i < onnxInference.stack_dim; i++)obs_stack.at(i).setZero(onnxInference.input_dim);

}

void RLController::reset(bool is_test_local) {
    pm_f.setConstant(0.5f);
    _pm_phase << 0, 0;
    target_command.setZero();
    _is_first_run = true;
    counter_rl = 0;
    joint_vel_target.setZero();  // 重置速度目标
    torque_exceed_duration.setZero();  // 重置力矩超过持续时间
    _torque_protection_active = false;  // 重置力矩保护状态
    if (is_test_local) {
        init_joint_act = joint_act;
        joint_pos = joint_act;
    } else {
        convert_dds_state2rl_state();
        joint_act = joint_pos;
        init_joint_act = joint_pos;
        _record_yaw = base_rpy[2];//todo
        cout << "Reset done! Rpy: " << base_rpy.transpose() << endl;
    }
    auto o = get_observation();
}


/**
 * @brief 强化学习控制主函数，执行ONNX模型推理并生成关节动作
 * 
 * 此函数在控制循环中周期性调用（默认15ms周期），执行以下步骤：
 * 1. 构建当前观测向量（包含历史帧堆叠）
 * 2. 调用ONNX模型进行推理
 * 3. 将模型输出转换为实际动作增量
 * 4. 应用动作增量更新关节目标位置
 * 
 * 调用流程：
 *   G1::Control() -> rl_control() -> onnxInference.inference()
 */
void RLController::rl_control() {
    counter_rl++;  // 推理计数器，用于统计推理次数
    
    // ========== ONNX模型推理 ==========
    // 1. 获取当前观测向量（包含历史帧堆叠，维度为 43×3=129）
    // 2. 调用ONNX推理器执行前向传播
    // 3. 返回模型原始输出（维度为12，值域为[-1, 1]）
    Matrix<float, Dynamic, 1> net_out;
    net_out = onnxInference.inference(motion_session, get_observation());
    
    // ========== 输出转换 ==========
    // 将模型输出从归一化范围[-1, 1]转换为实际动作增量范围
    // 转换公式：action = (net_out + 1) / 2 * (high - low) + low
    // 不同动作维度有不同的上下限（见config.yaml中的act_inc_high/low）
    action_increment = transform(net_out);
    
    // ========== 应用动作增量 ==========
    // 根据动作增量更新关节目标位置，并限制在关节位置限制范围内
    joint_increment_control(action_increment);
    
    // 更新实际控制周期（用于计算动作增量的时间积分）
    _rl_time_step = get_true_loop_period();
}

void RLController::joint_increment_control(Matrix<float, Dynamic, 1> increment) {
    pm_f = increment.segment(0, NUM_LEGS);
    compute_pm_phase(pm_f);
    joint_act.segment(0, NUM_ACTUAT_JOINTS) += increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS) * _rl_time_step;
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
    // cout << "joint_act: " << joint_act.transpose() << endl;
    // exit(1);
}


/**
 * @brief 构建当前观测向量，用于ONNX模型推理
 * 
 * 观测向量组成（总维度43）：
 * 1. 目标命令 (2维): [vx_cmd, yr_cmd] - 期望的前进速度和转向角速度
 * 2. 基座姿态 (2维): [roll, pitch] - 基座的横滚角和俯仰角（不含yaw）
 * 3. 基座角速度 (3维): [roll_rate, pitch_rate, yaw_rate] * 0.5 - 基座角速度（缩放）
 * 4. 关节位置误差 (10维): joint_pos - ref_joint_act - 相对于参考位置的关节位置偏差
 * 5. 关节速度 (10维): joint_vel * 0.1 - 关节速度（缩放）
 * 6. 关节位置误差 (10维): joint_act - joint_pos - 目标位置与实际位置的误差
 * 7. 相位信息 (4维): [sin(phase_0), cos(phase_0), sin(phase_1), cos(phase_1)] * static_flag
 * 8. 频率信息 (2维): (pm_f * 0.3 - 1) * static_flag - 相位频率（仅在运动时有效）
 * 
 * 观测堆叠：
 * - 使用历史3帧观测进行堆叠，最终输入维度为 43 × 3 = 129
 * - 堆叠有助于模型感知运动趋势和动态特性
 * 
 * @return 堆叠后的观测向量，维度为 (input_dim × stack_dim)
 */
Matrix<float, Dynamic, 1> RLController::get_observation() {
    Matrix<float, Dynamic, 1> obs;  // 当前帧观测向量
    Vec2<float> con_1;
    con_1.setOnes();  // 用于频率信息的常数向量 [1, 1]
    
    // 初始化单帧观测向量，大小为43（从config.yaml读取）
    obs.resize(onnxInference.input_dim);
    obs.setZero();
    
    // ========== 线程安全：加锁保护共享状态 ==========
    pthread_mutex_lock(&_rl_state_mutex);
    
    // 计算关节位置误差（目标位置 - 实际位置）
    joint_pos_error = joint_act - joint_pos;
    
    // 计算相位的sin和cos值（用于编码周期性运动）
    // 相位信息有助于模型理解步态周期
    for (int i(0); i < NUM_LEGS; i++) {
        pm_phase_sin_cos(i) = sin(_pm_phase[i]);              // 左/右腿相位的sin值
        pm_phase_sin_cos(NUM_LEGS + i) = cos(_pm_phase[i]);   // 左/右腿相位的cos值
    }
    
    // 处理摇杆输入，生成目标速度命令
    joystick_command_process();
    
    // 判断机器人是否处于运动状态
    // 如果速度命令的模长小于0.15，认为是静止状态
    if (sqrt(pow(target_command(0), 2) + pow(target_command(1), 2)) < 0.15)
        static_flag = 0.f;  // 静止标志
    else
        static_flag = 1.f;  // 运动标志
    
    // ========== 组装观测向量（按顺序拼接各个部分）==========
    obs << target_command,                                    // [0:2]   目标命令 (vx, yr)
            base_rpy.segment(0, 2),                           // [2:4]   基座姿态 (roll, pitch)
            base_rpy_rate * 0.5,                              // [4:7]   基座角速度（缩放）
            joint_pos.segment(0, NUM_ACTUAT_JOINTS) - _ref_joint_act,  // [7:17]  关节位置相对参考位置的偏差
            joint_vel.segment(0, NUM_ACTUAT_JOINTS) * 0.1f,  // [17:27] 关节速度（缩放）
            joint_pos_error.segment(0, NUM_ACTUAT_JOINTS),    // [27:37] 关节位置误差
            pm_phase_sin_cos * static_flag,                   // [37:41] 相位信息（仅在运动时有效）
            (pm_f * 0.3 - con_1) * static_flag;              // [41:43] 频率信息（仅在运动时有效）
    
    // 限制观测值范围在[-3, 3]之间，防止异常值影响模型推理
    obs = obs.cwiseMax(-3.).cwiseMin(3.);

    pthread_mutex_unlock(&_rl_state_mutex);
    
    // ========== 检查观测向量维度 ==========
    // 确保堆叠后的观测向量维度正确
    if (int(observation.size()) != onnxInference.input_dim * onnxInference.stack_dim) {
        cout << "The dimension of the input size observation is error!!!" << endl;
        cout << "True state size:" << observation.size() << "Policy input size:" << onnxInference.input_dim * onnxInference.stack_dim << endl;
        exit(1);
    }

    // ========== 观测堆叠处理 ==========
    // 将当前观测添加到历史堆叠队列中
    if (_is_first_run) {
        // 首次运行：用当前观测填充整个堆叠缓冲区
        for (int i(0); i < onnxInference.stack_dim; i++) {
            obs_stack.erase(obs_stack.begin());  // 移除最旧的观测
            obs_stack.push_back(obs);            // 添加当前观测
        }
        _is_first_run = false;
        cout << endl << "Reset observation history: Done!" << endl;
    } else {
        // 正常运行时：滑动窗口更新
        obs_stack.erase(obs_stack.begin());  // 移除最旧的观测（FIFO队列）
        obs_stack.push_back(obs);            // 添加最新的观测
    }
    
    // ========== 将堆叠的观测展平为一维向量 ==========
    // 将3帧历史观测按时间顺序拼接成一个长向量
    // 格式: [obs_t-2, obs_t-1, obs_t]，其中obs_t是最新观测
    for (int i(0); i < onnxInference.stack_dim; i++) {
        for (int j(0); j < onnxInference.input_dim; j++) {
            // 将第i帧观测的第j个元素放入展平向量的对应位置
            observation[onnxInference.input_dim * i + j] = obs_stack.at(i)[j];
        }
    }
    
    // 返回堆叠后的观测向量，维度为 43 × 3 = 129
    return observation;
}


void RLController::joystick_command_process() {
    float vx_cmd = 0,  yr_cmd = 0;
    auto yr_max = configParams.yr_cmd_range.at(1);
    auto vx_min = configParams.vx_cmd_range.at(0);
    auto vx_max = configParams.vx_cmd_range.at(1);
    if (task_mode == 3 or task_mode == 4) {
        ///stand
        vx_cmd = -vx_max * jsreader->Axis[1];
        yr_cmd = -yr_max * jsreader->Axis[2];

        if (fabs(yr_cmd) > 0.1 or configParams.kp_yaw_ctrl < 1e-2 or static_flag < 0.1) {
            _record_yaw = base_rpy[2];//todo
        } else {
            yr_cmd = configParams.kp_yaw_ctrl * smallest_signed_angle_between(base_rpy[2], _record_yaw);
        }

        yr_cmd = std::clamp(yr_cmd, -yr_max, yr_max);
        vx_cmd = std::clamp(vx_cmd, vx_min, vx_max);
    }
    target_command << vx_cmd, yr_cmd;
}

void RLController::set_rl_joint_act2dds_motor_command(char mode) {
    MotorCommand motor_command_tmp;
    
    // ========== 力矩保护检查 ==========
    bool torque_protection_triggered = false;
    if (configParams.enable_torque_protection) {
        for (int i = 0; i < NUM_JOINTS; ++i) {
            float abs_tau = std::abs(joint_tau[jointIndex2Sim[i]]);
            if (abs_tau > _torque_limit[jointIndex2Sim[i]]) {
                // 力矩超过阈值，增加持续时间计数
                torque_exceed_duration[jointIndex2Sim[i]] += _rl_time_step;
                
                // 如果持续时间超过阈值，触发保护
                if (torque_exceed_duration[jointIndex2Sim[i]] >= configParams.torque_protection_duration) {
                    torque_protection_triggered = true;
                    _torque_protection_active = true;
                    // 打印警告（限制频率，每秒最多1次）
                    static int warning_counter = 0;
                    static int warning_interval = static_cast<int>(1.0 / _rl_time_step);  // 每秒1次
                    if (warning_counter % warning_interval == 0) {
                        std::cerr << "[WARNING] 力矩保护触发！关节 " << i 
                                  << " 力矩: " << joint_tau[jointIndex2Sim[i]] 
                                  << " N·m，超过限制: " << _torque_limit[jointIndex2Sim[i]] 
                                  << " N·m，自动泄力！" << std::endl;
                    }
                    warning_counter++;
                }
            } else {
                // 力矩正常，重置持续时间计数
                torque_exceed_duration[jointIndex2Sim[i]] = 0.0f;
            }
        }
        
        // 如果所有关节力矩都正常，重置保护状态
        if (torque_exceed_duration.cwiseAbs().maxCoeff() < 1e-6f) {
            _torque_protection_active = false;
        }
    }
    
    // 如果力矩保护被触发，强制切换到泄力模式（类似mode=='x'）
    if (torque_protection_triggered) {
        mode = 'x';
    }
    
    for (int i = 0; i < NUM_JOINTS; ++i) {
        motor_command_tmp.q_target[i] = joint_act[jointIndex2Sim[i]];
        if (mode=='q' || mode=='x') {
            // q: 退出程序, x: 电机泄力（或力矩保护触发）
            motor_command_tmp.kp[i] = 0.;
            motor_command_tmp.kd[i] = 0.;
        } else if (mode=='1') {
            motor_command_tmp.kp[i] = _kp_soft[jointIndex2Sim[i]];
            motor_command_tmp.kd[i] = _kd_soft[jointIndex2Sim[i]];
        }
        else {
            motor_command_tmp.kp[i] = _kp[jointIndex2Sim[i]];
            motor_command_tmp.kd[i] = _kd[jointIndex2Sim[i]];
        }
        motor_command_tmp.tau_ff[i] = 0.;
        
        // 在模式'2'（回零/站立）时，使用计算出的速度前馈
        // 其他模式速度前馈设为0（由位置控制自然产生速度）
        if (mode == '2') {
            motor_command_tmp.dq_target[i] = joint_vel_target[jointIndex2Sim[i]];
        } else {
            motor_command_tmp.dq_target[i] = 0.;
        }
    }
    dds_motor_command->SetData(motor_command_tmp);
}

void RLController::convert_dds_state2rl_state() {
    Vec3<float> trans_axis(-1., 1, -1);
    if (dds_motor_state->GetData()) {
        for (int i = 0; i < NUM_JOINTS; ++i) {
            joint_pos[jointIndex2Sim[i]] = exp_filter(joint_pos[jointIndex2Sim[i]], dds_motor_state->GetData()->q[i], 0.2);
            joint_vel[jointIndex2Sim[i]] = exp_filter(joint_vel[jointIndex2Sim[i]], dds_motor_state->GetData()->dq[i], 0.1);
            joint_tau[jointIndex2Sim[i]] = dds_motor_state->GetData()->tau_est[i];
            joint_acc[jointIndex2Sim[i]] = dds_motor_state->GetData()->ddq[i];
        }
    }
    if (dds_base_state->GetData()) {
        for (int i(0); i < 3; i++) {
            base_rpy(i) = exp_filter(base_rpy(i), fmod(dds_base_state->GetData()->rpy.at(i) * trans_axis(i), 2 * M_PI), 0.2);
            base_rpy_rate(i) = exp_filter(base_rpy_rate(i), dds_base_state->GetData()->omega.at(i) * trans_axis(i), 0.1);
            base_acc(i) = exp_filter(base_acc(i), dds_base_state->GetData()->acc.at(i) * trans_axis(i), 0.1);
        }
    }
    counter_print++;
}


void RLController::compute_pm_phase(Vec2<float> f) {
    for (int leg(0); leg < NUM_LEGS; leg++) {
        _pm_phase[leg] += 2. * M_PI * f[leg] * _rl_time_step;
        _pm_phase[leg] = fmod(_pm_phase[leg], 2 * M_PI);
    }
}

/**
 * @brief 将ONNX模型输出从归一化范围转换为实际动作增量
 * 
 * 模型输出范围：[-1, 1]（tanh激活函数输出）
 * 转换过程：
 * 1. 将[-1, 1]映射到[0, 1]：net = (data + 1) / 2
 * 2. 线性映射到实际范围：action = net * (high - low) + low
 * 
 * 动作维度分组（共12维）：
 * - 维度0-1 (NUM_LEGS=2): 相位频率增量，使用act_inc_high/low[0]
 * - 维度2-11 (NUM_ACTUAT_JOINTS=10): 关节位置增量，使用act_inc_high/low[1]
 * - 维度12+: 其他动作（如果有），使用act_inc_high/low[2]
 * 
 * 配置参数（来自config.yaml）：
 *   act_inc_high: [3.5, 15.0]   - 各动作维度的上限
 *   act_inc_low:  [0.5, -15.0]  - 各动作维度的下限
 * 
 * @param data ONNX模型原始输出，维度为12，值域为[-1, 1]
 * @return 转换后的动作增量，维度为12，值域为实际动作范围
 */
Matrix<float, Dynamic, -1> RLController::transform(Matrix<float, Dynamic, -1> data) {
    // 步骤1：将模型输出从[-1, 1]映射到[0, 1]
    // 这样便于后续的线性缩放
    auto net = (data.array() + 1.) / 2.;
    
    int ii = 0;  // 用于索引配置数组中的上下限
    
    // 步骤2：对每个输出维度进行线性映射
    for (int i(0); i < onnxInference.output_dim; i++) {
        // 根据动作维度索引确定使用哪组上下限
        if (i < NUM_LEGS)
            // 前2维：相位频率增量（用于控制步态频率）
            ii = 0;
        else if (i < NUM_LEGS + NUM_ACTUAT_JOINTS + 1)
            // 中间10维：关节位置增量（用于控制关节运动）
            ii = 1;
        else
            // 其他维度（如果有）
            ii = 2;
        
        // 线性映射：将[0, 1]映射到[act_inc_low[ii], act_inc_high[ii]]
        // 公式：output = input * (high - low) + low
        action_increment(i) = net(i) * (configParams.act_inc_high[ii] - configParams.act_inc_low[ii]) + configParams.act_inc_low[ii];
    }
    return action_increment;
}


// S曲线插值函数：使用5次多项式实现平滑的加速和减速
float RLController::smooth_interpolation(float t) {
    // 将t限制在[0,1]范围内
    t = std::max(0.0f, std::min(1.0f, t));
    // 5次多项式：6*t^5 - 15*t^4 + 10*t^3
    // 这个曲线在t=0和t=1时速度、加速度都为0，运动更平滑
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

void RLController::smooth_joint_action(float ratio, const Vec10<float> &end_joint_act, float dt, float max_vel) {
    // 计算总移动距离
    Vec10<float> total_delta = end_joint_act - init_joint_act;
    
    // 计算当前位置到目标位置的误差
    Vec10<float> current_error = end_joint_act - joint_act;
    
    // 安全检查：如果当前位置误差过大，使用更保守的速度
    // 避免因为惯性导致无法及时到达目标位置而触发堵转保护
    float max_error = current_error.cwiseAbs().maxCoeff();
    if (max_error > 0.5f) {
        // 如果位置误差超过0.5弧度，进一步降低速度
        max_vel = max_vel * 0.7f;  // 降低30%的速度
    }
    
    // 使用S曲线插值代替线性插值，使加速度更平滑
    float smooth_ratio = smooth_interpolation(ratio);
    
    // 计算目标位置（基于S曲线）
    Vec10<float> target_act = init_joint_act + total_delta * smooth_ratio;
    
    // 计算当前位置到目标位置的剩余距离
    Vec10<float> remaining_delta = target_act - joint_act;
    
    // 计算S曲线在当前ratio点的速度因子（导数值）
    // S曲线 f(t) = 6*t^5 - 15*t^4 + 10*t^3
    // 导数 f'(t) = 30*t^4 - 60*t^3 + 30*t^2 = 30*t^2*(t^2 - 2*t + 1) = 30*t^2*(1-t)^2
    float vel_factor = 0.0f;
    if (ratio > 0.0f && ratio < 1.0f && dt > 1e-6f) {
        float t = std::max(0.0f, std::min(1.0f, ratio));
        float one_minus_t = 1.0f - t;
        vel_factor = 30.0f * t * t * one_minus_t * one_minus_t;  // 30*t^2*(1-t)^2
    }
    
    // 计算期望速度（基于S曲线的速度因子）
    Vec10<float> desired_vel = total_delta * vel_factor;
    
    // 限制每个关节的速度不超过max_vel (rad/s)
    for (int i = 0; i < NUM_JOINTS; ++i) {
        float vel_magnitude = std::abs(desired_vel[i]);
        if (vel_magnitude > max_vel) {
            // 如果速度超过限制，按比例缩小
            desired_vel[i] = desired_vel[i] / vel_magnitude * max_vel;
        }
    }
    
    // 计算速度限制后的位置增量
    Vec10<float> vel_limited_delta = desired_vel * dt;
    
    // 确保位置增量不会超过剩余距离（避免超调）
    // 同时考虑实际位置误差，避免目标位置和实际位置差距过大
    for (int i = 0; i < NUM_JOINTS; ++i) {
        float remaining = remaining_delta[i];
        float error = current_error[i];
        
        // 如果误差较大，进一步限制增量，避免累积误差
        if (std::abs(error) > 0.3f) {
            // 误差大时，限制单步增量不超过误差的20%
            float max_step = error * 0.2f;
            if (std::abs(vel_limited_delta[i]) > std::abs(max_step)) {
                vel_limited_delta[i] = max_step;
                desired_vel[i] = max_step / dt;
            }
        }
        
        if (std::abs(vel_limited_delta[i]) > std::abs(remaining)) {
            // 如果剩余距离很小，直接到达目标
            vel_limited_delta[i] = remaining;
            desired_vel[i] = remaining / dt;
        }
    }
    
    // 更新关节位置
    joint_act = joint_act + vel_limited_delta;
    
    // 限制关节位置在允许范围内
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
    
    // 保存目标速度，用于设置电机命令的速度前馈
    joint_vel_target = desired_vel;
}

float RLController::exp_filter(float history, float present, float weight) {
    auto result = history * weight + present * (1. - weight);
    return result;
}


void RLController::sin_control(float amplitude, float f, float motion_time) {
    Vec10<float> sin_joint_act;
    sin_joint_act.setConstant(amplitude * sin(2.f * M_PI * f * motion_time));
    if (configParams.sin_joint_idx == -1)
        joint_act.segment(0, NUM_ACTUAT_JOINTS) = init_joint_act.segment(0, NUM_ACTUAT_JOINTS) + sin_joint_act;
    else
        joint_act[configParams.sin_joint_idx] = init_joint_act[configParams.sin_joint_idx] + sin_joint_act[configParams.sin_joint_idx];
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
}


float RLController::get_true_loop_period() {
    static struct timeval last_time;
    static struct timeval now_time;
    static bool first_get_time = true;
    if (first_get_time) {
        gettimeofday(&last_time, nullptr);
        first_get_time = false;
    }
    gettimeofday(&now_time, nullptr);
    auto d_time = (float) (now_time.tv_sec - last_time.tv_sec) +
                  (float) (now_time.tv_usec - last_time.tv_usec) / 1000000;
    last_time = now_time;
    if (fabs(d_time - _rl_time_step) * 1000. > 2.)
        cout << "True period: " << d_time * 1000. << " ms" << endl;
    return d_time;
}

void RLController::stand_control(float ratio) {
    // 计算从初始位置到目标位置的最大距离
    Vec10<float> total_delta = _ref_joint_act - init_joint_act;
    float max_delta = total_delta.cwiseAbs().maxCoeff();
    
    // 根据最大移动距离动态调整最大速度，更保守的设置以避免堵转误判
    // 降低速度限制，让运动更平滑，减少惯性冲击
    float max_vel = 0.15f;  // 默认最大速度降低到 0.15 rad/s (约8.6度/秒)
    if (max_delta > 0.5f) {
        max_vel = 0.1f;   // 大距离时更保守，避免瞬间扭矩过大 (约5.7度/秒)
    } else if (max_delta > 0.2f) {
        max_vel = 0.12f;  // 中等距离 (约6.9度/秒)
    } else if (max_delta < 0.1f) {
        max_vel = 0.18f;  // 小距离时可以稍快，但仍保守 (约10.3度/秒)
    }
    
    // 调用改进的平滑关节动作函数，传入控制周期和速度限制
    smooth_joint_action(ratio, _ref_joint_act, _rl_time_step, max_vel);
}

void RLController::sim_gait_control() {
    static int data_index = 0;
    if (data_index <= sim_gait_data.size() - 2)
        data_index++;
    for (int i(0); i < NUM_ACTUAT_JOINTS; i++) {
        joint_act(i) = sim_gait_data.at(data_index).at(i);
    }
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
}

Vec3<float> RLController::convert_world_frame_to_base_frame(const Vec3<float> &world_vec, const Vec3<float> &rpy) {
    return ori::rpy_to_rotMat(rpy) * world_vec;
}

Vec3<float> RLController::quat_rotate_inverse(Vec4<float> q, Vec3<float> v) {
    Vec3<float> a, b, c;
    // q={w,x,y,z}
    // q << 0.99981624, -0.013256095, 0.012793032, -0.005295367; //todo: in isaac gym: x,y,z,w ;here IMU(q): w,x,y,z !!
    // v << -0.13947208, -0.08728597, 0.19939381;
    // result: lin_v(-0.14353749,-0.09399233,0.19336908)
    float q_w = q[0];
    Vec3<float> q_vec(q[1], q[2], q[3]);
    a = v * (2.0 * q_w * q_w - 1.0);
    b = q_vec.cross(v) * q_w * 2.0;
    c = q_vec * q_vec.transpose() * v * 2.0;
    // cout << "quat_rotate_inverse: " << (a - b + c).transpose() << endl << endl;
    return a - b + c;
}

/*!
 * Take the product of two quaternions
 */
Vec4<float> RLController::quat_product(Vec4<float> &q1, Vec4<float> &q2) {
    float r1 = q1[0];
    float r2 = q2[0];

    Vec3<float> v1(q1[1], q1[2], q1[3]);
    Vec3<float> v2(q2[1], q2[2], q2[3]);

    float r = r1 * r2 - v1.dot(v2);
    Vec3<float> v = r1 * v2 + r2 * v1 + v1.cross(v2);
    Vec4<float> q(r, v[0], v[1], v[2]);
    return q;
}

float RLController::smallest_signed_angle_between(float alpha, float beta) {
    auto a = beta - alpha;
    a += (a > M_PI) ? -2 * M_PI : (a < -M_PI) ? 2 * M_PI : 0.;
    return a;
}

Vec4<float> RLController::rpy_to_quat(const Vec3<float> &rpy) {
    auto R = ori::rpy_to_rotMat(rpy);
    auto q = ori::rotMat_to_quat(R);
    return q;
}

Vec4<float> RLController::quat_mul(Vec4<float> a, Vec4<float> b) {
    float x1 = a[1];
    float y1 = a[2];
    float z1 = a[3];
    float w1 = a[0];

    float x2 = b[1];
    float y2 = b[2];
    float z2 = b[3];
    float w2 = b[0];

    float ww = (z1 + x1) * (x2 + y2);
    float yy = (w1 - y1) * (w2 + z2);
    float zz = (w1 + y1) * (w2 - z2);
    float xx = ww + yy + zz;
    float qq = 0.5 * (xx + (z1 - x1) * (x2 - y2));

    float w = qq - ww + (z1 - y1) * (y2 - z2);
    float x = qq - xx + (x1 + w1) * (x2 + w2);
    float y = qq - yy + (w1 - x1) * (y2 + z2);
    float z = qq - zz + (z1 + y1) * (w2 - x2);

    Vec4<float> quat(w, x, y, z);

    return quat;
}