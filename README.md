# QMiniSimAndDeploy
将QMini部署在PC, JestonNano, RaspberryPi 等仿真或者嵌入式平台上


## 1. clone 依赖



## 2. 编译



## 常用操作记录

## 解压

## x86 PC 编译
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../../../install/x64/
make
make install

## aarch64 交叉编译

cmake -DCMAKE_SYSROOT=/path/to/target/sysroot \
      -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
      -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
      ..

cmake -DCMAKE_SYSROOT=/path/to/target/sysroot \
      -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
      -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \

/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc
/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++
aarch64-none-linux-gnu-gcc
aarch64-none-linux-gnu-g++


cmake \
    -DCMAKE_C_COMPILER=/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++ \
    ..


mkdir build-arm
cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm.cmake ..
make

## 运行
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:install/x86_64/lib/
./install/x86_64/bin/test_lib
