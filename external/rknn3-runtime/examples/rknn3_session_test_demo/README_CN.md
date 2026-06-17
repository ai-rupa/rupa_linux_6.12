# Session 测试用例说明
此工程提供 4 个 Session 测试用例：

1. rknn3_session_test  
   通用 Session 推理示例，用于演示基于典型的 prompt 输入的推理流程。
2. rknn3_session_test_token_embed_input  
   基于 token/embed 输入接口的推理示例，可用于非 prompt 输入类型的推理场景。
3. rknn3_session_test_eval_perf  
   性能评估示例，用于评估 LLM 模型的 TPS/TTFT 等核心性能指标。
4. rknn3_session_test_function_call  
   支持 Function Calling 特性的推理示例，用于演示大模型在实际函数调用场景下的推理方式。

## Linux 平台使用示例

### 编译

```sh
# 请先指定编译器路径
(optional)export GCC_COMPILER=<GCC_COMPILER_PATH>

./build-linux.sh -t <TARGET_PLATFORM> -a <ARCH> [-b <build_type>]

# 例如
./build-linux.sh -t rk3588 -a aarch64 -b Release
```

### 推送到板端

```sh
adb push install/rknn3_llm_test_<TARGET_PLATFORM>_Linux/ /data/
```

### 运行

```sh
adb shell
cd /data/rknn3_llm_test_<TARGET_PLATFORM>_Linux

export LD_LIBRARY_PATH=./lib

# Usage: ./rknn3_session_test <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff

# Usage: ./rknn3_session_test_token_embed_input <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test_token_embed_input Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff

# Usage: ./rknn3_session_test_eval_perf <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <n_input_tokens> <max_new_tokens> <core_mask>
./rknn3_session_test_eval_perf Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 128 128 0xff

# Usage: ./rknn3_session_test_function_call <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test_function_call Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff
```
参数说明: 
- `rknn_path`: rknn 文件路径
- `weight_path`: weight 文件路径
- `tokenizer.gguf`: tokenizer.gguf 文件路径
- `embedding.bin`: embedding.bin 文件路径
- `max_context_len`: 模型转换时 `rknn.config` 中配置的 max_ctx_len 值
- `n_input_tokens`: 输入的 token 数
- `max_new_tokens`: 每次会话最多生成的 token 数
- `core_mask`: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3


## Android 平台使用示例

### 编译

```sh
# 请先指定编译器路径
(optional)export ANDROID_NDK_PATH=<ANDROID_NDK_PATH>

./build-android.sh -t <TARGET_PLATFORM> -a <ARCH> [-b <build_type>]

# 例如
./build-android.sh -t rk3588 -a arm64-v8a -b Release
```

### 推送到板端

```sh
adb root
adb remount
adb push install/rknn3_llm_test_<TARGET_PLATFORM>_Android/ /data/
```

### 运行

```sh
adb shell
cd /data/rknn3_llm_test_<TARGET_PLATFORM>_Linux

export LD_LIBRARY_PATH=./lib

# Usage: ./rknn3_session_test <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff

# Usage: ./rknn3_session_test_token_embed_input <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test_token_embed_input Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff

# Usage: ./rknn3_session_test_eval_perf <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <n_input_tokens> <max_new_tokens> <core_mask>
./rknn3_session_test_eval_perf Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 128 128 0xff

# Usage: ./rknn3_session_test_function_call <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>
./rknn3_session_test_function_call Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 2048 512 0xff
```
参数说明: 
- `rknn_path`: rknn 文件路径
- `weight_path`: weight 文件路径
- `tokenizer.gguf`: tokenizer.gguf 文件路径
- `embedding.bin`: embedding.bin 文件路径
- `max_context_len`: 模型转换时 `rknn.config` 中配置的 max_ctx_len 值
- `n_input_tokens`: 输入的 token 数
- `max_new_tokens`: 每次会话最多生成的 token 数
- `core_mask`: 目前有 8 个核，对应 8 bit 数，使用哪一个核，就将哪一位置 1，例如使用核 0 和核 1，就将第 0 位和第 1 位置 1，得到的二进制数是0b11，对应的十六进制数是 0x3，core_mask 设置成 0x3