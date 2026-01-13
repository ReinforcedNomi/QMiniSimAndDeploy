#!/bin/bash

# 获取脚本所在目录的绝对路径（即使在sudo环境下也能正确工作）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo chmod 777 /dev/serial/by-id/*
# 将项目库路径放在最前面，确保优先使用项目自己编译的库
# 这样可以避免链接到ROS或其他系统的yaml-cpp库（版本可能不兼容）
export LD_LIBRARY_PATH="${SCRIPT_DIR}/install/aarch64/lib/:$LD_LIBRARY_PATH"

# 运行程序（使用绝对路径） 修正路径 不是x86_64
# 测试模式需要键盘输入，所以在前台运行
# 使用tee命令同时输出到终端和日志文件（覆盖模式，每次运行重新开始）
# 2>&1 将标准错误重定向到标准输出，然后通过tee同时显示在终端和写入日志文件
"${SCRIPT_DIR}/install/x64/bin/run_interface" 2>&1 | tee "${SCRIPT_DIR}/qmini_log.log"

