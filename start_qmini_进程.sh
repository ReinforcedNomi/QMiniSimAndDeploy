#! /bin/bash

# 将项目库路径放在最前面，确保优先使用项目自己编译的库
# 这样可以避免链接到ROS或其他系统的yaml-cpp库（版本可能不兼容）
export LD_LIBRARY_PATH=$(pwd)/install/x86_64/lib/:$LD_LIBRARY_PATH

# 运行程序
./install/x86_64/bin/run_interface &> qmini_log.log &

