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

};

#endif
