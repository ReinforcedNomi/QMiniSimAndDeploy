#include "user/custom.hpp"

int main(int argc, char const *argv[]) {
    std::cout << "Usage networkInterface: " << "lo of Q1 robot " << std::endl;
    std::string networkInterface = "lo";
    G1 g1(networkInterface, true);  // 设置为true启用测试模式（键盘控制）

    while (true) sleep(10);

    return 0;
}
// '1' - 准备就绪模式（folding）
// '2' - 站立模式（standing）
// '3' - RL行走模式（RL walking）
// '4' - RL站立模式（RL stand）
// '5' - 正弦测试模式（sin test）
// '6' - 模式6
// '7' - 模式7
// '8' - 模式8
// '9' - 模式9
// 'q' - 退出程序（仅在模式1时有效）