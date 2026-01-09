//
// 自定义RL控制器 - 独立于原始RLController
// 观测：195维（39维×5帧历史），动作：10维
//

#ifndef CUSTOM_RL_CONTROLLER_H
#define CUSTOM_RL_CONTROLLER_H

#include "onnx/onnxruntime_cxx_api.h"
#include "onnx_inference.h"
#include "utils/cpp_types.h"
#include "unitree/g1/motors.hpp"
#include "unitree/g1/data_buffer.hpp"
#include "unitree/g1/base_state.hpp"
#include "joystick_reader.h"
#include <vector>
#include <eigen3/Eigen/Dense>

using namespace Eigen;

class CustomRLController {
public:
    explicit CustomRLController() = default;

public:
    static const int NUM_JOINTS = 10;
    static const int OBS_DIM = 39;        // 单帧观测维度
    static const int OBS_HISTORY = 5;      // 历史帧数
    static const int TOTAL_OBS_DIM = 195;  // 总观测维度：39×5
    static const int ACTION_DIM = 10;      // 动作维度

    Vec10<float> joint_pos, joint_vel, joint_act;
    Vec3<float> base_rpy, base_rpy_rate;
    Vec4<float> base_quat;
    Vec10<float> last_action;  // 上一步动作

    DataBuffer<MotorCommand> *dds_motor_command = nullptr;
    DataBuffer<MotorState> *dds_motor_state = nullptr;
    DataBuffer<BaseState> *dds_base_state = nullptr;
    JoystickReader *jsreader = nullptr;

    Vec10<float> _kp, _kd;
    Vec10<float> act_pos_low, act_pos_high;
    Vec10<float> _ref_joint_act;

    int counter_rl = 0;
    float _rl_time_step = 0.01f;

private:
    OnnxInference onnxInference;
    Ort::Session *motion_session = nullptr;
    Ort::Env *env = nullptr;
    
    Matrix<float, Dynamic, 1> observation;  // 195维观测
    std::vector<Matrix<float, Dynamic, 1>> obs_history;  // 5帧历史观测
    bool _is_first_run = true;

    Vec3<float> velocity_command;  // 速度命令（来自摇杆）

    // 构建单帧观测（39维）
    Matrix<float, Dynamic, 1> build_single_frame_obs();

public:
    void init(const std::string& model_path = "custom_policy.onnx");
    void reset();
    void custom_rl_control();
    void convert_dds_state2rl_state();
    void set_joint_act2dds_motor_command();
    void process_joystick_input();  // 处理摇杆输入（左摇杆X/Y，右摇杆Z）
    
    // 辅助函数：四元数旋转逆变换
    Vec3<float> quat_rotate_inverse(Vec4<float> q, Vec3<float> v);

    virtual ~CustomRLController() {
        delete motion_session;
        if (env) {
            delete env;
        }
    }
};

#endif // CUSTOM_RL_CONTROLLER_H

