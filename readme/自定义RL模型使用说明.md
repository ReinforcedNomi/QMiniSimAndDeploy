# 自定义RL模型使用说明

## 概述

本系统新增了独立的自定义RL控制器，支持您自己训练的ONNX模型，完全独立于原有的RLController，不会产生冲突。

---

## 模型规格

### 输入（观测）
- **总维度**：195维
- **单帧观测**：39维
- **历史帧数**：5帧
- **计算公式**：195 = 39 × 5

### 单帧观测（39维）组成

| 序号 | 名称 | 维度 | 缩放 | 说明 |
|------|------|------|------|------|
| 0-2 | `base_ang_vel` | 3 | ×0.2 | 基座角速度（roll, pitch, yaw的角速度） |
| 3-5 | `projected_gravity` | 3 | 无 | 投影重力向量（重力在基座坐标系中的投影） |
| 6-8 | `velocity_commands` | 3 | 无 | 速度命令（来自摇杆：vx, vy, yaw_rate） |
| 9-18 | `joint_pos_rel` | 10 | 无 | 关节位置偏差（相对于参考位置） |
| 19-28 | `joint_vel_rel` | 10 | ×0.05 | 关节速度 |
| 29-38 | `last_action` | 10 | 无 | 上一步动作 |

### 输出（动作）
- **维度**：10维
- **范围**：[-1, 1]（归一化）
- **控制方式**：位置增量控制（与原工程相同）

---

## 使用方法

### 1. 准备ONNX模型文件

将您训练的ONNX模型文件命名为 `custom_policy.onnx`，放在程序运行目录下（与`policy.onnx`同级）。

**模型要求**：
- 输入节点名称：`input`（默认）
- 输出节点名称：`output`（默认）
- 输入形状：`[1, 195]`（195维观测）
- 输出形状：`[1, 10]`（10维动作）

### 2. 启动程序

正常启动程序，自定义RL控制器会在初始化时自动加载模型。

**如果模型加载失败**：
- 程序会输出警告信息
- 自定义RL模式将不可用
- 其他功能不受影响

### 3. 切换到自定义RL模式

**手柄操作**：
- 按下 **RB按键**（R1键）切换到自定义RL控制模式
- 程序会输出：`Current mode: Custom RL control...`

**摇杆控制**：
- **左摇杆X/Y方向**：控制速度命令的x和y分量
  - X轴（左右）：`velocity_command[0]`
  - Y轴（前后）：`velocity_command[1]`
- **右摇杆X方向**：控制角速度命令（Z自转）
  - X轴：`velocity_command[2]`（yaw_rate）

### 4. 退出自定义RL模式

按下其他模式按键（如A键切换到站立模式）即可退出。

---

## 代码结构

### 新增文件

1. **`include/user/custom_rl_controller.h`**
   - 自定义RL控制器类定义
   - 包含195维观测构建、10维动作处理等

2. **`src/user/custom_rl_controller.cpp`**
   - 自定义RL控制器实现
   - 观测构建、ONNX推理、动作处理等

### 修改的文件

1. **`include/user/custom.hpp`**
   - 添加了 `CustomRLController *customRLController` 成员变量

2. **`src/user/custom.cpp`**
   - 在构造函数中初始化自定义RL控制器
   - 在`Control()`中添加模式'C'的处理
   - 在`ModeProcess()`中添加模式切换逻辑

3. **`include/user/mode_switcher.h`**
   - 添加RB按键（But[5]）映射到模式'C'
   - 添加模式'C'的打印信息

---

## 观测构建详解

### 1. base_ang_vel（基座角速度）

```cpp
obs.segment(0, 3) = base_rpy_rate * 0.2f;
```

- 来源：`dds_base_state->omega`（IMU角速度）
- 缩放：×0.2
- 单位：rad/s

### 2. projected_gravity（投影重力向量）

```cpp
Vec3<float> gravity_world(0, 0, -1);  // 世界坐标系重力方向
Vec3<float> gravity_base = quat_rotate_inverse(base_quat, gravity_world);
obs.segment(3, 3) = gravity_base;
```

- 来源：通过四元数将世界坐标系的重力向量转换到基座坐标系
- 无缩放
- 单位：归一化向量

### 3. velocity_commands（速度命令）

```cpp
velocity_command[0] = jsreader->Axis[0];  // 左摇杆X -> vx
velocity_command[1] = jsreader->Axis[1];  // 左摇杆Y -> vy
velocity_command[2] = jsreader->Axis[2];  // 右摇杆X -> yaw_rate
obs.segment(6, 3) = velocity_command;
```

- 来源：手柄摇杆输入
- 无缩放
- 范围：[-1, 1]

### 4. joint_pos_rel（关节位置偏差）

```cpp
obs.segment(9, 10) = joint_pos - _ref_joint_act;
```

- 来源：当前关节位置 - 参考关节位置
- 无缩放
- 单位：rad

### 5. joint_vel_rel（关节速度）

```cpp
obs.segment(19, 10) = joint_vel * 0.05f;
```

- 来源：`dds_motor_state->dq`（关节速度）
- 缩放：×0.05
- 单位：rad/s

### 6. last_action（上一步动作）

```cpp
obs.segment(29, 10) = last_action;
```

- 来源：上一帧模型输出的动作
- 无缩放
- 范围：[-1, 1]

---

## 动作处理详解

### 位置增量控制

```cpp
// ONNX推理输出动作（10维，范围[-1, 1]）
Matrix<float, Dynamic, 1> action = onnxInference.inference(...);

// 转换为关节位置增量
float max_increment = 0.1f;  // 最大增量（rad）
Vec10<float> joint_increment;
for (int i = 0; i < ACTION_DIM; ++i) {
    joint_increment[i] = action[i] * max_increment * _rl_time_step;
}

// 增量更新关节目标位置
joint_act += joint_increment;

// 限位保护
joint_act = joint_act.cwiseMax(act_pos_low).cwiseMin(act_pos_high);
```

**说明**：
- 动作范围：[-1, 1]
- 增量范围：[-0.1 * dt, 0.1 * dt] rad
- 控制周期：dt = 0.01s（默认）
- 与原工程的位置增量控制方式相同

---

## 模式切换流程

```
按下RB按键
    ↓
ModeProcess() 检测到模式变化
    ↓
selected_mode = 'C'
    ↓
current_mode = 'C'
    ↓
customRLController->reset()
    ↓
Control() 循环执行
    ↓
case 'C':
    customRLController->convert_dds_state2rl_state()
    customRLController->custom_rl_control()
    customRLController->set_joint_act2dds_motor_command()
```

---

## 注意事项

### 1. 模型文件路径

- 默认路径：`custom_policy.onnx`（程序运行目录）
- 如需修改路径，编辑 `src/user/custom.cpp` 中的初始化代码：
  ```cpp
  customRLController->init("your_model_path.onnx");
  ```

### 2. 观测历史初始化

- 首次进入自定义RL模式时，观测历史会用当前观测填充5帧
- 确保模型能够处理这种初始化方式

### 3. 摇杆输入范围

- 摇杆输入范围：[-1, 1]
- 直接作为速度命令使用，无额外缩放
- 如需调整，修改 `process_joystick_input()` 函数

### 4. 动作增量范围

- 当前设置：最大增量 0.1 rad/帧
- 如需调整，修改 `custom_rl_control()` 中的 `max_increment` 参数

### 5. 限位保护

- 关节位置限制在 `[act_pos_low, act_pos_high]` 范围内
- 默认限位值在 `init()` 函数中设置
- 可以从配置文件读取（需要修改代码）

---

## 调试技巧

### 1. 检查模型是否加载成功

程序启动时会输出：
```
Custom RL model loaded: custom_policy.onnx
Custom RL controller initialized successfully.
```

如果看到这些输出，说明模型加载成功。

### 2. 检查当前模式

按下RB按键后，应该看到：
```
Current mode: Custom RL control...
```

### 3. 验证观测维度

如果模型输入维度不匹配，ONNX推理会报错。确保：
- 模型输入维度：195
- 模型输出维度：10

### 4. 验证摇杆输入

可以在 `process_joystick_input()` 中添加日志：
```cpp
std::cout << "Velocity command: " << velocity_command.transpose() << std::endl;
```

---

## 常见问题

### Q1: 模型加载失败怎么办？

**A**: 
1. 检查模型文件是否存在
2. 检查模型文件路径是否正确
3. 检查模型文件是否损坏
4. 查看程序输出的错误信息

### Q2: 可以修改观测构建方式吗？

**A**: 可以，修改 `build_single_frame_obs()` 函数即可。

### Q3: 可以修改动作处理方式吗？

**A**: 可以，修改 `custom_rl_control()` 函数中的动作处理部分。

### Q4: 如何调整动作增量范围？

**A**: 修改 `custom_rl_control()` 中的 `max_increment` 参数：
```cpp
float max_increment = 0.1f;  // 改为您需要的值
```

### Q5: 如何从配置文件读取参数？

**A**: 需要修改 `CustomRLController::init()` 函数，添加配置读取逻辑（参考 `RLController` 的实现）。

---

## 总结

通过本实现，您可以：
- ✅ 使用自己训练的ONNX模型（195维输入，10维输出）
- ✅ 通过RB按键切换到自定义RL模式
- ✅ 使用摇杆控制速度命令（左摇杆X/Y，右摇杆Z）
- ✅ 完全独立于原有RLController，不产生冲突
- ✅ 位置增量控制方式与原工程相同

**建议**：在正式使用前，先在测试环境中验证模型是否正常工作。

---

**文档版本**：v1.0  
**最后更新**：2025-01-06

