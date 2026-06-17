# Linux 平台使用示例

## 编译
指定编译器路径 GCC_COMPILER，例如`export GCC_COMPILER=~/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu `，执行如下命令：

```sh
# 请先指定编译器路径
(optional)export GCC_COMPILER=<GCC_COMPILER_PATH>

./build-linux.sh -t <TARGET_PLATFORM> -a <ARCH> [-b <build_type>]

# 例如: 
./build-linux.sh -t rk3588 -a aarch64 -b Release
```

## 推送到板端

将 install/rknn3_model_test_Linux 拷贝到设备上。

- 如果使用Rockchip的EVB板(如 RK3588 开发板)，可以使用以下命令：

连接设备并将程序和模型传输到`/userdata`

```
adb push install/rknn3_model_test_Linux /userdata/
```

- 如果你的板子有sshd服务，可以使用scp命令或者其他方式将程序和模型传输到板子上。

## 运行

```sh
adb shell
cd /userdata/rknn3_model_test_Linux/
```

```sh
export LD_LIBRARY_PATH=./lib
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight resnet50v2_with_weight/in.npy resnet50v2_with_weight/out.npy 0x1
# usage:
# rknn3_model_test <model_path> <weight_path> <input_npy_paths> <golden_output_npy_paths> <core_mask> [loop_count]
```
参数说明：
1. model_path: rknn 文件路径
2. weight_path: weight 文件路径
3. input_npy_paths: 输入 `npy` 文件路径，如果有多个输入，用 '#' 分割，例如 'input1.npy#input2.npy'
4. golden_output_npy_paths: 正确输出的 `npy` 文件路径，用于准确性比较，如果有多个输出，用 '#' 分割，例如 'golden_out0.npy#golden_out1.npy'
5. core_mask: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3。
6. loop_count: 推理次数，可选。

**注：**
1. **如果只想测试模型是否能正常推理，可以只填 `model_path` 和 `weight_path`，推理时将使用随机输入数据，且不对结果进行校验，`core_mask` 根据查询到的核心数自动生成，`loop_count` 使用默认值 1。**
2. **如果既想使用随机输入，又想设置 `core_mask` 或 `loop_count`，可以将 `input_npy_paths` 和 `golden_output_npy_paths`都设成空字符串。举例如下：**
```sh
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight "" "" 0x1 1000
```

# Android 平台使用示例

## 编译
指定 ANDROID_NDK_PATH，例如`export ANDROID_NDK_PATH=~/opts/ndk/android-ndk-r17c`，然后执行如下命令：
```sh
# 请先指定编译器路径
(optional)export ANDROID_NDK_PATH=<ANDROID_NDK_PATH>

./build-android.sh -t <target> -a <arch> [-b <build_type>]

# 例如: 
./build-android.sh -t rk3588 -a arm64-v8a -b Release
```

## 推送到板端

连接设备并将程序和模型传输到`/data`

```
adb push install/rknn3_model_test_Android /data/
```

## 运行

```sh
adb shell
cd /data/rknn3_model_test_Android/
```

```sh
export LD_LIBRARY_PATH=./lib
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight resnet50v2_with_weight/in.npy resnet50v2_with_weight/out.npy 0x1
# usage:
# rknn3_model_test <model_path> <weight_path> <input_npy_paths> <golden_output_npy_paths> <core_mask> [loop_count]
```
参数说明：
1. model_path: rknn 文件路径
2. weight_path: weight 文件路径
3. input_npy_paths: 输入 `npy` 文件路径，如果有多个输入，用 '#' 分割，例如 'input1.npy#input2.npy'
4. golden_output_npy_paths: 正确输出的 `npy` 文件路径，用于准确性比较，如果有多个输出，用 '#' 分割，例如 'golden_out0.npy#golden_out1.npy'
5. core_mask: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3。
6. loop_count: 推理次数，可选。

**注：**
1. **如果只想测试模型是否能正常推理，可以只填 `model_path` 和 `weight_path`，推理时将使用随机输入数据，且不对结果进行校验，`core_mask` 根据查询到的核心数自动生成，`loop_count` 使用默认值 1。**
2. **如果既想使用随机输入，又想设置 `core_mask` 或 `loop_count`，可以将 `input_npy_paths` 和 `golden_output_npy_paths`都设成空字符串。举例如下：**
```sh
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight "" "" 0x1 1000
```
