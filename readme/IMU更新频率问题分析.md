# IMU更新频率问题分析

## 问题现象
- **预期频率**：IMU线程设置为333Hz（每3ms调用一次）
- **实际频率**：日志中IMU数据更新频率只有约5-10Hz
- **日志证据**：IMU数据在日志中每5行出现一次（日志打印频率50Hz），实际更新频率 = 50/5 = 10Hz

## 根本原因分析

### 1. **每次调用都重新打开串口** ⭐ 主要原因
`imu_receiver.py`中的`read_imu_data()`函数每次调用时都会创建新的`serial.Serial()`对象：

```python
def read_imu_data(port="...", baudrate=921600, timeout=1):
    serial_ = serial.Serial(port=port, baudrate=baudrate, ...)  # 每次调用都重新打开
    ...
```

**问题**：
- 串口初始化需要时间（通常几十到几百毫秒）
- 重新打开串口时可能丢失缓冲区中的数据
- 需要重新同步数据包帧头，导致延迟

### 2. **需要等待两个数据包才能返回**
函数必须等待`TYPE_IMU`和`TYPE_AHRS`两个数据包都到达才能返回：

```python
if temp1 is True and temp2 is True:  # 必须两个包都收到
    return result
```

**问题**：
- 如果硬件发送这两个数据包的频率不够高，或者发送顺序不规律，就会导致阻塞
- 每次调用时`temp1`和`temp2`都会重置为`False`，必须从头开始等待

### 3. **串口读取阻塞**
`serial_.read()`在`timeout=1`秒的情况下，如果没有数据会阻塞最多1秒：

```python
timeout=1  # 每次读取最多阻塞1秒
check_head = serial_.read().hex()  # 可能阻塞
```

**问题**：
- 如果帧头不匹配（第88行），会`continue`继续循环，但下一次`read()`可能再次阻塞
- 在等待匹配数据包时，会不断阻塞等待

### 4. **while循环中的阻塞读取**
函数在`while serial_.isOpen()`循环中不断读取数据，直到收到两个匹配的数据包：

```python
while serial_.isOpen():
    check_head = serial_.read().hex()  # 阻塞读取
    if check_head != FRAME_HEAD:
        continue  # 继续循环，可能再次阻塞
    ...
```

**问题**：
- 如果硬件发送频率低，或者数据包格式不匹配，会不断阻塞等待
- 每次调用函数时，都需要从头开始等待数据包

### 5. **IMU线程频率与串口读取不匹配**
- **线程频率**：333Hz（每3ms调用一次）
- **串口读取耗时**：每次调用可能需要几十到几百毫秒才能返回
- **结果**：实际更新频率远低于线程频率

## 性能影响估算

假设：
- 串口初始化：50ms
- 等待第一个数据包（TYPE_IMU）：50-200ms（取决于硬件发送频率）
- 等待第二个数据包（TYPE_AHRS）：50-200ms
- **总耗时**：150-450ms

**实际更新频率**：1 / 0.15s = 6.7Hz 到 1 / 0.45s = 2.2Hz

这与观察到的5Hz左右的实际频率相符！

## 解决方案建议

### 方案1：复用串口连接（推荐）
将串口对象作为全局变量或类成员，只初始化一次：

```python
# 全局串口对象
_serial_port = None

def read_imu_data(port="...", baudrate=921600, timeout=1):
    global _serial_port
    if _serial_port is None:
        _serial_port = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
    # 使用已有的串口对象
    ...
```

### 方案2：使用非阻塞读取 + 缓冲区
使用`timeout=0`（非阻塞）或较小的timeout，配合数据缓冲区：

```python
serial_ = serial.Serial(port=port, baudrate=baudrate, timeout=0.01)  # 10ms超时
```

### 方案3：独立线程持续读取
创建一个独立的线程持续读取串口数据，将数据存入缓冲区，`read_imu_data()`只从缓冲区读取最新数据。

### 方案4：降低IMU线程频率
如果硬件发送频率本身就不高，可以降低IMU线程频率以匹配实际能力：

```cpp
// 从333Hz降低到50Hz或100Hz
imu_thread_ptr_ = CreateRecurrentThreadEx("imu", UT_CPU_ID_NONE, 0.02 * 1e6,  // 20ms = 50Hz
                                          &G1::IMUStateReader, this);
```

## 推荐实施顺序

1. **立即实施**：方案1（复用串口连接）- 最简单且效果明显
2. **进一步优化**：方案2（非阻塞读取）- 减少阻塞时间
3. **长期优化**：方案3（独立线程）- 最佳性能，但需要重构代码

