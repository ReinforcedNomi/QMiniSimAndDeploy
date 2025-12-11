# 设置目标系统名称
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 关键 设置为静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 交叉编译器前缀
set(CROSS_COMPILE_PREFIX "/root/zhaoshucheng_dir/20251121_Embodied_Peoject/QMiniSimAndDeploy/aarch64--glibc--stable-2022.08-1/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-")

# 设置交叉编译器
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_LINKER ${CROSS_COMPILE_PREFIX}ld)

# 设置目标系统的根文件系统路径 (如果有)
set(CMAKE_SYSROOT "/root/zhaoshucheng_dir/20251121_Embodied_Peoject/QMiniSimAndDeploy/jetson_nano_rootfs")
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# 设置查找路径的模式
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


# 设置交叉编译全局 include 路径
include_directories(
    ${CMAKE_SYSROOT}/usr/include/aarch64-linux-gnu
    ${CMAKE_SYSROOT}/usr/include
)

# 设置链接 so 的搜索路径
link_directories(
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu
    ${CMAKE_SYSROOT}/usr/lib
)

# 设置交叉编译的预编译链接
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -B${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -B${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/")




