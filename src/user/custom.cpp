//
// Created by cyy on 24-10-7.
//

#include "user/custom.hpp"
#include <iomanip>


void G1::ModeProcess() {
    if (_is_test_local) {
        selected_mode = modeSwitcher.get_selected_key(current_mode);
        ModeSwitcher::print_selected_mode(selected_mode);
    } else {
//        selected_mode = modeSwitcher.get_selected_stick(current_mode);
        selected_mode = modeSwitcher.get_selected_jskey(current_mode);
    }
    if (selected_mode != current_mode) {
        ModeSwitcher::print_selected_mode(selected_mode);
        /// mode transition
        relative_time = 0.;
        current_mode = selected_mode;
        rlController->reset(_is_test_local);
    }
    rlController->task_mode = modeSwitcher.rl_task_mode;
}

void G1::Control() {
    relative_time += control_dt_;
    float ratio = fmin(relative_time / MOVE_DURATION, 1.f);
    rlController->convert_dds_state2rl_state();
    switch (current_mode) {
        case 'q':
            rlController->_kp.setZero();
            rlController->_kd.setZero();
            dataReporter.close();
            usleep(1e3);
            exit(1);
        case 'x':
            ///X键 电机泄力模式
            // 不修改 _kp 和 _kd 的值，只在 set_rl_joint_act2dds_motor_command 中设置电机命令的 kp 和 kd 为 0
            // 保持当前关节位置，但将kp和kd设为0，实现泄力
            break;
        case 'r':
            ///R键 电机软件复位（清除故障码）
            // 通过模式切换（BRAKE -> FOC）来清除故障码，无需断电重启
            // 复位期间保持泄力模式（kp=kd=0），复位完成后需要手动切换到其他模式
            // 注意：复位完成后不会自动切换，用户可以手动切换到其他模式（如'2'stand模式）
            if (!Motor_control.IsResetting()) {
                Motor_control.SoftwareReset();
                std::cout << "\033[36m[Motor Reset] Reset initiated. After completion, press '2' for stand mode\033[0m" << std::endl;
            }
            break;
        case '2':
            ///to stand
            // 从配置恢复 KP 和 Kd 的值（如果之前被修改了）
            for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
                rlController->_kp[i] = rlController->configParams.kp.at(i);
                rlController->_kd[i] = rlController->configParams.kd.at(i);
            }
            rlController->stand_control(ratio);
            break;
        case '3':
            ///rl
            rlController->rl_control();
            if (rlController->counter_rl < 2)
                rlController->stand_control(ratio);
            break;
        case '5':
            // sin test (select)
            if (rlController->configParams.use_sim_gait)
                rlController->sim_gait_control();
            else
                rlController->sin_control(0.2, 2., relative_time);
            break;
        default: /// case '1'
            rlController->stand_control(ratio);
            break;
    }
    rlController->set_rl_joint_act2dds_motor_command(current_mode);
    control_count++;
    // 控制周期为control_dt秒，每秒1次 = 1.0 / control_dt 次
    // 例如：control_dt=0.015秒，则每秒约67次，即每67次打印一次
    int print_interval = static_cast<int>(1.0 / control_dt_);  // 每秒1次
    if (control_count % print_interval == 0) {
        control_count = 0;
        // 格式化输出当前关节位置，保留两位小数
        cout << "Q: [ ";
        for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
            cout << std::fixed << std::setprecision(2) << rlController->joint_pos[i];
            if (i < rlController->NUM_JOINTS - 1) {
                cout << ", ";
            }
        }
        cout << " ]" << endl;
        
        // 格式化输出下一帧目标关节位置，保留两位小数
        cout << "N: [ ";
        for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
            cout << std::fixed << std::setprecision(2) << rlController->joint_act[i];
            if (i < rlController->NUM_JOINTS - 1) {
                cout << ", ";
            }
        }
        cout << " ]" << endl;
        
        // 格式化输出实时输出扭矩，保留两位小数
        cout << "F: [ ";
        for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
            cout << std::fixed << std::setprecision(2) << rlController->joint_tau[i];
            if (i < rlController->NUM_JOINTS - 1) {
                cout << ", ";
            }
        }
        cout << " ]" << endl;
        
        // 检查并输出错误码（仅在有关节报错时输出）
        const std::array<MotorData, 10> &motor_data = Motor_control.GetData();
        bool has_error = false;
        for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
            // 验证错误码在合理范围内（0-7），过滤异常值
            int merror_value = motor_data[i].merror;
            if (merror_value > 0 && merror_value <= 7) {
                has_error = true;
                break;
            }
        }
        if (has_error) {
            cout << "E: [ ";
            for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
                int merror_value = motor_data[i].merror;
                // 如果错误码超出范围，显示为0（表示无法解析或数据异常）
                if (merror_value < 0 || merror_value > 7) {
                    cout << 0;
                } else {
                    cout << merror_value;
                }
                if (i < rlController->NUM_JOINTS - 1) {
                    cout << ", ";
                }
            }
            cout << " ]" << endl;
        }
        
        cout << endl;  // 输出后添加空行
//        cout << "rpy: " << rlController->base_rpy.transpose() << endl;
    }
}

void G1::ReportData() {
    if (current_mode >= '1' and current_mode != 'q') {
         if (rlController->counter_rl >= 2) {
            dataReporter.report_data(rlController);
         }
    }
}

void G1::RunJoystick() {
    xRockerGamepad.Step();
}

void G1::RecordMotorState(const std::array<MotorData, 10> &data) {
    MotorState ms_tmp;
    for (int i = 0; i < 10; ++i) {
        ms_tmp.q.at(i) = data[i].q;
        ms_tmp.dq.at(i) = data[i].dq;
        ms_tmp.ddq.at(i) = 0.;
        ms_tmp.tau_est.at(i) = data[i].tau;  // 修复：从电机数据读取真实的力矩值，而不是设为0
    }
    motor_state_buffer_.SetData(ms_tmp);
//    std::cout << "q: " << ms_tmp.q.at(0)<<endl;
}

unitree_hg::msg::dds_::LowCmd_ G1::SetMotorCmd() {
    unitree_hg::msg::dds_::LowCmd_ dds_low_command;
    //    std::cout << "qd0: " << dds_low_command.motor_cmd().at(0).q() << endl;

    const std::shared_ptr<const MotorCommand> mc = motor_command_buffer_.GetData();

    for (size_t i = 0; i < rlController->NUM_JOINTS; i++) {
        dds_low_command.motor_cmd().at(i).tau() = mc->tau_ff.at(i);
        dds_low_command.motor_cmd().at(i).q() = mc->q_target.at(i);
        dds_low_command.motor_cmd().at(i).dq() = mc->dq_target.at(i);
        dds_low_command.motor_cmd().at(i).kp() = mc->kp.at(i) * 1.;
        dds_low_command.motor_cmd().at(i).kd() = mc->kd.at(i) * 1.;
    }
    return dds_low_command;
}

void G1::RecordBaseState() {
    BaseState bs_tmp;
    bs_tmp.omega = {imuReader.RollSpeed, imuReader.PitchSpeed, imuReader.HeadingSpeed};
    bs_tmp.rpy = {imuReader.Roll, imuReader.Pitch, imuReader.Heading};
    bs_tmp.acc = {imuReader.Accelerometer_X, imuReader.Accelerometer_Y, imuReader.Accelerometer_Z};
    bs_tmp.quat = {imuReader.qw, imuReader.qx, imuReader.qy, imuReader.qz};
    base_state_buffer_.SetData(bs_tmp);

}

void G1::JointStateReadWriter() {
    Motor_control.Run(SetMotorCmd());
    RecordMotorState(Motor_control.GetData());
 //   std::cout<<"被触发"<<std::endl;
//    ///IMU
//    if (imuReader.fetchIMUData()) {
//        RecordBaseState();
//    } else {
//        std::cerr << "Failed to fetch IMU data" << std::endl;
//    }
}

void G1::IMUStateReader() {
    if (imuReader.fetchIMUData()) {
        RecordBaseState();
    } else {
        std::cerr << "Failed to fetch IMU data" << std::endl;
    }
}

/**
 * @brief 打印当前关节位置（YAML格式），用于设置零位
 * 
 * 使用方法：
 * 1. 将机器人调整到想要的零位姿态
 * 2. 在代码中调用此函数，或从终端触发
 * 3. 复制输出的YAML格式内容到config.yaml的ref_joint_act字段
 */
void G1::PrintCurrentJointPositionAsZero() {
    rlController->convert_dds_state2rl_state();  // 确保获取最新关节位置
    cout << "\n========== 当前关节位置（零位设置） ==========" << endl;
    cout << "# 将以下内容复制到 config.yaml 的 ref_joint_act 字段：" << endl;
    cout << "ref_joint_act: [ ";
    for (int i = 0; i < rlController->NUM_JOINTS; ++i) {
        cout << std::fixed << std::setprecision(3) << rlController->joint_pos[i];
        if (i < rlController->NUM_JOINTS - 1) {
            cout << ", ";
        }
    }
    cout << " ]" << endl;
    cout << "===============================================\n" << endl;
}