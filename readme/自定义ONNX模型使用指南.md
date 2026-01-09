# 自定义ONNX模型使用指南

## 概述

本系统现在支持同时保留原有的ONNX推理框架，并添加您自己训练的ONNX模型。您可以通过配置文件轻松切换使用哪套模型。

---

## 快速开始

### 1. 准备您的ONNX模型文件

将您训练的ONNX模型文件放在程序运行目录下（与`policy.onnx`同级目录）。

**示例**：
```
QMiniSimAndDeploy/
├── policy.onnx          # 原始模型（保留）
├── custom_policy.onnx   # 您的自定义模型（新增）
└── config.yaml
```

### 2. 配置config.yaml

在`config.yaml`文件中添加以下配置：

```yaml
# 自定义ONNX模型配置（第二套推理框架）
use_custom_onnx_model: true  # true=使用自定义模型，false=使用默认模型
custom_onnx_model_path: "custom_policy.onnx"  # 您的模型文件路径
custom_onnx_input_node_name: "input"  # 模型的输入节点名称
custom_onnx_output_node_name: "output"  # 模型的输出节点名称

# 如果您的模型输入输出维度与默认模型不同，需要配置以下参数
# custom_onnx_num_observations: 43  # 观测维度（默认与原始模型相同）
# custom_onnx_num_actions: 12      # 动作维度（默认与原始模型相同）
# custom_onnx_num_stacks: 3         # 堆叠帧数（默认与原始模型相同）
```

### 3. 运行程序

程序启动时会自动：
- 加载默认模型（`policy.onnx`）
- 如果`use_custom_onnx_model: true`，加载您的自定义模型
- 根据配置选择使用哪套模型进行推理

---

## 配置参数详解

### 基本配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `use_custom_onnx_model` | bool | `false` | 是否使用自定义模型。`true`=使用自定义模型，`false`=使用默认模型 |
| `custom_onnx_model_path` | string | `"custom_policy.onnx"` | 自定义模型文件的路径（相对于程序运行目录） |
| `custom_onnx_input_node_name` | string | `"input"` | 自定义模型的输入节点名称 |
| `custom_onnx_output_node_name` | string | `"output"` | 自定义模型的输出节点名称 |

### 高级配置（可选）

如果您的模型输入输出维度与默认模型不同，需要配置：

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `custom_onnx_num_observations` | int | 与原始模型相同 | 观测空间维度 |
| `custom_onnx_num_actions` | int | 与原始模型相同 | 动作空间维度 |
| `custom_onnx_num_stacks` | int | 与原始模型相同 | 观测堆叠帧数 |

---

## 使用场景

### 场景1：切换使用自定义模型

**配置**：
```yaml
use_custom_onnx_model: true
custom_onnx_model_path: "my_model.onnx"
```

**效果**：
- 程序使用`my_model.onnx`进行推理
- 原始模型`policy.onnx`仍然保留在系统中，但不使用

### 场景2：切换回默认模型

**配置**：
```yaml
use_custom_onnx_model: false
```

**效果**：
- 程序使用原始的`policy.onnx`进行推理
- 自定义模型不会被加载

### 场景3：模型输入输出节点名称不同

如果您的模型使用了不同的输入输出节点名称：

**配置**：
```yaml
use_custom_onnx_model: true
custom_onnx_model_path: "my_model.onnx"
custom_onnx_input_node_name: "obs"      # 您的模型输入节点名称
custom_onnx_output_node_name: "action"   # 您的模型输出节点名称
```

### 场景4：模型维度不同

如果您的模型输入输出维度与默认模型不同：

**配置**：
```yaml
use_custom_onnx_model: true
custom_onnx_model_path: "my_model.onnx"
custom_onnx_num_observations: 50  # 您的模型观测维度
custom_onnx_num_actions: 10       # 您的模型动作维度
custom_onnx_num_stacks: 3         # 您的模型堆叠帧数
```

---

## 实现原理

### 架构设计

```
RLController
├── onnxInference (原始推理框架)
│   └── motion_session (原始模型Session)
└── customOnnxInference (自定义推理框架)
    └── custom_motion_session (自定义模型Session)
```

### 代码流程

1. **初始化阶段** (`RLController::init()`)：
   ```cpp
   // 加载默认模型
   motion_session = new Ort::Session(env, "policy.onnx", ...);
   
   // 如果启用自定义模型，加载第二套模型
   if (use_custom_onnx_model) {
       custom_motion_session = new Ort::Session(env, custom_model_path, ...);
   }
   ```

2. **推理阶段** (`RLController::rl_control()`)：
   ```cpp
   if (use_custom_onnx_model && custom_motion_session != nullptr) {
       // 使用自定义模型
       net_out = customOnnxInference.inference(custom_motion_session, observation);
   } else {
       // 使用默认模型
       net_out = onnxInference.inference(motion_session, observation);
   }
   ```

3. **资源释放** (`RLController::~RLController()`)：
   ```cpp
   delete motion_session;        // 释放默认模型
   delete custom_motion_session;  // 释放自定义模型
   ```

---

## 注意事项

### 1. 模型文件路径

- 模型文件路径是**相对于程序运行目录**的
- 确保模型文件存在，否则程序会回退到默认模型

### 2. 模型兼容性

- **输入格式**：模型输入应该是 `[batch_size, obs_dim * stack_dim]` 的形状
- **输出格式**：模型输出应该是 `[action_dim]` 的形状
- **数据范围**：输入输出数据范围应该在 `[-1, 1]` 之间（系统会自动进行裁剪）

### 3. 错误处理

如果自定义模型加载失败：
- 程序会输出错误信息到 `stderr`
- 自动回退到使用默认模型
- 程序不会崩溃，可以继续运行

### 4. 性能考虑

- 两套模型都会在初始化时加载到内存
- 但只有一套模型会在运行时使用（根据配置选择）
- 如果内存有限，建议只保留需要使用的模型文件

---

## 调试技巧

### 1. 检查模型是否加载成功

程序启动时会输出：
```
Custom ONNX model loaded: custom_policy.onnx
  Input node: input
  Output node: output
```

如果看到这个输出，说明自定义模型加载成功。

### 2. 检查当前使用的模型

在`rl_control()`函数中添加日志：
```cpp
if (configParams.use_custom_onnx_model && custom_motion_session != nullptr) {
    cout << "Using custom model" << endl;
} else {
    cout << "Using default model" << endl;
}
```

### 3. 验证模型输入输出

如果模型输出异常，检查：
1. 模型输入输出维度是否正确
2. 输入输出节点名称是否正确
3. 模型文件是否损坏

---

## 示例配置

### 示例1：使用自定义模型（标准配置）

```yaml
# 使用自定义模型
use_custom_onnx_model: true
custom_onnx_model_path: "my_trained_model.onnx"
custom_onnx_input_node_name: "input"
custom_onnx_output_node_name: "output"
```

### 示例2：使用自定义模型（不同节点名称）

```yaml
use_custom_onnx_model: true
custom_onnx_model_path: "my_model.onnx"
custom_onnx_input_node_name: "observation"  # 不同的输入节点名称
custom_onnx_output_node_name: "action"       # 不同的输出节点名称
```

### 示例3：使用默认模型

```yaml
use_custom_onnx_model: false  # 使用默认模型
```

---

## 常见问题

### Q1: 如何知道我的模型输入输出节点名称？

**A**: 可以使用以下方法：
1. 使用ONNX工具查看：
   ```bash
   python -c "import onnx; model = onnx.load('your_model.onnx'); print([node.name for node in model.graph.input])"
   ```
2. 使用Netron可视化工具打开ONNX模型文件

### Q2: 可以同时使用两套模型吗？

**A**: 当前实现只支持使用一套模型（根据配置选择）。如果需要同时使用两套模型（例如A/B测试），需要修改`rl_control()`函数来实现。

### Q3: 模型加载失败怎么办？

**A**: 
1. 检查模型文件路径是否正确
2. 检查模型文件是否存在
3. 检查模型文件是否损坏
4. 查看程序输出的错误信息

### Q4: 如何切换回默认模型？

**A**: 在`config.yaml`中设置 `use_custom_onnx_model: false` 即可。

---

## 总结

通过本指南，您可以：
- ✅ 保留原有的ONNX推理框架
- ✅ 添加您自己训练的ONNX模型
- ✅ 通过配置文件轻松切换使用哪套模型
- ✅ 支持不同的输入输出节点名称
- ✅ 支持不同的模型维度

**建议**：在正式使用前，先在测试环境中验证您的模型是否正常工作。

---

**文档版本**：v1.0  
**最后更新**：2025-01-06

