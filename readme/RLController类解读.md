# RLController 类代码解读

## 概述

`RLController` 是强化学习控制器的核心类，负责：
- ONNX模型的加载和推理
- 机器人状态管理（基座姿态、关节状态）
- 观测向量构建和动作执行
- 多种控制模式（站立、RL控制、正弦测试等）

---

## 头文件包含

```cpp
#include "onnx/onnxruntime_cxx_api.h"  // ONNX Runtime API（用于模型推理）
#include "onnx_inference.h"             // ONNX推理封装类
#include "utils/config.h"               // 配置文件参数
#include "utils/cpp_types.h"            // 类型定义（Vec2, Vec3, Vec10等）
#include "unitree/g1/motors.hpp"        // 电机命令/状态定义
#include "unitree/g1/data_buffer.hpp"   // DDS数据缓冲区
#include "unitree/g1/base_state.hpp"    // 基座状态定义
#include "utils/orientation_tools.h"    // 姿态转换工具
#include "unitree/g1/joystick.hpp"      // 手柄输入
#include "joystick_reader.h"            // 手柄读取器
```

---

## 类型定义说明

基于 `cpp_types.h`：
- `Vec2<float>`: 2维向量（Eigen::Matrix<float, 2, 1>）
- `Vec3<float>`: 3维向量
- `Vec4<float>`: 4维向量（常用于四元数）
- `Vec10<float>`: 10维向量（10个关节）
- `Matrix<float, Dynamic, 1>`: 动态大小的一维矩阵

---

## 类成员变量详解

### 一、控制标志和计数器（Public）

```cpp
bool _is_first_run = true;      // 首次运行标志（用于初始化观测堆叠）
int counter_print = 0;           // 打印计数器
int counter_rl = 0;              // RL推理计数器
int task_mode = 0;               // 任务模式（3=forward, 4=stand, 5=sin test等）
float _rl_time_step = 0.01f;     // RL控制周期（默认10ms）
float _record_yaw = 0.;          // 记录的偏航角（用于yaw控制）
float static_flag = 0.f;         // 静止/运动标志（0=静止, 1=运动）
```

### 二、机器人几何常量

```cpp
static const int NUM_LEGS = 2;           // 腿的数量（左腿、右腿）
static const int NUM_JOINTS = 10;        // 关节总数
static const int NUM_ACTUAT_JOINTS = 10; // 执行关节数（与NUM_JOINTS相同）
```

### 三、目标命令和步态控制（Public）

```cpp
Vec2<float> target_command;           // 目标速度命令 [vx, yr]
                                       // vx: 前进速度 (m/s)
                                       // yr: 转向角速度 (rad/s)

Vec2<float> pm_f;                     // 步态相位频率 (Hz) [左腿频率, 右腿频率]
                                       // 从ONNX模型输出的前2维转换而来
                                       // 范围: 0.5~3.5 Hz

Vec4<float> pm_phase_sin_cos;         // 相位信息的sin/cos编码
                                       // [sin(左腿相位), sin(右腿相位), 
                                       //  cos(左腿相位), cos(右腿相位)]
                                       // 用于观测向量构建
```

### 四、基座状态（Public）

```cpp
Vec3<float> base_rpy;        // 基座姿态角 (roll, pitch, yaw) - 弧度
Vec3<float> base_rpy_rate;   // 基座角速度 (roll_rate, pitch_rate, yaw_rate) - rad/s
Vec3<float> base_vel;        // 基座线速度 [vx, vy, vz] - m/s（当前未使用）
Vec3<float> base_acc;        // 基座加速度 [ax, ay, az] - m/s²（当前未使用）
Vec4<float> base_quat;       // 基座四元数 (w, x, y, z)（当前未使用）
```

### 五、关节状态（Public）

```cpp
Vec10<float> joint_pos;              // 关节实际位置 (rad) - 从电机反馈读取
Vec10<float> joint_vel;              // 关节实际速度 (rad/s) - 从电机反馈读取
Vec10<float> joint_tau;              // 关节估计力矩 (N·m) - 从电机反馈读取
Vec10<float> joint_acc;              // 关节加速度 (rad/s²) - 从电机反馈读取
Vec10<float> joint_pos_error;        // 关节位置误差 = joint_act - joint_pos
Vec10<float> joint_act;              // 关节目标位置 (rad) - 控制器的输出目标
Vec10<float> init_joint_act;         // 初始关节位置 - reset时记录
Vec10<float> motion_test_start_joint_act;  // 运动测试起始位置（未使用）
Vec10<float> motion_test_end_joint_act;    // 运动测试结束位置（未使用）
Vec10<float> output_joint_act;       // 输出关节位置（未使用）
Vec10<float> joint_vel_target;       // 目标关节速度 (rad/s)
                                      // 用于速度前馈控制（stand模式）
```

### 六、ONNX模型相关（Public）

```cpp
Matrix<float, Dynamic, 1> action_increment;  // 动作增量向量（12维）
                                             // 从ONNX模型输出转换而来
                                             // [0:1] 相位频率增量
                                             // [2:11] 关节位置增量

Matrix<float, Dynamic, 1> observation;       // 观测向量（129维）
                                             // = 单帧观测(43维) × 堆叠帧数(3)
                                             // 用于ONNX模型输入
```

### 七、DDS数据缓冲区（Public）

```cpp
DataBuffer<MotorCommand> *dds_motor_command;  // 电机命令缓冲区（发布）
DataBuffer<MotorState> *dds_motor_state;      // 电机状态缓冲区（订阅）
DataBuffer<BaseState> *dds_base_state;        // 基座状态缓冲区（订阅）
```

**说明**：
- DDS (Data Distribution Service) 是实时数据通信中间件
- `dds_motor_command`: 控制器向电机发送命令（位置、速度、刚度、阻尼）
- `dds_motor_state`: 从电机读取反馈（位置、速度、力矩）
- `dds_base_state`: 从IMU读取基座状态（姿态、角速度、加速度）

### 八、输入设备（Public）

```cpp
Gamepad *gamepad = nullptr;           // 游戏手柄对象（未使用）
JoystickReader *jsreader = nullptr;   // 手柄读取器（实际使用）
```

### 九、控制参数（Public）

```cpp
Vec10<float> _kp;        // 位置刚度系数 (10个关节) - 用于位置控制
Vec10<float> _kd;        // 速度阻尼系数 (10个关节) - 用于位置控制
Vec10<float> _kp_soft;   // 软控制位置刚度 - 用于泄力模式
Vec10<float> _kd_soft;   // 软控制速度阻尼 - 用于泄力模式

ConfigParams configParams;  // 配置参数对象（从config.yaml读取）
                            // 包含：限位、参考位置、刚度阻尼等
```

### 十、私有成员变量（Private）

```cpp
Vec10<int> jointIndex2Sim;     // 关节索引映射 [0,1,2,3,4,5,6,7,8,9]
                                // 当前为恒等映射（索引i对应关节i）

Vec2<float> _pm_phase;         // 相位角 [左腿相位, 右腿相位]
                                // 范围: [0, 2π]，通过pm_f积分得到

Vec10<float> _ref_joint_act;   // 参考关节位置（站立姿态零位）
                                // 从config.yaml的ref_joint_act读取
                                // 用于计算关节位置偏差（观测向量）

Vec10<float> _offset_joint_act; // 关节位置偏移（当前未使用）
```

### 十一、ONNX推理相关（Private）

```cpp
OnnxInference onnxInference;     // ONNX推理封装对象
                                  // 负责模型加载、输入/输出管理

Ort::Session *motion_session;     // ONNX Runtime会话对象
                                   // 直接指向加载的模型

pthread_mutex_t _rl_state_mutex;  // 线程互斥锁
                                   // 保护观测向量构建过程中的共享状态
```

### 十二、限制和缓冲区（Private）

```cpp
Vec10<float> act_pos_low;        // 关节位置下限 (10个关节)
Vec10<float> act_pos_high;       // 关节位置上限 (10个关节)
                                 // 从config.yaml的act_pos_low/high读取

std::vector<Matrix<float, Dynamic, 1>> obs_stack;  // 观测堆叠缓冲区
                                                    // 存储最近3帧观测（FIFO队列）
                                                    // 用于构建129维堆叠观测向量
```

### 十三、辅助数据（Private）

```cpp
vector<vector<float>> sim_gait_data;  // 模拟步态数据（用于sim_gait_control模式）
                                      // 存储预定义的步态轨迹
```

---

## 成员函数分类

### 一、初始化和重置（Public）

```cpp
void init();                        // 初始化控制器
                                    // - 加载ONNX模型
                                    // - 初始化状态变量
                                    // - 加载配置参数

void reset(bool is_test_local);     // 重置控制器状态
                                    // - 重置计数器
                                    // - 重置观测历史
                                    // - 初始化关节位置
```

### 二、核心控制函数（Public）

```cpp
void rl_control();                  // RL控制主函数
                                    // - 获取观测
                                    // - ONNX推理
                                    // - 应用动作增量

void stand_control(float ratio);    // 站立控制
                                    // - 平滑过渡到ref_joint_act
                                    // - 使用S曲线插值
                                    // - 生成速度前馈

void sim_gait_control();            // 模拟步态控制
                                    // - 播放预定义步态数据

void sin_control(float amplitude, float f, float motion_time);
                                    // 正弦测试控制
                                    // - 单个关节正弦运动
```

### 三、状态转换函数（Public）

```cpp
void convert_dds_state2rl_state();  // 将DDS状态转换为RL状态
                                    // - 读取电机状态
                                    // - 读取IMU状态
                                    // - 指数滤波平滑

void set_rl_joint_act2dds_motor_command(char mode);
                                    // 将RL关节动作转换为电机命令
                                    // - 根据模式设置kp/kd
                                    // - 设置速度前馈（stand模式）
                                    // - 发布到DDS
```

### 四、观测和动作处理（Private）

```cpp
Matrix<float, Dynamic, 1> get_observation();
                                    // 构建观测向量（43维）
                                    // - 组装各部分观测
                                    // - 堆叠历史帧
                                    // - 返回129维堆叠观测

Matrix<float, Dynamic, -1> transform(Matrix<float, Dynamic, -1> data);
                                    // 动作转换
                                    // - 将[-1,1]映射到实际范围
                                    // - 前2维：相位频率
                                    // - 后10维：关节增量

void joint_increment_control(Matrix<float, Dynamic, 1> increment);
                                    // 关节增量控制
                                    // - 更新相位频率
                                    // - 积分关节位置
                                    // - 限位检查
```

### 五、辅助控制函数（Private）

```cpp
void joystick_command_process();    // 处理摇杆输入
                                    // - 读取摇杆值
                                    // - 生成目标速度命令
                                    // - Yaw控制逻辑

void compute_pm_phase(Vec2<float> f);
                                    // 计算相位角
                                    // - 积分频率得到相位
                                    // - 取模[0, 2π]

void smooth_joint_action(float ratio, const Vec10<float> &end_joint_act, 
                        float dt, float max_vel);
                                    // 平滑关节动作
                                    // - S曲线插值
                                    // - 速度限制
                                    // - 误差检查

float smooth_interpolation(float t);
                                    // S曲线插值函数
                                    // 5次多项式：6t⁵-15t⁴+10t³
```

### 六、工具函数（Public/Private）

```cpp
float exp_filter(float history, float present, float weight);
                                    // 指数滤波
                                    // result = history*weight + present*(1-weight)

float get_true_loop_period();       // 获取真实控制周期
                                    // - 计算实际执行时间

float smallest_signed_angle_between(float alpha, float beta);
                                    // 计算两个角度之间的最小有符号角度差
```

### 七、姿态转换函数（Public）

```cpp
Vec3<float> quat_rotate_inverse(Vec4<float> q, Vec3<float> v);
                                    // 四元数逆旋转（向量从世界系到基座系）

Vec4<float> quat_product(Vec4<float> &q1, Vec4<float> &q2);
                                    // 四元数乘法

Vec4<float> rpy_to_quat(const Vec3<float> &rpy);
                                    // RPY转四元数

Vec4<float> quat_mul(Vec4<float> a, Vec4<float> b);
                                    // 四元数乘法（另一种实现）

static Vec3<float> convert_world_frame_to_base_frame(
    const Vec3<float> &world_vec, const Vec3<float> &rpy);
                                    // 世界坐标系到基座坐标系转换（静态函数）
```

### 八、析构函数（Public）

```cpp
virtual ~RLController();
                                    // 析构函数
                                    // - 释放ONNX会话
                                    // - 释放DDS缓冲区
                                    // - 释放输入设备
```

---

## 关键设计模式

### 1. 数据流向

```
传感器 → DDS缓冲区 → convert_dds_state2rl_state() 
    → get_observation() → ONNX推理 
    → joint_increment_control() → set_rl_joint_act2dds_motor_command() 
    → DDS缓冲区 → 电机执行器
```

### 2. 线程安全

- 使用 `pthread_mutex_t _rl_state_mutex` 保护 `get_observation()` 中的共享状态
- 防止多线程访问时数据竞争

### 3. 观测堆叠机制

- 使用 `obs_stack` (FIFO队列) 存储3帧历史观测
- 首次运行：用当前观测填充整个队列
- 正常运行：滑动窗口更新（移除最旧，添加最新）

### 4. 状态管理

- `_is_first_run`: 标识首次运行，用于初始化观测堆叠
- `static_flag`: 区分静止/运动状态，影响观测向量中相位信息的有效性

---

## 使用示例

```cpp
// 1. 创建控制器
RLController controller;

// 2. 初始化（加载模型、配置）
controller.init();

// 3. 设置DDS缓冲区（由外部代码设置）
controller.dds_motor_state = motor_state_buffer;
controller.dds_base_state = base_state_buffer;
controller.dds_motor_command = motor_command_buffer;

// 4. 重置状态
controller.reset(false);

// 5. 控制循环（每15ms执行一次）
while (running) {
    // 更新状态
    controller.convert_dds_state2rl_state();
    
    // 执行控制（根据模式选择）
    if (mode == '3') {
        controller.rl_control();
    } else if (mode == '2') {
        controller.stand_control(ratio);
    }
    
    // 发送电机命令
    controller.set_rl_joint_act2dds_motor_command(mode);
}
```

---

## 注意事项

1. **内存管理**：使用原始指针，需要在析构函数中手动释放资源
2. **线程安全**：只在 `get_observation()` 中使用互斥锁，其他地方需要外部保证线程安全
3. **配置依赖**：大量参数从 `config.yaml` 读取，修改配置后需要重新初始化
4. **ONNX模型**：模型文件路径为 `"policy.onnx"`，需要确保在可执行文件目录下存在
5. **DDS缓冲区**：必须在调用控制函数前设置好DDS缓冲区指针

---

## 扩展建议

1. **使用智能指针**：将原始指针改为 `std::shared_ptr` 或 `std::unique_ptr`
2. **异常处理**：添加ONNX模型加载失败、DDS通信异常等错误处理
3. **日志系统**：添加详细的日志记录，便于调试和性能分析
4. **配置验证**：在 `init()` 中验证配置参数的有效性
5. **状态机模式**：将模式切换封装为状态机，提高代码可维护性

