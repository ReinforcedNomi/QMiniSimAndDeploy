# 这里是 onnxruntime 的预编译好的 CMakeLists.txt
# 目的是 make install 的时候，可以根据当前系统的架构，自动复制拷贝文件到 install 目录中

cmake_minimum_required(VERSION 3.1)
project(onnxruntime)


set(target_plantform "")

# 判断编译器的平台
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "CMAKE_CROSSCOMPILING: ${CMAKE_CROSSCOMPILING}")

# 打印当前使用的 编译器
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
# 打印当前使用的 编译器路径
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")

# if(CMAKE_SYSTEM_NAME)