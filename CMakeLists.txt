cmake_minimum_required(VERSION 3.5)
project(QminiSimAndDeploy VERSION 1.0)

# 添加
# 设置项目根目录
set(CMAKE_PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 判断 target 类型

# 整体编译选项和全局变量
set(CMAKE_BUILD_TYPE Debug)


# 添加整体 include 目录
include_directories(${CMAKE_PROJECT_ROOT_DIR}/include)
include_directories(${CMAKE_PROJECT_ROOT_DIR}/install/${CMAKE_SYSTEM_PROCESSOR}/include)


# 添加 subdir
add_subdirectory(${CMAKE_PROJECT_ROOT_DIR}/test)
# add_subdirectory(${CMAKE_PROJECT_ROOT_DIR}/src)