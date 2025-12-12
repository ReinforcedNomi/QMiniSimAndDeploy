# ONNX 模型规模分析

## 一、基本信息

### 1.1 模型文件信息

```
文件路径: policy.onnx
文件大小: 787 KB (约 0.77 MB)
格式: ONNX (Open Neural Network Exchange)
来源: PyTorch 2.0.0 导出
```

### 1.2 输入输出维度（从 config.yaml 和代码分析）

```yaml
输入维度配置:
  - 单帧观测维度: 43 (input_dim)
  - 堆叠帧数: 3 (stack_dim)
  - 实际输入维度: 43 × 3 = 129维

输出维度配置:
  - 动作输出维度: 12 (output_dim)
    - 前2维: 步态相位频率 [0.5~3.5 Hz]
    - 后10维: 关节位置增量 [-15~15 rad/s]
```

### 1.3 输入输出张量形状

```cpp
输入张量: [1, 129]
  - batch_size: 1 (单样本推理)
  - feature_dim: 129 (堆叠后的观测向量)

输出张量: [1, 12] 或 [12]
  - 动作向量维度: 12
  - 值域: [-1, 1] (tanh激活函数输出)
```

---

## 二、网络结构推测

基于强化学习策略网络的常见设计模式和模型大小（787KB），推测网络结构如下：

### 2.1 典型RL策略网络结构

```
输入层: 129维 (43×3堆叠观测)
  ↓
隐藏层1: 256-512维 (全连接层 + 激活函数)
  ↓
隐藏层2: 256-512维 (全连接层 + 激活函数)
  ↓
隐藏层3: 128-256维 (全连接层 + 激活函数, 可选)
  ↓
输出层: 12维 (全连接层 + tanh激活)
```

### 2.2 参数量估算

基于 787KB 文件大小，假设使用 FP32（4字节/参数）：

```
总参数量 ≈ 787KB / 4字节 ≈ 197,000 个参数
```

**参数分布估算**：

```
假设结构: 129 → 512 → 256 → 12

输入到隐藏层1:
  参数数 = 129 × 512 + 512 = 66,560

隐藏层1到隐藏层2:
  参数数 = 512 × 256 + 256 = 131,328

隐藏层2到输出层:
  参数数 = 256 × 12 + 12 = 3,084

总计: ≈ 200,972 个参数
文件大小: ≈ 200,972 × 4字节 ≈ 804KB ✓ (与实际787KB接近)
```

### 2.3 可能的网络变体

#### 变体1：三层全连接网络（最可能）
```
Input(129) → FC(512) → ReLU → Dropout(0.1)
         → FC(256) → ReLU → Dropout(0.1)
         → FC(12) → Tanh
```
- 参数量: ~200K
- 文件大小: ~800KB ✓

#### 变体2：四层全连接网络（较深）
```
Input(129) → FC(256) → ReLU
         → FC(256) → ReLU
         → FC(128) → ReLU
         → FC(12) → Tanh
```
- 参数量: ~170K
- 文件大小: ~680KB

#### 变体3：更宽的网络
```
Input(129) → FC(1024) → ReLU
         → FC(512) → ReLU
         → FC(12) → Tanh
```
- 参数量: ~350K
- 文件大小: ~1.4MB (超过实际大小)

---

## 三、模型规模对比

### 3.1 与常见模型对比

| 模型类型 | 参数量 | 文件大小 | 应用场景 |
|---------|--------|----------|----------|
| **本模型 (policy.onnx)** | **~200K** | **787KB** | **机器人强化学习策略** |
| LeNet-5 | ~60K | ~240KB | 手写数字识别 |
| MobileNet-V1 | 4.2M | ~17MB | 移动端图像分类 |
| ResNet-18 | 11.7M | ~47MB | 图像分类 |
| GPT-2 Small | 117M | ~468MB | 语言模型 |
| GPT-3 | 175B | ~700GB | 大语言模型 |

### 3.2 典型RL策略网络规模

| 任务类型 | 观测维度 | 动作维度 | 典型参数量 | 典型大小 |
|---------|---------|---------|-----------|---------|
| 简单控制任务 | 10-20 | 1-4 | 10K-50K | 40-200KB |
| 机器人运动控制 | 30-100 | 6-20 | 100K-500K | 400KB-2MB |
| **本模型 (QMini)** | **129** | **12** | **~200K** | **787KB** |
| 复杂游戏AI | 100-500 | 10-50 | 500K-5M | 2-20MB |

**结论**：本模型属于中等规模的RL策略网络，适合实时控制场景。

---

## 四、推理性能分析

### 4.1 计算复杂度

假设网络结构：129 → 512 → 256 → 12

```
前向传播计算量（FLOPs）:
  129×512 + 512×256 + 256×12 
  = 66,048 + 131,072 + 3,072
  = 200,192 FLOPs ≈ 0.2 MFLOPs
```

### 4.2 内存占用

```
推理时内存占用:
  - 输入张量: 129 × 4字节 = 516字节
  - 中间激活值: (512 + 256) × 4字节 ≈ 3KB
  - 输出张量: 12 × 4字节 = 48字节
  - 权重加载: 787KB
  - 总计: ~790KB (非常小，适合嵌入式设备)
```

### 4.3 推理时间估算

基于 ONNX Runtime CPU 推理：

```
典型推理时间:
  - CPU (Intel i7): ~0.1-0.5ms
  - ARM CPU (Jetson): ~1-3ms
  - 满足实时控制需求 (15ms控制周期)
```

---

## 五、模型优化空间

### 5.1 模型量化

#### FP32 → FP16 (半精度)
```
理论压缩比: 2×
文件大小: 787KB → ~394KB
精度损失: 通常可忽略
推理速度: 提升 1.5-2×
```

#### FP32 → INT8 (8位整数)
```
理论压缩比: 4×
文件大小: 787KB → ~197KB
精度损失: 需要校准，可能有轻微损失
推理速度: 提升 2-4×
```

### 5.2 模型剪枝

```
剪枝率: 20-50%
文件大小: 787KB → 630-394KB
推理速度: 提升 1.2-2×
精度影响: 需要评估
```

### 5.3 知识蒸馏

```
目标: 训练更小的学生模型
目标大小: 100-200KB
参数量: 50-100K
精度保持: 可能略降 5-10%
```

---

## 六、详细网络结构查看方法

### 6.1 使用 Python ONNX 库查看

```python
import onnx

# 加载模型
model = onnx.load('policy.onnx')

# 查看输入输出
print("输入:")
for input_tensor in model.graph.input:
    print(f"  {input_tensor.name}: {[d.dim_value for d in input_tensor.type.tensor_type.shape.dim]}")

print("\n输出:")
for output_tensor in model.graph.output:
    print(f"  {output_tensor.name}: {[d.dim_value for d in output_tensor.type.tensor_type.shape.dim]}")

# 查看所有节点
print("\n节点数:", len(model.graph.node))
print("\n权重节点数:", len(model.graph.initializer))

# 查看网络结构（简化版）
print("\n网络结构:")
for i, node in enumerate(model.graph.node[:10]):  # 前10个节点
    print(f"  {i}: {node.op_type} - {node.name}")
```

### 6.2 使用 Netron 可视化工具

```bash
# 安装 Netron
pip install netron

# 启动可视化服务器
netron policy.onnx

# 然后在浏览器中打开 http://localhost:8080
```

### 6.3 使用 ONNX Runtime 查看

```python
import onnxruntime as ort

# 创建推理会话
session = ort.InferenceSession('policy.onnx')

# 查看输入输出信息
print("输入:")
for input_meta in session.get_inputs():
    print(f"  {input_meta.name}: shape={input_meta.shape}, type={input_meta.type}")

print("\n输出:")
for output_meta in session.get_outputs():
    print(f"  {output_meta.name}: shape={output_meta.shape}, type={output_meta.type}")
```

---

## 七、模型规模总结

### 7.1 当前模型特点

| 特性 | 数值 | 评价 |
|------|------|------|
| **文件大小** | **787KB** | ✅ 适中，适合部署 |
| **参数量** | **~200K** | ✅ 中等规模 |
| **输入维度** | **129** | ✅ 适合RL任务 |
| **输出维度** | **12** | ✅ 匹配10关节+2频率 |
| **推理速度** | **<3ms** | ✅ 满足实时需求 |
| **内存占用** | **<1MB** | ✅ 适合嵌入式设备 |

### 7.2 适用场景

✅ **适合**：
- 实时机器人控制（15ms控制周期）
- 嵌入式设备部署（Jetson、树莓派等）
- 边缘计算场景
- 需要快速推理的应用

❌ **不适合**：
- 需要极高精度的复杂任务
- 需要处理大规模输入的场景
- 需要多任务学习的场景

### 7.3 优化建议

1. **如果部署在资源受限设备**：
   - 考虑 INT8 量化（文件大小降至 ~200KB）
   - 考虑模型剪枝（减少 20-30% 参数）

2. **如果需要更高精度**：
   - 可以增大网络宽度（512→1024）
   - 可以增加网络深度（3层→4层）
   - 但会增加文件大小和推理时间

3. **如果需要更快推理**：
   - 使用 GPU 加速（如果硬件支持）
   - 使用 INT8 量化
   - 使用 ONNX Runtime 的优化选项

---

## 八、实际验证方法

### 8.1 在代码中打印模型信息

可以在 `rl_controller.cpp` 的 `init()` 函数中添加：

```cpp
void RLController::init() {
    // ... 现有代码 ...
    
    // 打印模型信息
    cout << "=== ONNX Model Info ===" << endl;
    cout << "Input dimension: " << onnxInference.input_dim << endl;
    cout << "Output dimension: " << onnxInference.output_dim << endl;
    cout << "Stack dimension: " << onnxInference.stack_dim << endl;
    cout << "Total input size: " << onnxInference.input_dim * onnxInference.stack_dim << endl;
    cout << "Model file: policy.onnx" << endl;
    cout << "=========================" << endl;
}
```

### 8.2 测量推理时间

```cpp
void RLController::rl_control() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // ONNX推理
    net_out = onnxInference.inference(motion_session, get_observation());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (counter_rl % 100 == 0) {
        cout << "Inference time: " << duration.count() << " us" << endl;
    }
    
    // ... 其余代码 ...
}
```

---

## 附录：常用命令

### 查看模型文件信息

```bash
# 查看文件大小
ls -lh policy.onnx

# 查看文件类型
file policy.onnx

# 查看文件头部（验证ONNX格式）
head -c 100 policy.onnx | xxd
```

### Python 环境安装（用于分析）

```bash
# 安装 ONNX 相关库
pip install onnx onnxruntime

# 安装可视化工具
pip install netron
```

