add_executable(
    run_interface 
        "${CMAKE_CURRENT_LIST_DIR}/run_interface.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user/custom.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user/rl_controller.cpp")

# 找库
find_package(Python3 REQUIRED COMPONENTS Development)
if(Python3_FOUND)
    message(STATUS "Found Python3: ${Python3_EXECUTABLE} (version ${Python3_VERSION})")
else()
    message(FATAL_ERROR "Python3 not found.")
endif()


# include 部分
target_include_directories(run_interface 
    PUBLIC 
        "${Python3_INCLUDE_DIRS}"
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
        jsoncp
        onnxruntime
        unitree_sdk2
        UnitreeMotorSDK_Linux64
        yaml-cpp
        ${Python3_LIBRARIES}
)
target_link_libraries(
    run_interface
    PRIVATE
        unitree_sdk2
)



# 可执行文件安装
install(TARGETS run_interface DESTINATION ${CMAKE_INSTALL_BINDIR})


