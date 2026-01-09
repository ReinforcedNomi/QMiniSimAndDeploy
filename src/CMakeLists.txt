# 变量声明

set(MY_UnitreeMotorSDK_NAME "")    # 宇树的预编译库


# 判断 CMAKE_SYSTEM_PROCESSOR 给 MY_UnitreeMotorSDK_NAME 赋值
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(MY_UnitreeMotorSDK_NAME "UnitreeMotorSDK_Arm64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(MY_UnitreeMotorSDK_NAME "UnitreeMotorSDK_Linux64")
else()
    message(FATAL_ERROR "Unsupported system processor: ${CMAKE_SYSTEM_PROCESSOR}")
endif()



add_executable(
    run_interface 
        "${CMAKE_CURRENT_LIST_DIR}/run_interface.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user/custom.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user/rl_controller.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user/custom_rl_controller.cpp")

# add_executable()

# 找库
# find_package(Python3 REQUIRED COMPONENTS Development)
# if(Python3_FOUND)
#     message(STATUS "Found Python3: ${Python3_EXECUTABLE} (version ${Python3_VERSION})")
#     message(STATUS "Python3_INCLUDE_DIRS: ${Python3_INCLUDE_DIRS}")
#     message(STATUS "Python3_LIBRARIES: ${Python3_LIBRARIES}")
# else()
#     message(FATAL_ERROR "Python3 not found.")
# endif()


# include 部分
target_include_directories(run_interface 
    PUBLIC 
        # "${Python3_INCLUDE_DIRS}"
        "/usr/include/python3.10"
        "${CMAKE_PROJECT_ROOT_DIR}/install/${CMAKE_SYSTEM_PROCESSOR}/include/ddscxx")


# link 部分
target_link_directories(
    run_interface
    PUBLIC "${CMAKE_PROJECT_ROOT_DIR}/install/${CMAKE_SYSTEM_PROCESSOR}/lib/")

target_link_libraries(
    run_interface
    PUBLIC 
        jsoncpp
        ddsc
        ddscxx
        onnxruntime
        unitree_sdk2
        ${MY_UnitreeMotorSDK_NAME}
        yaml-cpp
        # ${Python3_LIBRARIES}
        python3.10
)
target_link_libraries(
    run_interface
    PRIVATE
        unitree_sdk2
)



# 可执行文件安装
install(TARGETS run_interface DESTINATION ${CMAKE_INSTALL_BINDIR})


