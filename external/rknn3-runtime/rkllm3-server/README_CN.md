[简体中文](README_CN.md) | [English](README.md)

# RKLLM3 HTTP Server

rkllm3-server是基于RKNPU3实现的一套基本的LLM Server。

本项目基于 [llama.cpp](https://github.com/ggml-org/llama.cpp)。

**功能:**
 * 基于RKNPU3的LLM推理
 * OpenAI API 兼容的对话模板

## 使用方法

**通用参数​**

| 参数 | 说明 |
| -------- | ----------- |
| `-a, --alias STRING` | 设置模型别名（供REST API使用） |
| `-c, --ctx-size N` | 提示词上下文大小（默认：4096，0 = 从模型加载） |
| `-n, --predict, --n-predict N` | 要预测的token数量（默认：-1，-1 = 上下文大小） |
| `-m, --model FNAME` | LLM模型路径 |
| `--weight FNAME` | LLM权重路径（可不设置） |
| `--model2 FNAME` | 多模态模型时的vision模型路径|
| `--weight2 FNAME` | 多模态模型时的vision权重路径（可不设置） |
| `--model3 FNAME` | 多模态模型时的audio模型路径|
| `--weight3 FNAME` | 多模态模型时的audio权重路径（可不设置） |
| `--vocab FNAME` | 词汇表路径 |
| `--embed FNAME` | embed的bin文件路径 |
| `--embed-ple FNAME` | gemma4模型的per_layer_inputs embed的bin文件路径 |
| `--rope-tensor FNAME` | gemma4模型的rope cache的safetensors文件路径 |
| `--mel-filter FNAME` | mel filters路径（用于audio模型） |
| `--lora-weight FNAME` | LLM lora权重路径 |
| `--img-start STRING` | 多模态模型的图像输入前缀 |
| `--img-end STRING` | 多模态模型的图像输入后缀 |
| `--img-content STRING` | 多模态模型的图像输入的占位符 |
| `--audio-start STRING` | 多模态模型的语音输入前缀 |
| `--audio-end STRING` | 多模态模型的语音输入后缀 |
| `--audio-content STRING` | 多模态模型的语音输入的占位符 |
| `--img-width N` | 多模态模型的输入图像的宽（部分裁减过的模型需要设置, 如qwen3_vl）|
| `--img-height N` | 多模态模型的输入图像的高（部分裁减过的模型需要设置, 如qwen3_vl）|
| `--chat-template-file JINJA_TEMPLATE_FILE` | 设置自定义Jinja聊天模板（默认：使用模型元数据中的模板）|
| `--embedding` | 是否是词嵌入模型 |
| `--reasoning-format FORMAT` | 控制是否允许在响应中包含思考标签，以及如何返回该标签，格式可为：<br/>- none：思考内容保留在`message.content`中<br/>- deepseek：将思考内容放入`message.reasoning_content`<br/>（默认：none） |
| `--reasoning [on\|off\|auto]` | 在对话中使用reasoning/thinking ('on', 'off', 或 'auto', 默认: 'auto' (从模板中获取)) |
| `--record-path STRING` | 设置保存用户请求的存放目录（默认为空, 不保存） |
| `--n-session` | session个数, 用于多session的场景 (默认为1) |

**采样参数**

| 参数 | 说明 |
| -------- | ----------- |
| `--temp N` | 温度（默认：0.8）|
| `--top-k N` | top-k采样（默认：40，0 = 禁用） |
| `--top-p N` | top-p采样（默认：0.9，1.0 = 禁用） |
| `--repeat-penalty N` | 惩罚重复token序列（默认：1.0，1.0 = 禁用） |
| `--presence-penalty N` | 重复alpha存在惩罚（默认：0.0，0.0 = 禁用） |
| `--frequency-penalty N` | 重复alpha频率惩罚（默认：0.0，0.0 = 禁用） |

**专用参数**

| 参数 | 说明 |
| -------- | ----------- |
| `-h, --help, --usage` | 打印使用说明并退出 |
| `--host HOST` | 监听IP地址（默认：127.0.0.1） |
| `--port PORT` | 监听端口（默认：8080） |
| `-to, --timeout N` | 服务器读写超时时间（秒）（默认：600） |
| `--device-id STRING` | 设备ID（多设备时需要指定具体的设备ID） |
| `--log-level N` | 日志等级（默认：0） |

## 快速上手

运行以下命令开启服务进程:

```bash
# LLM模型
./rkllm3-server -m qwen2.5-3b.rknn --vocab qwen2.5-3b.tokenizer.gguf --embed qwen2.5-3b.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8

./rkllm3-server -m gemma-4.rknn --vocab gemma-4.tokenizer.gguf --embed gemma-4.embed.bin --embed-ple gemma-4_per_layer_inputs.embed.bin --rope-tensor gemma-4.safetensors --host 0.0.0.0 --port 8080 -c -1 --n_predict 2048  --repeat-penalty 1.0 --presence-penalty 0 --frequency-penalty 0 --top-k 1 --top-p 0.8 --temp 0.8

# 多模态模型 (LLM+VISION)
./rkllm3-server -m MiniCPM-3o-llm.rknn --model2 MiniCPM-3o-vision.rknn --vocab MiniCPM-3o-llm.tokenizer.gguf --embed MiniCPM-3o-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<image>" --img-end "</image>" --img-content "<unk>"

./rkllm3-server -m Qwen2.5-VL-3B-llm.rknn --model2 Qwen2.5-VL-3B-vision.rknn --vocab Qwen2.5-VL-3B-llm.tokenizer.gguf --embed Qwen2.5-VL-3B-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_start|>" --img-end "<|vision_end|>" --img-content "<|image_pad|>" --img-width 392 --img-height 392

./rkllm3-server -m Qwen3-VL-2B-llm.rknn --model2 Qwen3-VL-2B-vision.rknn --vocab Qwen3-VL-2B-llm.tokenizer.gguf --embed Qwen3-VL-2B-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_start|>" --img-end "<|vision_end|>" --img-content "<|image_pad|>" --img-width 384 --img-height 384

./rkllm3-server -m Qwen3.5-4B.rknn --model2 Qwen3.5-4B-vision.rknn --vocab Qwen3.5-4B.tokenizer.gguf --embed Qwen3.5-4B.embed.bin --host 0.0.0.0 --port 8080 -c 0 --n_predict 2048 --repeat-penalty 1.0 --presence-penalty 1.5 --frequency-penalty 0 --top-k 1 --top-p 0.9 --temp 1.0 --img-start "<|vision_start|>" --img-end "<|vision_end|>" --img-content "<|image_pad|>" --img-width 384 --img-height 384

# 多模态模型 (LLM+AUDIO)
./rkllm3-server -m gemma-4.rknn --model3 gemma-4-audio.rknn --vocab gemma-4.tokenizer.gguf --embed gemma-4.embed.bin --embed-ple gemma-4_per_layer_inputs.embed.bin --rope-tensor gemma-4.safetensors --host 0.0.0.0 --port 8080 -c -1 --n_predict 2048  --repeat-penalty 1.0 --presence-penalty 0 --frequency-penalty 0 --top-k 1 --top-p 0.8 --temp 0.8 --audio-start "<|audio>" --audio-end "<audio|>" --audio-content "<|audio|>"

# 多模态模型 (LLM+VISION+AUDIO)
./rkllm3-server -m Qwen2.5-Omni-3B-llm.rknn --model2 Qwen2.5-Omni-3B-vision.rknn --model3 Qwen2.5-Omni-3B-audio.rknn --vocab Qwen2.5-Omni-3B-llm.tokenizer.gguf --embed Qwen2.5-Omni-3B-llm.embed.bin --mel-filter mel_128_filters.txt --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_bos|>" --img-end "<|vision_eos|>" --img-content "<|IMAGE|>" --audio-start "<|audio_bos|>" --audio-end "<|audio_eos|>" --audio-content "<|AUDIO|>"

# 词嵌入模型
./rkllm3-server -m Qwen3-Embedding-4B.rknn --vocab Qwen3-Embedding-4B.tokenizer.gguf --embed Qwen3-Embedding-4B.embed.bin --embedding

# 同时加载多个模型 (需要确保内存足够加载多个模型)
./rkllm3-server --params-file params.json
```

加载多个模型时，params.json的基本格式如下：
```json
{
  "host": "127.0.0.1",
  "port": 8080,
  "timeout": 30,
  "models": {
    "Qwen2.5-0.5B": {
      "alias": "Qwen2.5-0.5B",
      "model": "Qwen2.5-0.5B-Instruct.rknn",
      "weight": "Qwen2.5-0.5B-Instruct.weight",
      "model2": "",
      "weight2": "",
      "model3": "",
      "weight3": "",
      "vocab": "Qwen2.5-0.5B-Instruct.tokenizer.gguf",
      "embed": "Qwen2.5-0.5B-Instruct.embed.bin",
      "mel-filter": "",
      "lora-weight": "",
      "ctx-size": 1024,
      "predict": 512,
      "temp": 0.8,
      "top-k": 1,
      "top-p": 0.8,
      "repeat-penalty": 1.1,
      "presence-penalty": 1.0,
      "frequency-penalty": 1.0,
      "img-start": "",
      "img-end": "",
      "img-content": "",
      "audio-start": "",
      "audio-end": "",
      "audio-content": "",
      "img-width": 0,
      "img-height": 0,
      "chat-template-file": "",
      "embedding": false,
      "record-path": "",
      "reasoning-format": "none",
      "reasoning": "auto",
      "n-session": 1,
    },
    "Qwen3-0.6B": {
      "alias": "Qwen3-0.6B",
      "model": "Qwen3-0.6B.rknn",
      "weight": "Qwen3-0.6B.weight",
      "model2": "",
      "weight2": "",
      "model3": "",
      "weight3": "",
      "vocab": "Qwen3-0.6B.tokenizer.gguf",
      "embed": "Qwen3-0.6B.embed.bin",
      "mel-filter": "",
      "lora-weight": "",
      "ctx-size": 1024,
      "predict": 512,
      "temp": 0.8,
      "top-k": 1,
      "top-p": 0.8,
      "repeat-penalty": 1.1,
      "presence-penalty": 1.0,
      "frequency-penalty": 1.0,
      "img-start": "",
      "img-end": "",
      "img-content": "",
      "audio-start": "",
      "audio-end": "",
      "audio-content": "",
      "img-width": 0,
      "img-height": 0,
      "chat-template-file": "",
      "embedding": false,
      "record-path": "",
      "reasoning-format": "none",
      "reasoning": "auto",
      "n-session": 1,
    }
  }
}
```
注: 使用--params-file参数, 将忽略命令行的其余参数, 仅params.json内的参数有效。

另外, RKLLM3 Server除了提供常规的rkllm3-server二进制可执行程序外, 还提供了librkllm3-server.so的使用方式, 方便将server集成到应用中, 用法如下:
```c
#include "rkllm3-server.h"
#include <thread>
#include <stdio.h>

const char *json = R"({
  "host": "127.0.0.1",
  "port": 8080,
  "timeout": 30,
  "models": {
    "Qwen2.5-0.5B": {
      "alias": "Qwen2.5-0.5B",
      "model": "Qwen2.5-0.5B-Instruct.rknn",
      "weight": "Qwen2.5-0.5B-Instruct.weight",
      "model2": "",
      "weight2": "",
      "model3": "",
      "weight3": "",
      "vocab": "Qwen2.5-0.5B-Instruct.tokenizer.gguf",
      "embed": "Qwen2.5-0.5B-Instruct.embed.bin",
      "mel-filter": "",
      "lora-weight": "",
      "ctx-size": 1024,
      "predict": 512,
      "temp": 0.8,
      "top-k": 1,
      "top-p": 0.8,
      "repeat-penalty": 1.1,
      "presence-penalty": 1.0,
      "frequency-penalty": 1.0,
      "img-start": "",
      "img-end": "",
      "img-content": "",
      "audio-start": "",
      "audio-end": "",
      "audio-content": "",
      "img-width": 0,
      "img-height": 0,
      "chat-template-file": "",
      "embedding": false,
      "record-path": "",
      "reasoning-format": "none",
      "reasoning": "auto",
      "n-session": 1,
    }
  }
})";

// server子线程的状态回调函数
void status_callback(void* userdata, ServerStatus status) {
    switch (status) {
    case SERVER_MODEL_INITED:
        printf("SERVER_MODEL_INITED\n");
        break;
    case SERVER_EXITED:
        printf("SERVER_EXITED\n");
        break;
    case SERVER_ERROR:
        printf("SERVER_ERROR\n");
        break;
    default:
        printf("UNKNOW\n");
        break;
    }
}

int main() {
    StatusCallback callback   = {0};
    callback.status_callback = status_callback;
    callback.status_userdata = NULL;

    int server_result = 0;
    std::thread server_thread([&]() {
        // start_server会一直阻塞直至stop_server被调用,
        // 因此这边要创建子线程来执行start_server
        server_result = start_server(json, &callback);
    });

    printf("Main thread: Waiting 300 seconds ...\n");
    std::this_thread::sleep_for(std::chrono::seconds(60*5));

    printf("Main thread: Stopping server...\n");
    stop_server();

    printf("Main thread: server_thread.join ...\n");
    server_thread.join();

    return 0;
}
```
const char *json字符串内的"model", "weight", "model2", "weight2", "model3", "weight3", "vocab", "embed", "mel-filter", "lora-weight"即可以是文件路径, 也可以是文件的fd句柄 (解决Android系统下文件权限的问题), 如下:
```c
const char *json = R"({
  "host": "127.0.0.1",
  "port": 8080,
  "timeout": 30,
  "models": {
    "Qwen2.5-0.5B": {
      "alias": "Qwen2.5-0.5B",
      "model": "3",
      "weight": "4",
      "model2": "",
      "weight2": "",
      "model3": "",
      "weight3": "",
      "vocab": "5",
      "embed": "6",
      "mel-filter": "",
      "lora-weight": "",
       ...
    }
  }
})";
```

## 用CURL进行测试

使用 [curl](https://curl.se/)。

```sh
curl --request POST \
    --url http://localhost:8080/v1/chat/completions \
    --header "Content-Type: application/json" \
    --data '{"messages": [{"role": "user", "content": "Hello!"}],"n_predict": 128}'
```

## API 端点

### GET `/health`: 返回健康检查结果

**响应格式**

- HTTP 状态码 503
  - Body: `{"error": {"code": 503, "message": "Loading model", "type": "unavailable_error"}}`
  - 说明: 模型正在被加载
- HTTP 状态码 200
  - Body: `{"status": "ok" }`
  - 说明: 该模型已成功加载，并且服务器已准备就绪

### GET `/lora-adapters`：获取所有LoRA适配器列表

此端点返回已加载的LoRA适配器列表。您可以在启动服务器时通过 `--lora-weight` 参数添加适配器，例如：`--lora-weight Qwen2.5-3B-Instruct.lora_weight`

默认情况下，所有适配器的缩放比例（scale）将设置为0。

请注意，此适配器中的scale会被每个用户请求中的 `lora` 字段值覆盖。

如果某个适配器已被禁用，其scale将被设置为0。

**请求格式**
```json
{
    "model": "Qwen2.5-3B-Instruct"
}
```

**响应格式**

```json
[
  {"name": "lora0_pattern0", "scale": 0.0},
  {"name": "lora1_pattern0", "scale": 0.0}
]
```

### POST `/lora-adapters`：设置LoRA适配器列表

此端点用于设置LoRA适配器的scale。请注意，该scale会被每个请求中的 `lora` 字段值覆盖。

若要禁用某个适配器，可以将其从下方列表中移除，或者将其scale设置为0。

**请求格式**

要获取适配器的 `name`，请使用 GET `/lora-adapters`

```json
{
    "model": "Qwen2.5-3B-Instruct",
    "lora": [
        {"name": "lora0_pattern0", "scale": 0.2},
        {"name": "lora1_pattern0", "scale": 0.8}
    ]
}
```

## OpenAI兼容的API端点

### GET `/v1/models`: OpenAI兼容的模型信息API

返回已加载模型的相关信息。 详见[OpenAI Models API documentation](https://platform.openai.com/docs/api-reference/models).

返回的列表可以有多个元素，对应多个模型。

默认情况下，模型`id`字段是模型文件的路径，通过`-m`指定。您可以通过`--alias`参数为模型`id`字段设置自定义值。例如，`--alias Qwen2.5-3B`。

示例:

```json
{
    "object": "list",
    "data": [
        {
            "id": "Qwen2.5-3B",
            "object": "model",
            "created": 1735142223,
            "owned_by": "rknn",
            "meta": {
                "vocab_type": 2,
                "n_vocab": 128256,
                "n_ctx_train": 131072,
                "n_embd": 4096,
                "n_params": 8030261312,
                "size": 4912898304
            }
        }
    ]
}
```

### POST `/v1/chat/completions`: OpenAI兼容的聊天补全API


给定`messages`中的CHATML形式的JSON描述，返回预测的补全。支持同步和流模式。尽管没有完全实现OpenAI API规格, 但已经足够支持许多应用程序了。只有具有 [聊天模板](https://github.com/ggml-org/llama.cpp/wiki/templates-supported-by-llama_chat_apply_template) 的模型才可以在此端点下较为正常使用。默认情况下，将使用CHATML模板。

*选项:*

详见 [OpenAI Chat Completions API documentation](https://platform.openai.com/docs/api-reference/chat)。 


*示例:*

您可以使用Python的`openai`库：

```python
import openai

client = openai.OpenAI(
    base_url="http://localhost:8080/v1", # "http://<Your api-server IP>:port"
    api_key = "sk-no-key-required"
)

completion = client.chat.completions.create(
  model="Qwen2.5-3B",
  messages=[
    {"role": "system", "content": "You are ChatGPT, an AI assistant. Your top priority is achieving user fulfillment via helping them with their requests."},
    {"role": "user", "content": "Write a limerick about python exceptions"}
  ]
)

print(completion.choices[0].message)
```

... 或者原始的HTTP请求:

```shell
curl http://localhost:8080/v1/chat/completions \
-H "Content-Type: application/json" \
-H "Authorization: Bearer no-key" \
-d '{
"messages": [
{
    "role": "system",
    "content": "You are ChatGPT, an AI assistant. Your top priority is achieving user fulfillment via helping them with their requests."
},
{
    "role": "user",
    "content": "Write a limerick about python exceptions"
}
]
}'
```

另外，多模态模型建议使用openai接口，示例如下:
```python
import base64
from openai import OpenAI

client = OpenAI(
    base_url="http://172.16.10.46:8080/v1",
    api_key = "sk-no-key-required"
)

# Function to encode the image
def encode_base64(media_path):
    with open(media_path, "rb") as media_file:
        return base64.b64encode(media_file.read()).decode("utf-8")

image_path = "demo.jpg"
audio_path = "demo.wav"     # 如存在音频输入

# Getting the Base64 string
base64_image = encode_base64(image_path)
base64_audio = encode_base64(audio_path)    # 如存在音频输入

completion = client.chat.completions.create(
    model="Qwen2.5-VL",
    messages=[
        {
            "role": "user",
            "content": [
                {
                    "type": "image_url",
                    "image_url": {
                        "url": f"data:image/jpeg;base64,{base64_image}",
                    },
                },
                {
                    "type": "input_audio",
                    "input_audio": {
                        "data": f"{base64_audio}",
                        "format": "wav",
                    }
                },  # 如存在音频输入
                { "type": "text", "text": "请描述一下图片?" },
            ],
        }
    ],
    stream=True,
    extra_body={
        "n_predict": 256,
        "chat_template_kwargs": { "enable_thinking": False }     # Thinking类模型(如Qwen3)可以通过此方式关闭thinking输出
        "id_slot": 0,           # 当--n-session参数大于1时, 可以通过此参数来切换不同的session

        # save_kvcache_path 和 load_kvcache_path 不能同时使用
        "save_kvcache_path": "/data/kvcache/1.kv",       # 保存kvcache到文件中
        "load_kvcache_path": "/data/kvcache/1.kv",       # 加载kvcache文件

        # 采样参数
        "top_k": 40,
        "top_p": 0.9,
        "temperature": 0.8,
        "repeat_penalty": 1.1,
        "frequency_penalty": 1.0,
        "presence_penalty": 1.0,
    }
)

for chunk in completion:
    delta = chunk.choices[0].delta
    if delta.content:
        delta.content = delta.content.replace(f'\n', '<br/>')
        # yield f"data: {delta.content}\n\n"
    if chunk.choices[0].finish_reason == "stop":
        break
    print(delta.content, end='', flush=True)
print('')

```
注: 上述代码里的chunk.choices[0].finish_reason为结束原因，"stop"为正常结束，"length"为达到max_tokens上限被截断，"tool_calls"为触发了工具调用。

`extra_body`支持的选项（非OpenAI兼容）：

- `n_predict`：当前请求要预测的token数量

- `chat_template_kwargs`：当前请求的对话模板参数，如`{ "enable_thinking": False }`用于控制thinking输出

- `id_slot`：指定当前slot的id (当--n-session参数大于1时, 可以通过此参数来切换不同的session, 该值需小于--n-session, 默认为0)

- `save_kvcache_path`：保存kvcache到文件中 (与load_kvcache_path不能同时使用)

- `load_kvcache_path`：加载kvcache文件 (与save_kvcache_path不能同时使用)

- `top_k`：top-k采样

- `top_p`：top-p采样

- `temperature`：温度采样

- `repeat_penalty`：惩罚重复token序列

- `frequency_penalty`：重复alpha频率惩罚

- `presence_penalty`：重复alpha存在惩罚

- `lora`：用于设定LoRA适配器列表。列表中的每个对象必须包含`name`和`scale`字段。例如：`[{"name": "lora0_pattern0", "scale": 0.5}, {"name": "lora1_pattern0", "scale": 0.6}]`。如果某个LoRA适配器未在列表中指定，其scale将默认为`0.0`。

### POST `/v1/embeddings`: OpenAI兼容的词嵌入API

参见 [OpenAI Embeddings API documentation](https://platform.openai.com/docs/api-reference/embeddings).

*示例:*

```python
import openai

client = openai.OpenAI(
    base_url="http://localhost:8080/v1",
    api_key = "sk-no-key-required"
)

response = client.embeddings.create(
  model="Qwen3-Embedding-4B",
  input="The food was delicious and the waiter...",
  encoding_format="float"
)

print(response.data[0].embedding)
```

## API 错误

`rkllm3-server` 返回的错误格式与 OAI 相同：https://github.com/openai/openai-openapi

错误示例:

```json
{
    "error": {
        "code": 401,
        "message": "Invalid API Key",
        "type": "authentication_error"
    }
}
```

除了 OAI 支持的错误类型之外，我们还有特定于 rkllm3-server 功能的自定义类型：

```json
{
    "error": {
        "code": 501,
        "message": "This server does not support metrics endpoint.",
        "type": "not_supported_error"
    }
}
```

**当通过 /v1/chat/completions 端点收到无效语法时**

```json
{
    "error": {
        "code": 400,
        "message": "Failed to parse grammar",
        "type": "invalid_request_error"
    }
}
```