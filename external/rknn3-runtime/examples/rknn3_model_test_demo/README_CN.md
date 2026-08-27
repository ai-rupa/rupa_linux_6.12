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

## install 目录库文件说明

编译并执行 `make install` 后，`install/rknn3_model_test_Linux/lib` 中会按平台安装以下库：

- RK3576 / RK3588：`librknn3_api.so` + `librknn3_api_rkcp.so`
- RK3572：`librknn3_api.so` + `librknn3_api_native.so`

## 推送到板端

将 install/rknn3_model_test_Linux 拷贝到设备上。

- 如果使用 Rockchip 的 EVB 板 (如 RK3588 开发板)，可以使用以下命令：

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
# rknn3_model_test <model_path> <weight_path> [input_npy_paths] [golden-output_npy_paths] <core_mask> [loop_count] [key_path] [shape_id] [chunk_size_mb]
```
参数说明：
1. model_path: rknn 文件路径
2. weight_path: weight 文件路径
3. input_npy_paths: 输入 `npy` 文件路径，如果有多个输入，用 '#' 分割，例如 'input1.npy#input2.npy'
4. golden_output_npy_paths: 正确输出的 `npy` 文件路径，用于准确性比较，如果有多个输出，用 '#' 分割，例如 'golden_out0.npy#golden_out1.npy'
5. core_mask: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是 0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3。
6. loop_count: 推理次数，可选。
7. key_path: 解密密钥文件路径，可选。使用 `none` 跳过解密。
8. shape_id: 动态 shape 模型的 shape ID，可选，默认为 0。
9. chunk_size_mb: 流式权重加载的块大小（MB），可选，默认为 0。当设为 0 时，一次性加载全部权重数据；当设为大于 0 的值时，启用流式权重加载模式，按指定块大小分块上传权重数据。

**注：**
1. **如果只想测试模型是否能正常推理，可以只填 `model_path` 和 `weight_path`，推理时将使用随机输入数据，且不对结果进行校验，`core_mask` 根据查询到的核心数自动生成，`loop_count` 使用默认值 1。**
2. **如果既想使用随机输入，又想设置 `core_mask` 或 `loop_count`，可以将 `input_npy_paths` 和 `golden_output_npy_paths`都设成空字符串。举例如下：**
```sh
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight "" "" 0x1 1000
```
3. **对于动态 shape 模型，可以通过 `shape_id` 参数指定要使用的 shape 组合，默认使用 shape 0。举例如下：**
```sh
./rknn3_model_test model.rknn model.weight "" "" 0x1 1 none 1
```
4. **流式权重加载模式适用于大模型场景，可以减少内存峰值占用。启用后会先加载模型结构（weight=NULL），再调用 `rknn3_model_init`，最后分块上传权重数据。举例如下：**
```sh
./rknn3_model_test model.rknn model.weight "" "" 0x1 1 none 0 64
```

# Android 平台使用示例

## 编译
指定 ANDROID_NDK_PATH，例如`export ANDROID_NDK_PATH=~/opts/ndk/android-ndk-r17c`，然后执行如下命令：
```sh
# 请先指定编译器路径
(optional)export ANDROID_NDK_PATH=<ANDROID_NDK_PATH>

./build-android.sh -t <target> -a <arch> [-b <build_type>]

# 例如：
./build-android.sh -t rk3588 -a arm64-v8a -b Release
```

## install 目录库文件说明

编译并执行 `make install` 后，`install/rknn3_model_test_Android/lib` 中会按平台安装以下库：

- RK3576 / RK3588：`librknn3_api.so` + `librknn3_api_rkcp.so`
- RK3572：`librknn3_api.so` + `librknn3_api_native.so`

## 推送到板端

连接设备并将程序和模型传输到 `/data`

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
# rknn3_model_test <model_path> <weight_path> [input_npy_paths] [golden-output_npy_paths] <core_mask> [loop_count] [key_path] [shape_id] [chunk_size_mb]
```
参数说明：
1. model_path: rknn 文件路径
2. weight_path: weight 文件路径
3. input_npy_paths: 输入 `npy` 文件路径，如果有多个输入，用 '#' 分割，例如 'input1.npy#input2.npy'
4. golden_output_npy_paths: 正确输出的 `npy` 文件路径，用于准确性比较，如果有多个输出，用 '#' 分割，例如 'golden_out0.npy#golden_out1.npy'
5. core_mask: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是 0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3。
6. loop_count: 推理次数，可选。
7. key_path: 解密密钥文件路径，可选。使用 `none` 跳过解密。
8. shape_id: 动态 shape 模型的 shape ID，可选，默认为 0。
9. chunk_size_mb: 流式权重加载的块大小（MB），可选，默认为 0。当设为 0 时，一次性加载全部权重数据；当设为大于 0 的值时，启用流式权重加载模式，按指定块大小分块上传权重数据。

**注：**
1. **如果只想测试模型是否能正常推理，可以只填 `model_path` 和 `weight_path`，推理时将使用随机输入数据，且不对结果进行校验，`core_mask` 根据查询到的核心数自动生成，`loop_count` 使用默认值 1。**
2. **如果既想使用随机输入，又想设置 `core_mask` 或 `loop_count`，可以将 `input_npy_paths` 和 `golden_output_npy_paths`都设成空字符串。举例如下：**
```sh
./rknn3_model_test resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight "" "" 0x1 1000
```
3. **对于动态 shape 模型，可以通过 `shape_id` 参数指定要使用的 shape 组合，默认使用 shape 0。举例如下：**
```sh
./rknn3_model_test model.rknn model.weight "" "" 0x1 1 none 1
```

# Windows 平台使用示例（Cygwin）

## 编译

在 Windows Cygwin 环境下，执行如下命令：

```sh
./build-cygwin.sh -t <TARGET_PLATFORM> [-b <build_type>]

# 例如：
./build-cygwin.sh -t rk3588 -b Release
```

**注意：**
- 需要在 Cygwin 终端中运行构建脚本
- 确保已安装 Cygwin 的编译工具链（gcc、g++、make 等）
- 目标平台参数必须小写（如 rk3588、rk3576）

## install 目录库文件说明

编译并执行 `make install` 后，`install/rknn3_model_test_windows/lib` 中会按平台安装以下库：

- RK3576 / RK3588：`librknn3_api.dll` + `librknn3_api_rkcp.dll`
- RK3572：`librknn3_api.dll` + `librknn3_api_native.dll`

## 运行

在 Cygwin 终端或 Windows 命令行中运行：

```sh
# 方式 1: Cygwin 终端
cd install/rknn3_model_test_windows/
export PATH=./lib:$PATH
./rknn3_model_test.exe resnet50v2_with_weight/resnet50v2.rknn resnet50v2_with_weight/resnet50v2.weight resnet50v2_with_weight/in.npy resnet50v2_with_weight/out.npy 0x1

# 方式 2: Windows 命令行 (cmd/PowerShell)
cd install\rknn3_model_test_windows
set PATH=.\lib;%PATH%
rknn3_model_test.exe resnet50v2_with_weight\resnet50v2.rknn resnet50v2_with_weight\resnet50v2.weight resnet50v2_with_weight\in.npy resnet50v2_with_weight\out.npy 0x1

# usage:
# rknn3_model_test.exe <model_path> <weight_path> [input_npy_paths] [golden-output_npy_paths] <core_mask> [loop_count] [key_path] [shape_id]
```

参数说明：
1. model_path: rknn 文件路径
2. weight_path: weight 文件路径
3. input_npy_paths: 输入 `npy` 文件路径，如果有多个输入，用 '#' 分割，例如 'input1.npy#input2.npy'
4. golden_output_npy_paths: 正确输出的 `npy` 文件路径，用于准确性比较，如果有多个输出，用 '#' 分割，例如 'golden_out0.npy#golden_out1.npy'
5. core_mask: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是 0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3。
6. loop_count: 推理次数，可选。
7. key_path: 解密密钥文件路径，可选。使用 `none` 跳过解密。
8. shape_id: 动态 shape 模型的 shape ID，可选，默认为 0。

**注：**
1. **如果只想测试模型是否能正常推理，可以只填 `model_path` 和 `weight_path`，推理时将使用随机输入数据，且不对结果进行校验，`core_mask` 根据查询到的核心数自动生成，`loop_count` 使用默认值 1。**
2. **如果既想使用随机输入，又想设置 `core_mask` 或 `loop_count`，可以将 `input_npy_paths` 和 `golden_output_npy_paths`都设成空字符串。举例如下：**
```sh
rknn3_model_test.exe resnet50v2_with_weight\resnet50v2.rknn resnet50v2_with_weight\resnet50v2.weight "" "" 0x1 1000
```
3. **对于动态 shape 模型，可以通过 `shape_id` 参数指定要使用的 shape 组合，默认使用 shape 0。举例如下：**
```sh
rknn3_model_test.exe model.rknn model.weight "" "" 0x1 1 none 1
```
4. **Windows 平台下路径分隔符使用反斜杠 `\`，但在 Cygwin 终端中仍可使用正斜杠 `/`**
