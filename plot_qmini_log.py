#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
读取 qmini_log.log 文件，提取 Q 和 N 数据，绘制波形图
Q: 当前关节位置（蓝色）
N: 目标关节位置（红色）
"""

import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams
import sys
import os

# 设置中文字体支持
rcParams['font.sans-serif'] = ['DejaVu Sans', 'SimHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

def parse_log_file(log_file_path):
    """
    解析日志文件，提取 Q 和 N 数据
    
    Args:
        log_file_path: 日志文件路径
    
    Returns:
        q_data: Q数据列表，每个元素是一个包含10个关节位置的列表
        n_data: N数据列表，每个元素是一个包含10个关节位置的列表
    """
    q_data = []
    n_data = []
    
    # Q 和 N 的正则表达式
    q_pattern = re.compile(r'Q:\s*\[\s*([-\d\.\s,]+)\s*\]')
    n_pattern = re.compile(r'N:\s*\[\s*([-\d\.\s,]+)\s*\]')
    
    try:
        with open(log_file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                
                # 匹配 Q 数据
                q_match = q_pattern.search(line)
                if q_match:
                    q_str = q_match.group(1)
                    q_values = [float(x.strip()) for x in q_str.split(',') if x.strip()]
                    if len(q_values) == 10:
                        q_data.append(q_values)
                
                # 匹配 N 数据
                n_match = n_pattern.search(line)
                if n_match:
                    n_str = n_match.group(1)
                    n_values = [float(x.strip()) for x in n_str.split(',') if x.strip()]
                    if len(n_values) == 10:
                        n_data.append(n_values)
    
    except FileNotFoundError:
        print(f"错误: 找不到文件 {log_file_path}")
        return None, None
    except Exception as e:
        print(f"错误: 读取文件时出错: {e}")
        return None, None
    
    return q_data, n_data

def plot_joint_data(q_data, n_data, save_path=None):
    """
    绘制关节位置波形图
    
    Args:
        q_data: Q数据列表（当前位置）
        n_data: N数据列表（目标位置）
        save_path: 保存图片的路径（可选）
    """
    if not q_data or not n_data:
        print("错误: 没有数据可绘制")
        return
    
    # 转换为 numpy 数组
    q_array = np.array(q_data)
    n_array = np.array(n_data)
    
    # 确保 Q 和 N 数据长度一致（取较小值）
    min_len = min(len(q_array), len(n_array))
    q_array = q_array[:min_len]
    n_array = n_array[:min_len]
    
    # 时间轴（0.2秒间隔）
    time_axis = np.arange(min_len) * 0.2
    
    # 关节名称
    joint_names = [f'Joint {i}' for i in range(10)]
    
    # 创建子图：2行5列，显示10个关节
    fig, axes = plt.subplots(2, 5, figsize=(20, 8))
    fig.suptitle('QMini Joint Positions: Q (Current) vs N (Target)', fontsize=16, fontweight='bold')
    
    # 绘制每个关节的波形
    for i in range(10):
        row = i // 5
        col = i % 5
        ax = axes[row, col]
        
        # 绘制 Q 数据（当前位置，蓝色实线）
        ax.plot(time_axis, q_array[:, i], 'b-', label='Q (Current)', linewidth=1.5, alpha=0.8)
        
        # 绘制 N 数据（目标位置，红色虚线）
        ax.plot(time_axis, n_array[:, i], 'r--', label='N (Target)', linewidth=1.5, alpha=0.8)
        
        # 设置标题和标签
        ax.set_title(f'{joint_names[i]}', fontsize=12, fontweight='bold')
        ax.set_xlabel('Time (s)', fontsize=10)
        ax.set_ylabel('Position (rad)', fontsize=10)
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best', fontsize=9)
    
    # 调整布局
    plt.tight_layout()
    
    # 保存图片
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"图片已保存到: {save_path}")
    
    # 显示图片
    plt.show()

def plot_all_joints_together(q_data, n_data, save_path=None):
    """
    在一个图中绘制所有关节的波形（可选）
    
    Args:
        q_data: Q数据列表
        n_data: N数据列表
        save_path: 保存图片的路径（可选）
    """
    if not q_data or not n_data:
        return
    
    q_array = np.array(q_data)
    n_array = np.array(n_data)
    
    min_len = min(len(q_array), len(n_array))
    q_array = q_array[:min_len]
    n_array = n_array[:min_len]
    
    time_axis = np.arange(min_len) * 0.2
    
    plt.figure(figsize=(16, 10))
    
    # 绘制所有关节
    for i in range(10):
        plt.plot(time_axis, q_array[:, i], 'b-', label=f'Q Joint {i}' if i < 5 else '', 
                linewidth=1.5, alpha=0.7)
        plt.plot(time_axis, n_array[:, i], 'r--', label=f'N Joint {i}' if i < 5 else '', 
                linewidth=1.5, alpha=0.7)
    
    plt.title('All Joints: Q (Current) vs N (Target)', fontsize=16, fontweight='bold')
    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Position (rad)', fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.legend(loc='best', fontsize=9, ncol=2)
    
    if save_path:
        save_path_all = save_path.replace('.png', '_all_joints.png')
        plt.savefig(save_path_all, dpi=150, bbox_inches='tight')
        print(f"所有关节图片已保存到: {save_path_all}")
    
    plt.show()

def main():
    """主函数"""
    # 默认日志文件路径
    default_log_path = '/home/qmini/Programs/QMiniSimAndDeploy/qmini_log.log'
    
    # 检查命令行参数
    if len(sys.argv) > 1:
        log_file_path = sys.argv[1]
    else:
        log_file_path = default_log_path
    
    # 检查文件是否存在
    if not os.path.exists(log_file_path):
        print(f"错误: 文件不存在: {log_file_path}")
        print(f"使用方法: python {sys.argv[0]} [log_file_path]")
        return
    
    print(f"正在读取日志文件: {log_file_path}")
    
    # 解析日志文件
    q_data, n_data = parse_log_file(log_file_path)
    
    if q_data is None or n_data is None:
        return
    
    print(f"成功读取 {len(q_data)} 条 Q 数据")
    print(f"成功读取 {len(n_data)} 条 N 数据")
    
    if len(q_data) == 0 or len(n_data) == 0:
        print("警告: 没有找到有效数据")
        return
    
    # 保存路径
    save_path = log_file_path.replace('.log', '_joint_plot.png')
    
    # 绘制分关节图
    print("正在绘制分关节波形图...")
    plot_joint_data(q_data, n_data, save_path)
    
    # 绘制所有关节在一张图上（可选，注释掉以禁用）
    # print("正在绘制所有关节波形图...")
    # plot_all_joints_together(q_data, n_data, save_path)

if __name__ == '__main__':
    main()

