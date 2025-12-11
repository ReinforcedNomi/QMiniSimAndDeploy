//
// Created by cyy on 2020/12/27.
//

#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include "onnx/onnxruntime_cxx_api.h"
#include <ctime>
#include <sys/time.h>
#include <eigen3/Eigen/Dense>
#include <assert.h>
using namespace Eigen;
using namespace std;

/**
 * @class OnnxInference
 * @brief ONNX模型推理封装类，负责执行ONNX模型的前向传播
 * 
 * 功能：
 * 1. 初始化推理器，设置输入/输出维度
 * 2. 将Eigen向量转换为ONNX Runtime张量格式
 * 3. 执行模型推理
 * 4. 将输出张量转换回Eigen向量格式
 * 
 * 使用流程：
 *   1. 调用init()初始化维度参数
 *   2. 在控制循环中调用inference()执行推理
 */
class OnnxInference {
public:
    explicit OnnxInference() = default;

public:
    int64_t input_dim = 0;    // 单帧观测维度（从config.yaml读取，默认43）
    int64_t output_dim = 0;   // 动作输出维度（从config.yaml读取，默认12）
    int64_t stack_dim = 0;     // 观测堆叠帧数（从config.yaml读取，默认3）

private:
    std::string _modelPath;    // 模型文件路径（当前未使用，模型在RLController中加载）
    
    // ONNX模型输入/输出节点名称（需与模型定义一致）
    std::vector<const char *> input_node_names = {"input"};   // 输入节点名称
    std::vector<const char *> output_node_names = {"output"}; // 输出节点名称
    
    std::vector<int64_t> input_node_dims;  // 输入张量维度 [batch_size, feature_dim]
    size_t input_tensor_size = 0;         // 输入张量总元素个数

public:
    /**
     * @brief 初始化ONNX推理器，设置输入/输出维度
     * 
     * @param obs_space 单帧观测维度（默认43）
     * @param act_space 动作输出维度（默认12）
     * @param stack_space 观测堆叠帧数（默认3）
     * 
     * 输入张量维度：
     *   - batch_size: 1（单样本推理）
     *   - feature_dim: obs_space * stack_space（堆叠后的特征维度）
     *   - 例如：43 * 3 = 129
     */
    void init(int obs_space, int act_space, int stack_space) {
        input_dim = obs_space;      // 单帧观测维度
        output_dim = act_space;     // 动作输出维度
        stack_dim = stack_space;     // 堆叠帧数
        
        // 设置输入张量维度：[batch_size=1, feature_dim=obs_space*stack_space]
        // 例如：[1, 129] 表示1个样本，每个样本129维特征
        input_node_dims = {1, input_dim * stack_dim};
        
        // 计算输入张量总元素个数（用于内存分配）
        input_tensor_size = input_node_dims.at(0) * input_node_dims.at(1);
        //        cout << "input_tensor_size: " << input_tensor_size << endl;
    }

    /**
     * @brief 执行ONNX模型推理
     * 
     * 推理流程：
     * 1. 将Eigen观测向量转换为std::vector<float>
     * 2. 创建ONNX Runtime内存信息和输入张量
     * 3. 调用session->Run()执行模型前向传播
     * 4. 从输出张量提取数据并转换为Eigen向量
     * 5. 对输出进行裁剪，确保值域在[-1, 1]
     * 
     * @param session ONNX Runtime会话对象（包含已加载的模型）
     * @param observation 输入观测向量，维度为 (input_dim * stack_dim)
     * @return 模型输出动作向量，维度为 output_dim，值域为[-1, 1]
     */
    Matrix<float, Dynamic, 1> inference(Ort::Session *session, Matrix<float, Dynamic, 1> observation) {
        // ========== 步骤1：准备输入数据 ==========
        // 将Eigen向量转换为std::vector，便于ONNX Runtime处理
        std::vector<float> input_tensor_values(input_tensor_size);
        for (unsigned int i = 0; i < input_tensor_size; i++)
            input_tensor_values[i] = observation[i];
        
        // ========== 步骤2：创建ONNX Runtime内存信息 ==========
        // 指定使用CPU内存，使用Arena分配器（性能优化）
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        // ========== 步骤3：创建输入张量 ==========
        // 将C++数组包装为ONNX Runtime张量对象
        // 参数说明：
        //   - memory_info: 内存信息
        //   - input_tensor_values.data(): 数据指针
        //   - input_tensor_size: 数据元素个数
        //   - input_node_dims.data(): 张量维度数组
        //   - 2: 张量维度数（2D: [batch, features]）
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, input_tensor_values.data(),
                                                                  input_tensor_size, input_node_dims.data(), 2);
        
        // ========== 步骤4：准备输入向量 ==========
        // ONNX Runtime需要std::vector<Ort::Value>格式的输入
        std::vector<Ort::Value> ort_inputs;
        ort_inputs.push_back(std::move(input_tensor));  // 使用move避免拷贝
        
        // ========== 步骤5：执行模型推理 ==========
        // 调用ONNX Runtime执行前向传播
        // 参数说明：
        //   - Ort::RunOptions{nullptr}: 运行选项（使用默认）
        //   - input_node_names.data(): 输入节点名称数组
        //   - ort_inputs.data(): 输入张量数组
        //   - ort_inputs.size(): 输入数量（通常为1）
        //   - output_node_names.data(): 输出节点名称数组
        //   - 1: 输出数量（通常为1）
        auto output_tensors = session->Run(Ort::RunOptions{nullptr}, input_node_names.data(), ort_inputs.data(),
                                           ort_inputs.size(), output_node_names.data(), 1);
        
        // ========== 步骤6：提取输出数据 ==========
        // 从输出张量中获取原始数据指针
        float *outputData = output_tensors[0].GetTensorMutableData<float>();
        
        // 使用Eigen::Map将原始数组映射为Eigen向量（零拷贝）
        // 参数：数据指针，行数（output_dim），列数（1）
        Eigen::Map<Eigen::Matrix<float, -1, 1> > net_out(outputData, output_dim, 1);
        
        // ========== 步骤7：输出后处理 ==========
        // 将输出裁剪到[-1, 1]范围（防止模型输出异常值）
        // 这通常对应tanh激活函数的输出范围
        auto net_out_action = net_out.cwiseMax(-1).cwiseMin(1.);
        
        // cout << "net_true_action: " << net_out_action.transpose() << endl;
        return net_out_action;
    }
};
