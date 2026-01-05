# RL控制缓冲区机制分析

## 一、缓冲区存储帧数

### 1. DataBuffer 是单帧缓冲区

**结论**：`DataBuffer<MotorCommand>` **只存储1帧数据**（最新的一帧），不是多帧缓冲区。

### 2. 工作原理

```cpp
// 写入缓冲区（每次Control循环都会更新）
void RLController::set_rl_joint_act2dds_motor_command(char mode) {
    MotorCommand motor_command_tmp;
    // ... 填充数据 ...
    dds_motor_command->SetData(motor_command_tmp);  // ⭐ 覆盖写入，只保留最新一帧
}

// 读取缓冲区（每次命令写入线程都会读取）
unitree_hg::msg::dds_::LowCmd_ G1::SetMotorCmd() {
    const std::shared_ptr<const MotorCommand> mc = motor_command_buffer_.GetData();  // ⭐ 读取最新一帧
    // ... 使用数据 ...
}
```

**特点**：
- `SetData()` 每次调用都会**覆盖**缓冲区中的数据
- `GetData()` 读取的是**最新**的一帧数据
- 这是一个**线程安全的单帧缓冲区**，用于在控制线程和命令写入线程之间传递数据

---

## 二、模式切换时的行为分析

### 场景：RL模式 → 站立模式 → RL模式

#### 时间线分析

```
时刻 T0: RL模式运行中
  - Control() 循环执行
  - rl_control() 计算 joint_act
  - set_rl_joint_act2dds_motor_command('3') 更新缓冲区
  - 缓冲区: RL指令

时刻 T1: 切换到站立模式
  - ModeProcess() 检测到模式变化
  - 调用 reset() 重置状态
  - current_mode = '2'
  - relative_time = 0

时刻 T2: 站立模式第一帧
  - Control() 执行
  - stand_control(ratio) 计算 joint_act（从当前位置平滑过渡到参考位置）
  - set_rl_joint_act2dds_motor_command('2') 更新缓冲区
  - 缓冲区: 站立指令（覆盖了之前的RL指令）

时刻 T3-Tn: 站立模式持续运行
  - 每次Control()循环都更新缓冲区为站立指令
  - 缓冲区: 站立指令

时刻 Tn+1: 切换回RL模式
  - ModeProcess() 检测到模式变化
  - 调用 reset() 重置状态
  - current_mode = '3'
  - counter_rl = 0
  - joint_act = joint_pos（设置为当前实际位置）

时刻 Tn+2: RL模式第一帧
  - Control() 执行
  - rl_control() 被调用
  - counter_rl = 1（第一次RL控制）
  - 由于 counter_rl < 2，先调用 stand_control(ratio)
  - 然后调用 rl_control()
  - rl_control() 计算新的 joint_act（基于当前状态）
  - set_rl_joint_act2dds_motor_command('3') 更新缓冲区
  - 缓冲区: **新的RL指令**（不是上一次RL的指令）
```

### 关键代码分析

**文件**: `src/user/custom.cpp`

```cpp
void G1::Control() {
    // ...
    switch (current_mode) {
        case '3':
            ///rl
            rlController->rl_control();  // ⭐ 计算新的RL指令
            if (rlController->counter_rl < 2)
                rlController->stand_control(ratio);  // 前两帧先执行站立控制
            break;
        // ...
    }
    rlController->set_rl_joint_act2dds_motor_command(current_mode);  // ⭐ 更新缓冲区
}
```

**文件**: `src/user/rl_controller.cpp`

```cpp
void RLController::rl_control() {
    counter_rl++;
    Matrix<float, Dynamic, 1> net_out;
    net_out = onnxInference.inference(motion_session, get_observation());  // ⭐ 基于当前观测推理
    action_increment = transform(net_out);
    joint_increment_control(action_increment);  // ⭐ 更新 joint_act
    _rl_time_step = get_true_loop_period();
}

void RLController::joint_increment_control(Matrix<float, Dynamic, 1> increment) {
    pm_f = increment.segment(0, NUM_LEGS);
    compute_pm_phase(pm_f);
    joint_act.segment(0, NUM_ACTUAT_JOINTS) += increment.segment(NUM_LEGS, NUM_ACTUAT_JOINTS) * _rl_time_step;  // ⭐ 增量更新
    joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
}
```

**文件**: `src/user/rl_controller.cpp`

```cpp
void RLController::reset(bool is_test_local) {
    // ...
    if (is_test_local) {
        init_joint_act = joint_act;
        joint_pos = joint_act;
    } else {
        convert_dds_state2rl_state();  // ⭐ 读取当前实际位置
        joint_act = joint_pos;  // ⭐ 重置为当前实际位置
        init_joint_act = joint_pos;
    }
    // ...
}
```

---

## 三、结论

### 问题1：缓冲区存储几帧数据？

**答案**：**1帧**（单帧缓冲区）

- `DataBuffer` 只存储最新的一帧数据
- 每次 `SetData()` 都会覆盖之前的数据
- 这是一个线程安全的单帧缓冲区

### 问题2：RL模式 → 站立 → RL模式，第一帧执行的是上一次RL的指令吗？

**答案**：**不是**，第一帧执行的是**新RL模式计算出的指令**

**原因**：

1. **模式切换时**：
   - `reset()` 会将 `joint_act` 重置为当前实际位置（`joint_pos`）
   - 清空了之前RL模式的状态

2. **RL模式第一帧**：
   - `rl_control()` 基于**当前观测**（`get_observation()`）进行推理
   - 计算出的 `action_increment` 是基于**当前状态**的增量
   - `joint_act` 被更新为：`joint_act = joint_pos + action_increment * dt`
   - 缓冲区被更新为新的RL指令

3. **特殊情况**：
   - 如果 `counter_rl < 2`，前两帧会先执行 `stand_control(ratio)`
   - 但 `rl_control()` 仍然会被调用，`joint_act` 会被RL控制更新
   - 最终缓冲区中的是RL控制计算出的指令

### 数据流向图

```
模式切换：RL → Stand → RL

RL模式（切换前）:
  joint_act = RL计算值
  ↓
  SetData() → 缓冲区: RL指令

切换到Stand模式:
  reset() → joint_act = joint_pos（当前实际位置）
  ↓
  stand_control() → joint_act = 平滑过渡值
  ↓
  SetData() → 缓冲区: Stand指令（覆盖RL指令）

切换回RL模式:
  reset() → joint_act = joint_pos（当前实际位置）
  ↓
  rl_control() → joint_act = joint_pos + RL增量（新计算）
  ↓
  SetData() → 缓冲区: 新RL指令（覆盖Stand指令）
```

---

## 四、注意事项

### 1. 缓冲区是实时更新的

- 每个控制周期（15ms）都会更新缓冲区
- 缓冲区中始终是**最新**的控制指令
- 不会保留历史指令

### 2. 模式切换时的状态重置

- `reset()` 会重置 `joint_act` 为当前实际位置
- 确保模式切换时从当前状态开始，而不是从旧状态继续

### 3. RL模式的前两帧特殊处理

```cpp
case '3':
    rlController->rl_control();  // 计算RL指令
    if (rlController->counter_rl < 2)
        rlController->stand_control(ratio);  // 前两帧先执行站立控制
    break;
```

**说明**：
- 前两帧会先执行站立控制，确保平滑过渡
- 但 `rl_control()` 仍然会被调用，`joint_act` 会被RL控制更新
- 最终缓冲区中的是RL控制计算出的指令

---

## 五、总结

| 问题 | 答案 |
|------|------|
| 缓冲区存储几帧数据？ | **1帧**（单帧缓冲区） |
| 切换回RL第一帧执行旧RL指令？ | **不是**，执行的是**新RL模式计算出的指令** |
| 缓冲区更新频率？ | 每个控制周期（15ms）更新一次 |
| 模式切换时缓冲区内容？ | 立即更新为新模式的指令 |

