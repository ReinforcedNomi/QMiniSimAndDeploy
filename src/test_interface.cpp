#include "user/custom.hpp"

int main(int argc, char const *argv[]) {
    std::cout << "Usage networkInterface: " << "enp9s0 of cyy528 " << std::endl;
    std::string networkInterface = "enp9s0";
    G1 g1(networkInterface, false);

    while (true) sleep(10);

    return 0;
}
