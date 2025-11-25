# 设置目标系统名称
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 交叉编译器前缀
set(CROSS_COMPILE_PREFIX "/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-")

# 设置交叉编译器
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_LINKER ${CROSS_COMPILE_PREFIX}ld)

# 设置目标系统的根文件系统路径 (如果有)
# set(CMAKE_SYSROOT /path/to/your/sysroot)

# 设置查找路径的模式
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译器标志
# set(CMAKE_C_FLAGS "-march=armv8-a" CACHE STRING "C compiler flags")
# set(CMAKE_CXX_FLAGS "-march=armv8-a" CACHE STRING "C++ compiler flags")

# (可选) 设置调试标志
# set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "C debug compiler flags")
# set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "C++ debug compiler flags")

# (可选) 设置发布标志
# set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "C release compiler flags")
# set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "C++ release compiler flags")