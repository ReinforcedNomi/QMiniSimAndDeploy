//
// Created by cyy on 2022/4/7.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <yaml-cpp/yaml.h>
#include <yaml-cpp/node/parse.h>
#include <string>


class ConfigParams {
public:
    ConfigParams() {
        YAML::Node params = YAML::LoadFile("config.yaml");

        sin_joint_idx = params["sin_joint_idx"].as<int>();
        use_sim_gait = params["use_sim_gait"].as<bool>();
        control_dt = params["control_dt"].as<float>();
        kp_yaw_ctrl=params["kp_yaw_ctrl"].as<float>();


        num_actions = params["num_actions"].as<int>();
        num_observations = params["num_observations"].as<int>();
        num_stacks = params["num_stacks"].as<int>();


        kp = params["kp"].as < std::vector < float > > ();
        kd = params["kd"].as < std::vector < float > > ();

        kp_soft = params["kp_soft"].as < std::vector < float > > ();
        kd_soft = params["kd_soft"].as < std::vector < float > > ();

        vx_cmd_range = params["vx_cmd_range"].as < std::vector < float > > ();
        yr_cmd_range = params["yr_cmd_range"].as < std::vector < float > > ();

        act_inc_high = params["act_inc_high"].as < std::vector < float > > ();
        act_inc_low = params["act_inc_low"].as < std::vector < float > > ();

        act_pos_high = params["act_pos_high"].as < std::vector < float > > ();
        act_pos_low = params["act_pos_low"].as < std::vector < float > > ();

        ref_joint_act = params["ref_joint_act"].as < std::vector < float > > ();
        
        // ONNX模型路径，如果配置文件中没有则使用默认值
        if (params["onnx_model_path"]) {
            onnx_model_path = params["onnx_model_path"].as<std::string>();
        } else {
            onnx_model_path = "policy.onnx";  // 默认路径
        }
        
        // 力矩保护参数
        if (params["enable_torque_protection"]) {
            enable_torque_protection = params["enable_torque_protection"].as<bool>();
        } else {
            enable_torque_protection = false;  // 默认关闭
        }
        
        if (params["torque_limit"]) {
            torque_limit = params["torque_limit"].as < std::vector < float > > ();
        } else {
            torque_limit = std::vector<float>(10, 12.0f);  // 默认12.0 N·m
        }
        
        if (params["torque_protection_duration"]) {
            torque_protection_duration = params["torque_protection_duration"].as<float>();
        } else {
            torque_protection_duration = 0.5f;  // 默认0.5秒
        }
        
        // 日志打印频率（Hz）
        if (params["log_print_frequency"]) {
            log_print_frequency = params["log_print_frequency"].as<float>();
        } else {
            log_print_frequency = 5.0f;  // 默认5Hz
        }
        
        // 步态频率参数
        if (params["initial_gait_frequency"]) {
            initial_gait_frequency = params["initial_gait_frequency"].as<float>();
        } else {
            initial_gait_frequency = 0.3f;  // 默认0.3Hz
        }
        
        if (params["gait_frequency_max"]) {
            gait_frequency_max = params["gait_frequency_max"].as<float>();
        } else {
            gait_frequency_max = 2.0f;  // 默认2.0Hz
        }
        
        if (params["gait_frequency_min"]) {
            gait_frequency_min = params["gait_frequency_min"].as<float>();
        } else {
            gait_frequency_min = 0.2f;  // 默认0.2Hz
        }
        
        // 步态幅度参数（关节增量范围）
        if (params["joint_increment_max"]) {
            joint_increment_max = params["joint_increment_max"].as<float>();
        } else {
            joint_increment_max = 10.0f;  // 默认10.0 rad/s
        }
        
        if (params["joint_increment_min"]) {
            joint_increment_min = params["joint_increment_min"].as<float>();
        } else {
            joint_increment_min = -10.0f;  // 默认-10.0 rad/s
        }
        
        // 基于ref_joint_act的偏差控制参数
        if (params["enable_ref_bias_control"]) {
            enable_ref_bias_control = params["enable_ref_bias_control"].as<bool>();
        } else {
            enable_ref_bias_control = true;  // 默认启用
        }
        
        if (params["ref_bias_weight"]) {
            ref_bias_weight = params["ref_bias_weight"].as<float>();
        } else {
            ref_bias_weight = 0.1f;  // 默认0.1（轻微回归）
        }
        
        // 关节位置偏移量
        if (params["joint_offset"]) {
            joint_offset = params["joint_offset"].as < std::vector < float > > ();
        } else {
            joint_offset = std::vector<float>(10, 0.0f);  // 默认全为0
        }
        // 确保有10个关节的偏移量
        if (joint_offset.size() < 10) {
            joint_offset.resize(10, 0.0f);
        }
        
        // 用新的步态参数覆盖act_inc_high和act_inc_low
        // act_inc_high[0] = 频率上限, act_inc_high[1] = 关节增量上限
        // act_inc_low[0] = 频率下限, act_inc_low[1] = 关节增量下限
        if (act_inc_high.size() >= 2) {
            act_inc_high[0] = gait_frequency_max;
            act_inc_high[1] = joint_increment_max;
        }
        if (act_inc_low.size() >= 2) {
            act_inc_low[0] = gait_frequency_min;
            act_inc_low[1] = joint_increment_min;
        }
    }

public:
    int num_stacks = 0;
    int num_actions = 0;
    int num_observations = 0;
    float control_dt = 0.01;
    float kp_yaw_ctrl = true;


    int sin_joint_idx = -1;
    bool use_sim_gait = false;

    std::vector<float> kp = {0.};
    std::vector<float> kd = {0.};

    std::vector<float> kp_soft = {0.};
    std::vector<float> kd_soft = {0.};

    std::vector<float> vx_cmd_range = {0.};
    std::vector<float> yr_cmd_range = {0.};

    std::vector<float> act_inc_high = {0.};
    std::vector<float> act_inc_low = {0.};

    std::vector<float> act_pos_high = {0.};
    std::vector<float> act_pos_low = {0.};

    std::vector<float> ref_joint_act = {0.};
    
    std::string onnx_model_path = "policy.onnx";  // ONNX模型文件路径
    
    // 力矩保护参数
    bool enable_torque_protection = false;  // 是否启用力矩保护
    std::vector<float> torque_limit = {12.0f};  // 力矩上限 (N·m)
    float torque_protection_duration = 0.5f;  // 力矩超过阈值后持续多长时间才触发保护 (秒)
    
    // 日志打印频率（Hz）
    float log_print_frequency = 5.0f;  // 日志输出频率，默认5Hz
    
    // 步态频率参数
    float initial_gait_frequency = 0.3f;  // 初始步态频率，默认0.3Hz
    float gait_frequency_max = 2.0f;      // 最大步态频率上限，默认2.0Hz
    float gait_frequency_min = 0.2f;      // 最小步态频率下限，默认0.2Hz
    
    // 步态幅度参数（关节增量范围）
    float joint_increment_max = 10.0f;    // 关节增量最大值，默认10.0 rad/s
    float joint_increment_min = -10.0f;   // 关节增量最小值，默认-10.0 rad/s
    
    // 基于ref_joint_act的偏差控制参数
    bool enable_ref_bias_control = true;  // 是否启用基于ref_joint_act的偏差控制
    float ref_bias_weight = 0.1f;         // 回归权重，范围[0,1]，默认0.1（轻微回归到ref_joint_act）
    
    // 关节位置偏移量
    std::vector<float> joint_offset = {0.0f};  // 各关节的位置偏移量（rad）

};

#endif
