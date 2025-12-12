# Sim2Real Gap 分析与降低方法

## 概述

Sim2Real（仿真到现实）gap 是指训练好的强化学习模型在仿真环境中表现良好，但在真实机器人上性能下降的现象。本文档分析当前代码中可能存在的 gap 来源，并提供降低方法。

---

## 一、Sim2Real Gap 的主要来源

### 1.1 动力学差异 (Dynamics Gap)

#### 来源
- **仿真中的理想化假设**：
  - 完美的电机响应（无延迟、无摩擦）
  - 理想的质量分布和惯性
  - 无关节柔性和背隙
  - 理想的传感器无噪声

#### 代码中的体现
```cpp
// 当前代码直接使用传感器读数，可能存在：
joint_pos = exp_filter(joint_pos, dds_motor_state->GetData()->q[i], 0.2);
joint_vel = exp_filter(joint_vel, dds_motor_state->GetData()->dq[i], 0.1);
```
- 滤波权重是硬编码的（0.2, 0.1）
- 没有考虑传感器的系统误差和漂移

### 1.2 观测空间差异 (Observation Gap)

#### 来源
- **传感器噪声**：
  - 关节编码器噪声
  - IMU噪声和漂移
  - 数据采集频率差异

#### 代码中的体现
```cpp
// 观测向量中的缩放因子
base_rpy_rate * 0.5,                    // 角速度缩放
joint_vel * 0.1f,                       // 关节速度缩放
joint_pos - _ref_joint_act,             // 相对位置（可能受零位误差影响）
obs = obs.cwiseMax(-3.).cwiseMin(3.);   // 硬裁剪
```

**问题**：
- 缩放因子固定，可能不匹配真实传感器的量程
- 硬裁剪可能掩盖真实的异常值
- 零位（`ref_joint_act`）的准确性直接影响观测质量

### 1.3 动作空间差异 (Action Gap)

#### 来源
- **执行器限制**：
  - 电机响应延迟（仿真中通常忽略）
  - 速度和加速度限制
  - 力矩饱和
  - 控制频率差异

#### 代码中的体现
```cpp
// 动作增量通过时间积分得到位置
joint_act += increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS) * _rl_time_step;

// 然后限位
joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
```

**问题**：
- 假设理想积分，但真实电机有延迟
- 限位范围可能与仿真中不同
- 动作转换中的缩放可能不匹配

### 1.4 延迟和同步问题 (Latency Gap)

#### 来源
- **通信延迟**：
  - DDS通信延迟
  - 传感器数据采集到使用的延迟
  - 计算延迟（ONNX推理时间）

#### 代码中的体现
```cpp
// 控制周期固定为15ms
control_dt: 0.015  // config.yaml

// 但真实周期可能波动
float get_true_loop_period();  // 虽然有测量，但未用于补偿
```

**问题**：
- 固定的 `_rl_time_step = 0.01f` 可能与真实周期不匹配
- 观测堆叠假设固定时间间隔，但真实时间间隔可能波动

### 1.5 标定和坐标系差异 (Calibration Gap)

#### 来源
- **坐标系统一**：
  - 关节零位标定误差
  - IMU安装角度误差
  - 坐标系定义差异（仿真vs现实）

#### 代码中的体现
```cpp
// 坐标转换
Vec3<float> trans_axis(-1., 1, -1);  // 硬编码的转换轴
base_rpy(i) = fmod(dds_base_state->GetData()->rpy.at(i) * trans_axis(i), 2 * M_PI);
```

**问题**：
- 转换轴硬编码，如果硬件改变需要修改代码
- 零位 (`ref_joint_act`) 需要手动标定，可能有误差

---

## 二、当前代码中已有的缓解措施

### 2.1 指数滤波（已实现）

```cpp
// 位置滤波：权重0.2（保留80%历史，20%新值）
joint_pos = exp_filter(joint_pos, sensor_value, 0.2);

// 速度滤波：权重0.1（保留90%历史，10%新值）
joint_vel = exp_filter(joint_vel, sensor_value, 0.1);
```

**作用**：平滑传感器噪声

**局限性**：
- 滤波权重固定，无法适应不同噪声水平
- 可能引入相位延迟

### 2.2 观测值裁剪（已实现）

```cpp
obs = obs.cwiseMax(-3.).cwiseMin(3.);  // 限制在[-3, 3]
```

**作用**：防止异常值影响模型推理

**局限性**：
- 硬裁剪可能丢失重要信息
- 阈值固定，可能不适合所有场景

### 2.3 动作平滑（已实现）

```cpp
// S曲线插值
smooth_joint_action(ratio, _ref_joint_act, _rl_time_step, max_vel);
```

**作用**：平滑过渡，减少冲击

**局限性**：
- 仅在stand模式使用
- RL模式直接积分，可能不够平滑

---

## 三、降低 Sim2Real Gap 的改进方案

### 3.1 观测空间改进

#### 3.1.1 自适应滤波权重

**当前问题**：
```cpp
// 硬编码的滤波权重
exp_filter(joint_pos, sensor_value, 0.2);  // 固定0.2
```

**改进方案**：
```cpp
// 根据传感器噪声水平自适应调整
float adaptive_filter_weight(float noise_level, float base_weight = 0.2) {
    // 噪声大时，更依赖历史值（权重接近1）
    // 噪声小时，更信任新值（权重接近0）
    return std::min(0.9f, base_weight + noise_level * 0.5f);
}

// 使用时
float noise = estimate_sensor_noise();  // 估计噪声水平
float weight = adaptive_filter_weight(noise);
joint_pos = exp_filter(joint_pos, sensor_value, weight);
```

#### 3.1.2 传感器标定和补偿

**建议**：
```cpp
// 添加零位和比例因子标定
struct SensorCalibration {
    Vec10<float> joint_offset;      // 关节零位偏移
    Vec10<float> joint_scale;       // 关节比例因子
    Vec3<float> imu_rpy_offset;     // IMU角度偏移
    Vec3<float> imu_acc_offset;     // IMU加速度偏移
};

// 在观测构建时应用标定
joint_pos_calibrated = (joint_pos - calibration.joint_offset) * calibration.joint_scale;
```

#### 3.1.3 观测归一化改进

**当前问题**：
```cpp
// 硬裁剪
obs = obs.cwiseMax(-3.).cwiseMin(3.);
```

**改进方案**：
```cpp
// 使用软裁剪（tanh缩放）或自适应阈值
void adaptive_clip_observation(Matrix<float, Dynamic, 1>& obs) {
    // 方法1：软裁剪（更平滑）
    obs = obs.array().tanh() * 3.0f;
    
    // 方法2：自适应阈值（基于历史统计）
    static Vec43<float> obs_mean, obs_std;
    update_obs_statistics(obs, obs_mean, obs_std);  // 在线更新统计量
    obs = (obs - obs_mean).cwiseQuotient(obs_std + 1e-6f);  // 标准化
    obs = obs.cwiseMax(-3.).cwiseMin(3.);  // 然后裁剪
}
```

### 3.2 动作空间改进

#### 3.2.1 动作延迟补偿

**当前问题**：
```cpp
// 直接积分，假设瞬时响应
joint_act += increment * _rl_time_step;
```

**改进方案**：
```cpp
// 考虑电机响应延迟
class ActionDelayCompensator {
    std::deque<Vec10<float>> action_history;  // 动作历史
    float estimated_delay = 0.03f;            // 估计延迟（30ms）
    
public:
    Vec10<float> get_compensated_action(Vec10<float> current_action) {
        // 预测未来动作（如果模型输出是预测性的）
        // 或使用历史动作的平均值
        if (action_history.size() > 3) {
            return (current_action + action_history.back()) * 0.5f;
        }
        return current_action;
    }
};
```

#### 3.2.2 速度限制和加速度限制

**建议**：
```cpp
// 添加速度和加速度限制
void apply_action_limits(Vec10<float>& joint_act, Vec10<float>& joint_vel_target) {
    static Vec10<float> prev_joint_act = joint_act;
    Vec10<float> desired_vel = (joint_act - prev_joint_act) / _rl_time_step;
    
    // 速度限制
    float max_vel = 2.0f;  // rad/s
    for (int i = 0; i < 10; ++i) {
        if (std::abs(desired_vel[i]) > max_vel) {
            joint_act[i] = prev_joint_act[i] + std::copysign(max_vel * _rl_time_step, desired_vel[i]);
        }
    }
    
    // 加速度限制
    Vec10<float> desired_acc = (desired_vel - prev_vel) / _rl_time_step;
    // ... 类似处理
    
    prev_joint_act = joint_act;
    prev_vel = desired_vel;
}
```

#### 3.2.3 动作平滑（扩展到RL模式）

**当前问题**：
- `smooth_joint_action()` 只在stand模式使用

**改进方案**：
```cpp
// 在RL模式也添加轻量级平滑
void rl_control() {
    // ... ONNX推理 ...
    
    // 应用动作增量前先平滑
    Vec10<float> smoothed_increment = smooth_action_increment(action_increment);
    joint_increment_control(smoothed_increment);
}

Vec10<float> smooth_action_increment(Matrix<float, Dynamic, 1> increment) {
    static Vec10<float> prev_increment = Vec10<float>::Zero();
    Vec10<float> joint_inc = increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS);
    
    // 低通滤波平滑
    float smooth_factor = 0.7f;  // 可调参数
    Vec10<float> smoothed = smooth_factor * prev_increment + (1 - smooth_factor) * joint_inc;
    prev_increment = smoothed;
    
    return smoothed;
}
```

### 3.3 时间同步改进

#### 3.3.1 使用真实控制周期

**当前问题**：
```cpp
float _rl_time_step = 0.01f;  // 固定值
```

**改进方案**：
```cpp
void rl_control() {
    // 使用真实测量周期
    _rl_time_step = get_true_loop_period();
    
    // 在积分时使用真实周期
    joint_act += increment * _rl_time_step;
}
```

#### 3.3.2 时间戳同步

**建议**：
```cpp
// 为传感器数据添加时间戳
struct TimestampedState {
    MotorState motor_state;
    BaseState base_state;
    uint64_t timestamp_us;  // 微秒时间戳
};

// 在观测构建时考虑时间延迟
void convert_dds_state2rl_state() {
    // 检查时间戳，如果延迟过大则丢弃或标记
    uint64_t current_time = get_current_time_us();
    uint64_t state_time = dds_motor_state->GetData()->timestamp_us;
    float delay = (current_time - state_time) / 1e6f;  // 秒
    
    if (delay > 0.05f) {  // 延迟超过50ms
        // 使用预测或历史值
        // 或发出警告
    }
}
```

### 3.4 标定和坐标系改进

#### 3.4.1 自动零位标定

**当前问题**：
- 零位 (`ref_joint_act`) 需要手动设置

**改进方案**：
```cpp
// 添加零位自动标定功能
void auto_calibrate_zero_position(int num_samples = 100) {
    Vec10<float> sum = Vec10<float>::Zero();
    
    for (int i = 0; i < num_samples; ++i) {
        convert_dds_state2rl_state();
        sum += joint_pos;
        usleep(10000);  // 10ms
    }
    
    _ref_joint_act = sum / num_samples;
    cout << "Auto-calibrated zero position: " << _ref_joint_act.transpose() << endl;
}
```

#### 3.4.2 坐标系标定

**建议**：
```cpp
// 添加IMU安装角度标定
struct CoordinateCalibration {
    Vec3<float> imu_rpy_offset;      // IMU安装角度偏移
    Vec3<float> trans_axis;          // 坐标转换轴（可配置）
};

// 在观测构建时应用
base_rpy_calibrated = (base_rpy + calibration.imu_rpy_offset) * calibration.trans_axis;
```

### 3.5 域随机化（Domain Randomization）

#### 3.5.1 训练时随机化

**建议**：在训练环境中添加随机化：
- 传感器噪声（高斯噪声）
- 延迟（随机延迟）
- 动力学参数（质量、摩擦系数）
- 观测缩放因子

#### 3.5.2 部署时自适应

**改进方案**：
```cpp
// 在线估计域参数，并自适应调整
class DomainAdaptation {
    Vec10<float> estimated_mass_scale;      // 估计的质量缩放
    Vec10<float> estimated_friction_scale;  // 估计的摩擦缩放
    
public:
    void adapt_observation(Matrix<float, Dynamic, 1>& obs) {
        // 根据估计的域参数调整观测
        // 例如：补偿质量差异对加速度的影响
    }
    
    void adapt_action(Matrix<float, Dynamic, 1>& action) {
        // 根据估计的域参数调整动作
        // 例如：补偿摩擦对速度的影响
    }
};
```

### 3.6 模型输出后处理

#### 3.6.1 输出裁剪改进

**当前实现**：
```cpp
// 在onnx_inference.cpp中
auto net_out_action = net_out.cwiseMax(-1).cwiseMin(1.);
```

**改进方案**：
```cpp
// 使用软裁剪或自适应裁剪
Matrix<float, Dynamic, 1> soft_clip_output(Matrix<float, Dynamic, 1> output) {
    // 方法1：tanh缩放（更平滑）
    return output.array().tanh();
    
    // 方法2：带斜率的软裁剪
    float slope = 0.1f;  // 超出范围时的斜率
    for (int i = 0; i < output.size(); ++i) {
        if (output[i] > 1.0f) {
            output[i] = 1.0f + (output[i] - 1.0f) * slope;
        } else if (output[i] < -1.0f) {
            output[i] = -1.0f + (output[i] + 1.0f) * slope;
        }
    }
    return output;
}
```

---

## 四、实施优先级建议

### 高优先级（立即实施）

1. **使用真实控制周期**
   ```cpp
   _rl_time_step = get_true_loop_period();  // 而不是固定值
   ```

2. **添加动作平滑（RL模式）**
   - 在 `rl_control()` 中对动作增量进行低通滤波

3. **改进零位标定**
   - 添加自动标定功能或更精确的手动标定流程

### 中优先级（近期实施）

4. **自适应滤波权重**
   - 根据传感器噪声动态调整滤波参数

5. **动作限制**
   - 添加速度和加速度限制

6. **观测归一化改进**
   - 使用软裁剪或自适应阈值

### 低优先级（长期优化）

7. **域自适应**
   - 在线估计域参数并自适应调整

8. **时间戳同步**
   - 添加传感器数据时间戳，处理延迟

9. **完整标定系统**
   - 建立完整的标定流程和工具

---

## 五、代码修改示例

### 5.1 修改 rl_control() 使用真实周期

```cpp
void RLController::rl_control() {
    counter_rl++;
    
    // 使用真实控制周期
    _rl_time_step = get_true_loop_period();
    
    // ... 其余代码保持不变 ...
    
    // 在积分时使用真实周期
    joint_act.segment(0, NUM_ACTUAT_JOINTS) += 
        increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS) * _rl_time_step;
}
```

### 5.2 添加动作平滑到RL模式

```cpp
// 在 rl_controller.h 中添加
private:
    Vec10<float> prev_action_increment;  // 上次的动作增量
    
// 在 rl_controller.cpp 中修改
void RLController::rl_control() {
    // ... ONNX推理 ...
    
    // 平滑动作增量
    Vec10<float> joint_inc = action_increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS);
    float smooth_factor = 0.7f;  // 可配置
    joint_inc = smooth_factor * prev_action_increment + 
                (1 - smooth_factor) * joint_inc;
    prev_action_increment = joint_inc;
    
    // 应用平滑后的增量
    Matrix<float, Dynamic, 1> smoothed_increment = action_increment;
    smoothed_increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS) = joint_inc;
    joint_increment_control(smoothed_increment);
}
```

### 5.3 添加自适应滤波

```cpp
// 在 rl_controller.h 中添加
private:
    Vec10<float> joint_pos_variance;  // 关节位置方差（用于估计噪声）
    
// 在 rl_controller.cpp 中修改
void RLController::convert_dds_state2rl_state() {
    if (dds_motor_state->GetData()) {
        for (int i = 0; i < NUM_JOINTS; ++i) {
            float sensor_value = dds_motor_state->GetData()->q[i];
            
            // 更新方差估计（简化版）
            float error = sensor_value - joint_pos[jointIndex2Sim[i]];
            joint_pos_variance[jointIndex2Sim[i]] = 
                0.95f * joint_pos_variance[jointIndex2Sim[i]] + 0.05f * error * error;
            
            // 自适应滤波权重
            float noise_level = sqrt(joint_pos_variance[jointIndex2Sim[i]]);
            float adaptive_weight = std::min(0.9f, 0.2f + noise_level * 0.5f);
            
            joint_pos[jointIndex2Sim[i]] = 
                exp_filter(joint_pos[jointIndex2Sim[i]], sensor_value, adaptive_weight);
        }
    }
}
```

---

## 六、测试和验证

### 6.1 对比测试

- **仿真测试**：在仿真环境中测试改进后的代码
- **真实测试**：在真实机器人上对比改进前后的性能

### 6.2 性能指标

- **稳定性**：机器人是否能稳定站立和运动
- **跟踪精度**：关节位置和速度的跟踪误差
- **鲁棒性**：对扰动和噪声的容忍度

### 6.3 调试工具

```cpp
// 添加调试输出
void RLController::debug_print_observation() {
    if (counter_print % 100 == 0) {  // 每100次打印一次
        cout << "Obs range: [" << observation.minCoeff() 
             << ", " << observation.maxCoeff() << "]" << endl;
        cout << "Action range: [" << action_increment.minCoeff() 
             << ", " << action_increment.maxCoeff() << "]" << endl;
        cout << "True period: " << _rl_time_step * 1000 << " ms" << endl;
    }
}
```

---

## 七、总结

Sim2Real gap 是强化学习机器人部署中的核心挑战。通过：

1. **改进观测空间**：自适应滤波、标定、归一化
2. **改进动作空间**：延迟补偿、限制、平滑
3. **改进时间同步**：使用真实周期、时间戳
4. **改进标定**：自动零位、坐标系标定
5. **域自适应**：在线估计和调整

可以显著降低 gap，提高模型在真实机器人上的性能。

建议优先实施**高优先级**改进，这些改动简单但效果明显。

