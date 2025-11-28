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

cmake \
      -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
      -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
      ..

/root/zhaoshucheng_dir/20251121_Embodied_Peoject/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc \
      --sysroot=/root/zhaoshucheng_dir/20251121_Embodied_Peoject/JestoNanoImg/Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64 \
      CMakeFiles/cmTC_d8315.dir/testCCompiler.c.o \
      -o cmTC_d8315 \
      -L/root/zhaoshucheng_dir/20251121_Embodied_Peoject/JestoNanoImg/Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64/usr/lib/aarch64-linux-gnu 

### 交叉编译 third_party

#### 挂载和卸载 sshfs

挂载
sshfs -p 22 qmini@10.168.1.160:/ jetson_nano_rootfs/

卸载
umount jetson_nano_rootfs/
或者
umount -f jetson_nano_rootfs/

#### eigen

cd third_party/eigen
mkdir build
cd build
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
    -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
    ..
make
make install

#### jsoncpp
cd third_party/jsoncpp
mkdir build
cd build
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
    -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
    -DJSONCPP_WITH_TESTS=OFF \
    ..
make
make install

#### onnxruntime
这个没有源码，只有预编译的，手工拷贝
在 install/aarch64/include 创建 onnx 文件夹
将 third_party/onnxruntime/aarch64/include 中的文件和文件夹拷贝到 install/aarch64/include/onnx 中
将 third_party/onnxruntime/aarch64/lib 中的 so 拷贝到 install/aarch64/lib 中

#### third_party/unitree_actuator_sdk

这里也是预编译的，没有源码

将 third_party/unitree_actuator_sdk/include 里面的文件夹拷贝到 install/aarch64/include 中
将 third_party/unitree_actuator_sdk/lib/libUnitreeMotorSDK_Arm64.so 拷贝到 install/aarch64/lib 中

<!-- cd third_party/unitree_actuator_sdk
mkdir build
cd build
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
    -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
    ..
make
make install -->


#### third_party/yaml-cpp
cd third_party/yaml-cpp
mkdir build
cd build
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
    -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
    -DYAML_BUILD_SHARED_LIBS=ON \
    -DYAML_CPP_BUILD_TESTS=OFF \
    -DYAML_CPP_BUILD_TOOLS=OFF \
    ..
make
make install


#### third_party/unitree_sdk2
cd third_party/unitree_sdk2
mkdir build
cd build
cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../../cmake_toolchain/toolchain-JetsonNano.cmake \
    -DCMAKE_INSTALL_PREFIX=../../../install/aarch64/ \
    ..
make
make install




## 运行

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:install/x86_64/lib/
./install/x86_64/bin/test_lib

export CROSS_COMPILE=/root/zhaoshucheng_dir/20251121_Embodied_Peoject/QMiniSimAndDeploy/aarch64--glibc--stable-2022.08-1/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-

/root/zhaoshucheng_dir/20251121_Embodied_Peoject/QMiniSimAndDeploy/aarch64--glibc--stable-2022.08-1/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-c++.br_real


