#!/bin/bash

# QMini 自动化编译安装脚本
# 用于 x86_64 平台的编译和安装

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}QMini 自动化编译安装脚本${NC}"
echo -e "${GREEN}========================================${NC}"

# 检查构建目录
BUILD_DIR="build_x86"
INSTALL_DIR_X64="install/x64"
INSTALL_DIR_X86_64="install/x86_64"

echo -e "\n${YELLOW}[1/5] 检查构建目录...${NC}"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}创建构建目录: $BUILD_DIR${NC}"
    mkdir -p "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# CMake 配置
echo -e "\n${YELLOW}[2/5] 运行 CMake 配置...${NC}"
cmake .. -DCMAKE_INSTALL_PREFIX=../$INSTALL_DIR_X64/

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 配置失败！${NC}"
    exit 1
fi

# 编译
echo -e "\n${YELLOW}[3/5] 开始编译...${NC}"
CPU_CORES=$(nproc)
echo -e "使用 ${CPU_CORES} 个 CPU 核心进行编译"
make -j${CPU_CORES}

if [ $? -ne 0 ]; then
    echo -e "${RED}编译失败！${NC}"
    exit 1
fi

# 安装
echo -e "\n${YELLOW}[4/5] 安装到 $INSTALL_DIR_X64...${NC}"
make install

if [ $? -ne 0 ]; then
    echo -e "${RED}安装失败！${NC}"
    exit 1
fi

# 复制到 x86_64 目录
echo -e "\n${YELLOW}[5/5] 复制文件到 $INSTALL_DIR_X86_64...${NC}"
cd "$SCRIPT_DIR"

# 确保目标目录存在
mkdir -p "$INSTALL_DIR_X86_64/bin"

# 复制可执行文件
if [ -f "$INSTALL_DIR_X64/bin/run_interface" ]; then
    cp "$INSTALL_DIR_X64/bin/run_interface" "$INSTALL_DIR_X86_64/bin/run_interface"
    echo -e "${GREEN}✓ 已复制 run_interface${NC}"
else
    echo -e "${RED}✗ 未找到 run_interface${NC}"
    exit 1
fi

if [ -f "$INSTALL_DIR_X64/bin/test_lib" ]; then
    cp "$INSTALL_DIR_X64/bin/test_lib" "$INSTALL_DIR_X86_64/bin/test_lib"
    echo -e "${GREEN}✓ 已复制 test_lib${NC}"
fi

# 显示文件信息
echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}编译安装完成！${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "\n可执行文件位置:"
echo -e "  ${GREEN}$INSTALL_DIR_X86_64/bin/run_interface${NC}"
ls -lh "$INSTALL_DIR_X86_64/bin/run_interface" 2>/dev/null || echo "文件不存在"

echo -e "\n${YELLOW}提示:${NC}"
echo -e "  运行程序: ./start_qmini_进程.sh"
echo -e "  停止程序: ./stop_qmini_进程.sh"

