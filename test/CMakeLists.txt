add_executable(test_lib "${CMAKE_CURRENT_LIST_DIR}/test_lib.cpp")


target_link_directories(
    test_lib
    PUBLIC "${CMAKE_PROJECT_ROOT_DIR}/install/${CMAKE_SYSTEM_PROCESSOR}/lib/")

target_link_libraries(
    test_lib
    PUBLIC jsoncpp
)

# 可执行文件安装
install(TARGETS test_lib DESTINATION ${CMAKE_INSTALL_BINDIR})


