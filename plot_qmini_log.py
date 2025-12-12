#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
实时读取 qmini_log.log 文件，提取 Q、N、F、E 数据，绘制实时波形图
Q: 当前关节位置（蓝色）
N: 目标关节位置（红色）
F: 实时输出扭矩（绿色）
E: 错误码（仅在有关节报错时输出，用红色点标记）

实时显示最近30秒的数据，自动更新
"""

import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams
from matplotlib.animation import FuncAnimation
import sys
import os
from datetime import datetime
import time

# 设置中文字体支持
rcParams['font.sans-serif'] = ['DejaVu Sans', 'SimHei', 'Arial Unicode MS']
rcParams['axes.unicode_minus'] = False

# 全局变量：存储已读取的行数，用于增量读取
_last_file_position = 0
# 数据缓存：存储最近30秒的数据
_data_cache = {
    'q_data': [],
    'n_data': [],
    'f_data': [],
    'e_data': [],
    'timestamps': []  # 记录每条数据的时间戳
}
# 数据更新频率：每秒1次（与日志输出频率一致）
DATA_UPDATE_RATE = 1.0  # Hz
WINDOW_DURATION = 30.0  # 秒，显示窗口时长

def parse_log_file_incremental(log_file_path):
    """
    增量解析日志文件，只读取新增的数据
    
    Args:
        log_file_path: 日志文件路径
    
    Returns:
        新增的数据字典，包含 q_data, n_data, f_data, e_data
    """
    global _last_file_position
    
    new_data = {
        'q_data': [],
        'n_data': [],
        'f_data': [],
        'e_data': []
    }
    
    # Q、N、F、E 的正则表达式
    q_pattern = re.compile(r'Q:\s*\[\s*([-\d\.\s,]+)\s*\]')
    n_pattern = re.compile(r'N:\s*\[\s*([-\d\.\s,]+)\s*\]')
    f_pattern = re.compile(r'F:\s*\[\s*([-\d\.\s,]+)\s*\]')
    e_pattern = re.compile(r'E:\s*\[\s*([-\d\.\s,]+)\s*\]')
    
    try:
        with open(log_file_path, 'r', encoding='utf-8') as f:
            # 移动到上次读取的位置
            f.seek(_last_file_position)
            
            for line in f:
                line = line.strip()
                
                # 匹配 Q 数据
                q_match = q_pattern.search(line)
                if q_match:
                    q_str = q_match.group(1)
                    q_values = [float(x.strip()) for x in q_str.split(',') if x.strip()]
                    if len(q_values) == 10:
                        new_data['q_data'].append(q_values)
                
                # 匹配 N 数据
                n_match = n_pattern.search(line)
                if n_match:
                    n_str = n_match.group(1)
                    n_values = [float(x.strip()) for x in n_str.split(',') if x.strip()]
                    if len(n_values) == 10:
                        new_data['n_data'].append(n_values)
                
                # 匹配 F 数据（扭矩）
                f_match = f_pattern.search(line)
                if f_match:
                    f_str = f_match.group(1)
                    f_values = [float(x.strip()) for x in f_str.split(',') if x.strip()]
                    if len(f_values) == 10:
                        new_data['f_data'].append(f_values)
                
                # 匹配 E 数据（错误码）
                e_match = e_pattern.search(line)
                if e_match:
                    e_str = e_match.group(1)
                    e_values = [int(x.strip()) for x in e_str.split(',') if x.strip()]
                    if len(e_values) == 10:
                        new_data['e_data'].append(e_values)
            
            # 更新文件位置
            _last_file_position = f.tell()
    
    except FileNotFoundError:
        print(f"错误: 找不到文件 {log_file_path}")
        return None
    except Exception as e:
        print(f"错误: 读取文件时出错: {e}")
        return None
    
    return new_data

def parse_log_file(log_file_path):
    """
    解析日志文件，提取 Q、N、F、E 数据
    
    Args:
        log_file_path: 日志文件路径
    
    Returns:
        q_data: Q数据列表，每个元素是一个包含10个关节位置的列表
        n_data: N数据列表，每个元素是一个包含10个关节位置的列表
        f_data: F数据列表，每个元素是一个包含10个关节扭矩的列表
        e_data: E数据列表，每个元素是一个包含10个关节错误码的列表（可能为空）
    """
    q_data = []
    n_data = []
    f_data = []
    e_data = []
    
    # Q、N、F、E 的正则表达式
    q_pattern = re.compile(r'Q:\s*\[\s*([-\d\.\s,]+)\s*\]')
    n_pattern = re.compile(r'N:\s*\[\s*([-\d\.\s,]+)\s*\]')
    f_pattern = re.compile(r'F:\s*\[\s*([-\d\.\s,]+)\s*\]')
    e_pattern = re.compile(r'E:\s*\[\s*([-\d\.\s,]+)\s*\]')
    
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
                
                # 匹配 F 数据（扭矩）
                f_match = f_pattern.search(line)
                if f_match:
                    f_str = f_match.group(1)
                    f_values = [float(x.strip()) for x in f_str.split(',') if x.strip()]
                    if len(f_values) == 10:
                        f_data.append(f_values)
                
                # 匹配 E 数据（错误码）
                e_match = e_pattern.search(line)
                if e_match:
                    e_str = e_match.group(1)
                    e_values = [int(x.strip()) for x in e_str.split(',') if x.strip()]
                    if len(e_values) == 10:
                        e_data.append(e_values)
    
    except FileNotFoundError:
        print(f"错误: 找不到文件 {log_file_path}")
        return None, None, None, None
    except Exception as e:
        print(f"错误: 读取文件时出错: {e}")
        return None, None, None, None
    
    return q_data, n_data, f_data, e_data

def update_data_cache(new_data, current_time):
    """
    更新数据缓存，保持最近30秒的数据
    
    Args:
        new_data: 新增的数据字典
        current_time: 当前时间戳
    """
    global _data_cache
    
    # Q和N数据是配对的，使用相同的时间戳
    # 确保Q和N数据同步
    q_count = len(new_data.get('q_data', [])) if new_data else 0
    n_count = len(new_data.get('n_data', [])) if new_data else 0
    
    # 添加新数据，Q和N配对
    pair_count = min(q_count, n_count)
    for i in range(pair_count):
        if new_data.get('q_data') and i < len(new_data['q_data']):
            _data_cache['q_data'].append(new_data['q_data'][i])
        if new_data.get('n_data') and i < len(new_data['n_data']):
            _data_cache['n_data'].append(new_data['n_data'][i])
        # 使用当前时间戳（精确到秒，因为日志每秒输出一次）
        _data_cache['timestamps'].append(current_time)
    
    # F数据（扭矩）
    if new_data and new_data.get('f_data'):
        for f_val in new_data['f_data']:
            _data_cache['f_data'].append(f_val)
    
    # E数据（错误码）
    if new_data and new_data.get('e_data'):
        for e_val in new_data['e_data']:
            _data_cache['e_data'].append(e_val)
    
    # 移除超过30秒的旧数据
    if len(_data_cache['timestamps']) > 0:
        cutoff_time = current_time - WINDOW_DURATION
        
        # 找到需要保留的数据起始索引
        keep_start_idx = 0
        for i, ts in enumerate(_data_cache['timestamps']):
            if ts >= cutoff_time:
                keep_start_idx = i
                break
        else:
            # 所有数据都过期了，保留最后一个（如果有）
            keep_start_idx = max(0, len(_data_cache['timestamps']) - 1)
        
        # 移除过期数据
        if keep_start_idx > 0:
            _data_cache['q_data'] = _data_cache['q_data'][keep_start_idx:]
            _data_cache['n_data'] = _data_cache['n_data'][keep_start_idx:]
            _data_cache['timestamps'] = _data_cache['timestamps'][keep_start_idx:]
            # F和E数据可能长度不同，需要分别处理
            if len(_data_cache['f_data']) > keep_start_idx:
                _data_cache['f_data'] = _data_cache['f_data'][keep_start_idx:]
            if len(_data_cache['e_data']) > keep_start_idx:
                _data_cache['e_data'] = _data_cache['e_data'][keep_start_idx:]
    
    # 确保Q、N和timestamps长度一致
    min_len = min(len(_data_cache['q_data']), len(_data_cache['n_data']), len(_data_cache['timestamps']))
    if min_len < len(_data_cache['q_data']):
        _data_cache['q_data'] = _data_cache['q_data'][:min_len]
    if min_len < len(_data_cache['n_data']):
        _data_cache['n_data'] = _data_cache['n_data'][:min_len]
    if min_len < len(_data_cache['timestamps']):
        _data_cache['timestamps'] = _data_cache['timestamps'][:min_len]

def plot_joint_data(q_data, n_data, f_data=None, e_data=None, save_path=None):
    """
    绘制关节位置波形图
    
    Args:
        q_data: Q数据列表（当前位置）
        n_data: N数据列表（目标位置）
        f_data: F数据列表（实时扭矩，可选）
        e_data: E数据列表（错误码，可选）
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
    
    # 处理 F 数据（扭矩）
    f_array = None
    if f_data and len(f_data) > 0:
        f_array = np.array(f_data)
        if len(f_array) > min_len:
            f_array = f_array[:min_len]
        elif len(f_array) < min_len:
            # 如果F数据不足，用NaN填充
            f_padded = np.full((min_len, 10), np.nan)
            f_padded[:len(f_array)] = f_array
            f_array = f_padded
    
    # 处理 E 数据（错误码）
    e_array = None
    if e_data and len(e_data) > 0:
        e_array = np.array(e_data)
        if len(e_array) > min_len:
            e_array = e_array[:min_len]
        elif len(e_array) < min_len:
            # 如果E数据不足，用0填充（表示无错误）
            e_padded = np.zeros((min_len, 10), dtype=int)
            e_padded[:len(e_array)] = e_array
            e_array = e_padded
    
    # 时间轴（每秒1次，所以间隔1秒）
    time_axis = np.arange(min_len)
    
    # 关节名称
    joint_names = [f'Joint {i}' for i in range(10)]
    
    # 创建子图：10行3列，每行显示一个关节的位置、扭矩、错误码
    fig = plt.figure(figsize=(18, 24))
    
    # 为每个关节创建一行，包含3个子图（位置、扭矩、错误码）
    for i in range(10):
        # 第一列：位置（Q 和 N）
        ax1 = plt.subplot(10, 3, i * 3 + 1)
        ax1.plot(time_axis, q_array[:, i], 'b-', label='Q (Current)', linewidth=1.5, alpha=0.8)
        ax1.plot(time_axis, n_array[:, i], 'r--', label='N (Target)', linewidth=1.5, alpha=0.8)
        ax1.set_title(f'{joint_names[i]} - Position', fontsize=11, fontweight='bold')
        ax1.set_xlabel('Time (s)', fontsize=9)
        ax1.set_ylabel('Position (rad)', fontsize=9)
        ax1.grid(True, alpha=0.3)
        ax1.legend(loc='best', fontsize=8)
        
        # 第二列：扭矩（F）
        ax2 = plt.subplot(10, 3, i * 3 + 2)
        if f_array is not None:
            ax2.plot(time_axis, f_array[:, i], 'g-', label='F (Torque)', linewidth=1.5, alpha=0.8)
            ax2.axhline(y=0, color='k', linestyle=':', linewidth=0.5, alpha=0.5)
            ax2.legend(loc='best', fontsize=8)
        else:
            ax2.text(0.5, 0.5, 'No F data', ha='center', va='center', transform=ax2.transAxes)
        ax2.set_title(f'{joint_names[i]} - Torque', fontsize=11, fontweight='bold')
        ax2.set_xlabel('Time (s)', fontsize=9)
        ax2.set_ylabel('Torque (N·m)', fontsize=9)
        ax2.grid(True, alpha=0.3)
        
        # 第三列：错误码（E）
        ax3 = plt.subplot(10, 3, i * 3 + 3)
        if e_array is not None:
            error_mask = e_array[:, i] != 0
            if np.any(error_mask):
                error_times = time_axis[error_mask]
                error_codes = e_array[error_mask, i]
                # 用不同颜色标记不同错误码
                colors = ['red', 'orange', 'purple', 'brown', 'pink']
                for t, code in zip(error_times, error_codes):
                    color = colors[code % len(colors)] if code < len(colors) else 'red'
                    ax3.scatter(t, code, c=color, s=50, alpha=0.8, zorder=5)
                ax3.set_ylim(-0.5, 5.5)
                ax3.set_yticks([0, 1, 2, 3, 4, 5])
                ax3.set_yticklabels(['0:Normal', '1:Overheat', '2:Overcurrent', '3:Overvoltage', '4:Encoder', '5:Protect'])
            else:
                ax3.text(0.5, 0.5, 'No errors', ha='center', va='center', transform=ax3.transAxes, 
                       color='green', fontweight='bold')
                ax3.set_ylim(-0.5, 5.5)
                ax3.set_yticks([0])
                ax3.set_yticklabels(['0:Normal'])
        else:
            ax3.text(0.5, 0.5, 'No E data', ha='center', va='center', transform=ax3.transAxes)
            ax3.set_ylim(-0.5, 5.5)
            ax3.set_yticks([0])
            ax3.set_yticklabels(['0:Normal'])
        ax3.set_title(f'{joint_names[i]} - Error Code', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Time (s)', fontsize=9)
        ax3.set_ylabel('Error Code', fontsize=9)
        ax3.grid(True, alpha=0.3)
    
    # 添加总标题
    fig.suptitle('QMini Joint Data: Position (Q/N) | Torque (F) | Error Code (E)', 
                 fontsize=16, fontweight='bold', y=0.995)
    
    # 调整布局
    plt.tight_layout(rect=[0, 0, 1, 0.99])
    
    # 保存图片
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"图片已保存到: {save_path}")
    
    # 显示图片
    plt.show()

def update_real_time_plot(frame, log_file_path, axes_list, lines_dict):
    """
    实时更新图表的动画函数
    
    Args:
        frame: 动画帧数
        log_file_path: 日志文件路径
        axes_list: 子图列表
        lines_dict: 线条字典，存储所有需要更新的线条对象
    """
    global _data_cache
    
    # 获取当前时间
    current_time = time.time()
    
    # 增量读取新数据
    new_data = parse_log_file_incremental(log_file_path)
    
    # 更新数据缓存（保持30秒窗口）
    update_data_cache(new_data, current_time)
    
    # 获取当前缓存的数据
    q_data = _data_cache['q_data']
    n_data = _data_cache['n_data']
    f_data = _data_cache['f_data'] if _data_cache['f_data'] else None
    e_data = _data_cache['e_data'] if _data_cache['e_data'] else None
    timestamps = _data_cache['timestamps']
    
    if len(q_data) == 0 or len(n_data) == 0:
        return lines_dict
    
    # 转换为numpy数组
    min_len = min(len(q_data), len(n_data), len(timestamps))
    if min_len == 0:
        return lines_dict
    
    q_array = np.array(q_data[-min_len:])
    n_array = np.array(n_data[-min_len:])
    timestamps_array = np.array(timestamps[-min_len:])
    
    # 计算相对时间（相对于窗口开始，即最近30秒的起点）
    if len(timestamps_array) > 0:
        # 窗口开始时间 = 最新时间戳 - 窗口时长
        window_start_time = timestamps_array[-1] - WINDOW_DURATION
        # 计算每个数据点相对于窗口开始的时间
        time_axis = timestamps_array - window_start_time
        # 确保时间轴在[0, WINDOW_DURATION]范围内
        time_axis = np.clip(time_axis, 0, WINDOW_DURATION)
    else:
        # 如果没有时间戳，使用索引作为时间（假设每秒一次）
        time_axis = np.arange(min_len)
    
    # 处理F数据
    f_array = None
    if f_data and len(f_data) >= min_len:
        f_array = np.array(f_data[-min_len:])
    
    # 处理E数据
    e_array = None
    if e_data and len(e_data) >= min_len:
        e_array = np.array(e_data[-min_len:])
    
    # 更新每个关节的图表
    for i in range(10):
        # 更新位置图（第一列）
        if i * 3 + 1 in lines_dict:
            lines_dict[i * 3 + 1]['q'].set_data(time_axis, q_array[:, i])
            lines_dict[i * 3 + 1]['n'].set_data(time_axis, n_array[:, i])
            # 更新x轴范围（滑动窗口：0 到 WINDOW_DURATION）
            if len(time_axis) > 0:
                axes_list[i * 3].set_xlim(0, WINDOW_DURATION)
            # 更新y轴范围
            if len(q_array) > 0 and len(n_array) > 0:
                y_min = min(np.min(q_array[:, i]), np.min(n_array[:, i]))
                y_max = max(np.max(q_array[:, i]), np.max(n_array[:, i]))
                y_range = y_max - y_min
                if y_range > 0:
                    axes_list[i * 3].set_ylim(y_min - y_range * 0.1, y_max + y_range * 0.1)
        
        # 更新扭矩图（第二列）
        if i * 3 + 2 in lines_dict and f_array is not None:
            lines_dict[i * 3 + 2]['f'].set_data(time_axis, f_array[:, i])
            # 更新x轴范围（滑动窗口：0 到 WINDOW_DURATION）
            if len(time_axis) > 0:
                axes_list[i * 3 + 1].set_xlim(0, WINDOW_DURATION)
            if len(f_array) > 0:
                y_min = np.min(f_array[:, i])
                y_max = np.max(f_array[:, i])
                y_range = y_max - y_min
                if y_range > 0:
                    axes_list[i * 3 + 1].set_ylim(y_min - y_range * 0.1, y_max + y_range * 0.1)
        
        # 更新错误码图（第三列）
        if i * 3 + 3 in lines_dict and e_array is not None:
            # 清除旧的散点
            if 'scatter' in lines_dict[i * 3 + 3]:
                for sc in lines_dict[i * 3 + 3]['scatter']:
                    sc.remove()
                lines_dict[i * 3 + 3]['scatter'] = []
            
            # 绘制新的错误码点
            error_mask = e_array[:, i] != 0
            if np.any(error_mask):
                error_times = time_axis[error_mask]
                error_codes = e_array[error_mask, i]
                colors = ['red', 'orange', 'purple', 'brown', 'pink']
                scatter_list = []
                for t, code in zip(error_times, error_codes):
                    color = colors[code % len(colors)] if code < len(colors) else 'red'
                    sc = axes_list[i * 3 + 2].scatter(t, code, c=color, s=50, alpha=0.8, zorder=5)
                    scatter_list.append(sc)
                lines_dict[i * 3 + 3]['scatter'] = scatter_list
                # 更新x轴范围（滑动窗口：0 到 WINDOW_DURATION）
                if len(time_axis) > 0:
                    axes_list[i * 3 + 2].set_xlim(0, WINDOW_DURATION)
    
    return lines_dict

def plot_real_time(log_file_path):
    """
    实时绘制关节数据波形图（滑动窗口30秒）
    
    Args:
        log_file_path: 日志文件路径
    """
    global _data_cache, _last_file_position
    
    # 初始化数据缓存
    _data_cache = {
        'q_data': [],
        'n_data': [],
        'f_data': [],
        'e_data': [],
        'timestamps': []
    }
    _last_file_position = 0
    
    # 创建图形和子图：10行3列
    fig = plt.figure(figsize=(18, 24))
    axes_list = []
    lines_dict = {}
    
    joint_names = [f'Joint {i}' for i in range(10)]
    
    # 初始化所有子图和线条
    for i in range(10):
        # 第一列：位置（Q 和 N）
        ax1 = plt.subplot(10, 3, i * 3 + 1)
        line_q, = ax1.plot([], [], 'b-', label='Q (Current)', linewidth=1.5, alpha=0.8)
        line_n, = ax1.plot([], [], 'r--', label='N (Target)', linewidth=1.5, alpha=0.8)
        ax1.set_title(f'{joint_names[i]} - Position', fontsize=11, fontweight='bold')
        ax1.set_xlabel('Time (s)', fontsize=9)
        ax1.set_ylabel('Position (rad)', fontsize=9)
        ax1.set_xlim(0, WINDOW_DURATION)
        ax1.grid(True, alpha=0.3)
        ax1.legend(loc='best', fontsize=8)
        axes_list.append(ax1)
        lines_dict[i * 3 + 1] = {'q': line_q, 'n': line_n}
        
        # 第二列：扭矩（F）
        ax2 = plt.subplot(10, 3, i * 3 + 2)
        line_f, = ax2.plot([], [], 'g-', label='F (Torque)', linewidth=1.5, alpha=0.8)
        ax2.axhline(y=0, color='k', linestyle=':', linewidth=0.5, alpha=0.5)
        ax2.set_title(f'{joint_names[i]} - Torque', fontsize=11, fontweight='bold')
        ax2.set_xlabel('Time (s)', fontsize=9)
        ax2.set_ylabel('Torque (N·m)', fontsize=9)
        ax2.set_xlim(0, WINDOW_DURATION)
        ax2.grid(True, alpha=0.3)
        ax2.legend(loc='best', fontsize=8)
        axes_list.append(ax2)
        lines_dict[i * 3 + 2] = {'f': line_f}
        
        # 第三列：错误码（E）
        ax3 = plt.subplot(10, 3, i * 3 + 3)
        ax3.set_title(f'{joint_names[i]} - Error Code', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Time (s)', fontsize=9)
        ax3.set_ylabel('Error Code', fontsize=9)
        ax3.set_xlim(0, WINDOW_DURATION)
        ax3.set_ylim(-0.5, 5.5)
        ax3.set_yticks([0, 1, 2, 3, 4, 5])
        ax3.set_yticklabels(['0:Normal', '1:Overheat', '2:Overcurrent', '3:Overvoltage', '4:Encoder', '5:Protect'])
        ax3.grid(True, alpha=0.3)
        axes_list.append(ax3)
        lines_dict[i * 3 + 3] = {'scatter': []}
    
    # 添加总标题
    fig.suptitle(f'QMini Real-time Joint Data (Last {WINDOW_DURATION}s): Position | Torque | Error Code', 
                 fontsize=16, fontweight='bold', y=0.995)
    
    # 调整布局
    plt.tight_layout(rect=[0, 0, 1, 0.99])
    
    # 创建动画，更新频率为10Hz（比数据更新频率高，保证流畅）
    interval_ms = int(1000 / 10)  # 10Hz = 100ms
    ani = FuncAnimation(fig, update_real_time_plot, fargs=(log_file_path, axes_list, lines_dict),
                       interval=interval_ms, blit=False, cache_frame_data=False)
    
    # 显示窗口
    plt.show()
    
    return ani

def plot_all_joints_together(q_data, n_data, f_data=None, save_path=None):
    """
    在一个图中绘制所有关节的波形（可选）
    
    Args:
        q_data: Q数据列表
        n_data: N数据列表
        f_data: F数据列表（扭矩，可选）
        save_path: 保存图片的路径（可选）
    """
    if not q_data or not n_data:
        return
    
    q_array = np.array(q_data)
    n_array = np.array(n_data)
    
    min_len = min(len(q_array), len(n_array))
    q_array = q_array[:min_len]
    n_array = n_array[:min_len]
    
    time_axis = np.arange(min_len)
    
    # 创建三个子图：位置、扭矩
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 12))
    
    # 第一个子图：位置
    for i in range(10):
        ax1.plot(time_axis, q_array[:, i], 'b-', label=f'Q Joint {i}' if i < 5 else '', 
                linewidth=1.5, alpha=0.7)
        ax1.plot(time_axis, n_array[:, i], 'r--', label=f'N Joint {i}' if i < 5 else '', 
                linewidth=1.5, alpha=0.7)
    
    ax1.set_title('All Joints: Q (Current) vs N (Target) Position', fontsize=14, fontweight='bold')
    ax1.set_xlabel('Time (s)', fontsize=12)
    ax1.set_ylabel('Position (rad)', fontsize=12)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='best', fontsize=9, ncol=2)
    
    # 第二个子图：扭矩
    if f_data and len(f_data) > 0:
        f_array = np.array(f_data)
        if len(f_array) > min_len:
            f_array = f_array[:min_len]
        elif len(f_array) < min_len:
            f_padded = np.full((min_len, 10), np.nan)
            f_padded[:len(f_array)] = f_array
            f_array = f_padded
        
        for i in range(10):
            ax2.plot(time_axis, f_array[:, i], 'g-', label=f'F Joint {i}' if i < 5 else '', 
                    linewidth=1.5, alpha=0.7)
        ax2.axhline(y=0, color='k', linestyle=':', linewidth=0.5, alpha=0.5)
        ax2.set_title('All Joints: F (Torque)', fontsize=14, fontweight='bold')
        ax2.set_xlabel('Time (s)', fontsize=12)
        ax2.set_ylabel('Torque (N·m)', fontsize=12)
        ax2.grid(True, alpha=0.3)
        ax2.legend(loc='best', fontsize=9, ncol=2)
    else:
        ax2.text(0.5, 0.5, 'No F (Torque) data available', ha='center', va='center', 
                transform=ax2.transAxes, fontsize=14)
        ax2.set_title('All Joints: F (Torque)', fontsize=14, fontweight='bold')
    
    plt.tight_layout()
    
    if save_path:
        # 从原始保存路径提取目录和时间戳，生成新的文件名
        log_dir = os.path.dirname(save_path)
        base_filename = os.path.basename(save_path)
        # 提取时间戳部分（Joint_log_YYMM_DD_HH_MM.png -> YYMM_DD_HH_MM）
        timestamp_part = base_filename.replace('Joint_log_', '').replace('.png', '')
        save_path_all = os.path.join(log_dir, f'Joint_log_{timestamp_part}_all_joints.png')
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
    
    # 检查是否有 --realtime 或 -r 参数
    realtime_mode = False
    if len(sys.argv) > 1:
        if '--realtime' in sys.argv or '-r' in sys.argv:
            realtime_mode = True
            # 移除参数，保留文件路径
            sys.argv = [a for a in sys.argv if a not in ['--realtime', '-r']]
            if len(sys.argv) > 1:
                log_file_path = sys.argv[1]
    
    # 检查文件是否存在
    if not os.path.exists(log_file_path):
        print(f"错误: 文件不存在: {log_file_path}")
        print(f"使用方法:")
        print(f"  实时模式: python {sys.argv[0]} --realtime [log_file_path]")
        print(f"  静态模式: python {sys.argv[0]} [log_file_path]")
        return
    
    if realtime_mode:
        # 实时模式
        print(f"实时模式启动，读取日志文件: {log_file_path}")
        print(f"显示窗口: 最近 {WINDOW_DURATION} 秒的数据")
        print("按 Ctrl+C 退出实时显示")
        try:
            plot_real_time(log_file_path)
        except KeyboardInterrupt:
            print("\n实时显示已停止")
    else:
        # 静态模式（原有的功能）
        print(f"静态模式，读取日志文件: {log_file_path}")
        
        # 解析日志文件
        q_data, n_data, f_data, e_data = parse_log_file(log_file_path)
        
        if q_data is None or n_data is None:
            return
        
        print(f"成功读取 {len(q_data)} 条 Q 数据")
        print(f"成功读取 {len(n_data)} 条 N 数据")
        if f_data:
            print(f"成功读取 {len(f_data)} 条 F 数据（扭矩）")
        else:
            print("未找到 F 数据（扭矩）")
        if e_data:
            print(f"成功读取 {len(e_data)} 条 E 数据（错误码）")
        else:
            print("未找到 E 数据（错误码）")
        
        if len(q_data) == 0 or len(n_data) == 0:
            print("警告: 没有找到有效数据")
            return
        
        # 创建 joint_log 目录（如果不存在）
        log_dir = os.path.join(os.path.dirname(log_file_path), 'joint_log')
        os.makedirs(log_dir, exist_ok=True)
        
        # 生成带时间戳的文件名（格式：Joint_log_YYMM_DD_HH_MM.png）
        now = datetime.now()
        timestamp = now.strftime('%y%m_%d_%H_%M')
        filename = f'Joint_log_{timestamp}.png'
        save_path = os.path.join(log_dir, filename)
        
        # 绘制分关节图（包含位置、扭矩、错误码）
        print("正在绘制分关节波形图...")
        plot_joint_data(q_data, n_data, f_data, e_data, save_path)
        
        # 绘制所有关节在一张图上（可选，注释掉以禁用）
        # print("正在绘制所有关节波形图...")
        # plot_all_joints_together(q_data, n_data, f_data, save_path)

if __name__ == '__main__':
    main()

