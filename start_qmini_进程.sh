#!/bin/bash

# 获取脚本所在目录的绝对路径（即使在sudo环境下也能正确工作）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 将项目库路径放在最前面，确保优先使用项目自己编译的库
# 这样可以避免链接到ROS或其他系统的yaml-cpp库（版本可能不兼容）
export LD_LIBRARY_PATH="${SCRIPT_DIR}/install/x86_64/lib/:$LD_LIBRARY_PATH"

# 运行程序（使用绝对路径）
"${SCRIPT_DIR}/install/x86_64/bin/run_interface" &> "${SCRIPT_DIR}/qmini_log.log" &

