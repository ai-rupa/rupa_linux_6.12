# RKLLM3 HTTP Server

rkllm3-server is a basic LLM Server implementation based on RKNPU3.

**Features:**
 * LLM inference based on RKNPU3
 * OpenAI API compatible chat template

## Usage

**Common params**

| Argument | Explanation |
| -------- | ----------- |
| `-a, --alias STRING` | Set model alias (for REST API) |
| `-c, --ctx-size N` | Prompt context size (default: 4096, 0 = load from model) |
| `-n, --predict, --n-predict N` | Number of tokens to predict (default: -1, -1 = context size) |
| `-m, --model FNAME` | RKNN LLM model path |
| `--weight FNAME` | RKNN LLM weight path (optional) |
| `--model2 FNAME` | Vision model path for multimodal models |
| `--weight2 FNAME` | Vision weight path for multimodal models (optional) |
| `--model3 FNAME` | Audio model path for multimodal models |
| `--weight3 FNAME` | Audio weight path for multimodal models (optional) |
| `--vocab FNAME` | Vocabulary path |
| `--embed FNAME` | Embed.bin path |
| `--mel-filter FNAME` | Mel filters path (for audio models) |
| `--img-start STRING` | Image input prefix for multimodal models |
| `--img-end STRING` | Image input suffix for multimodal models |
| `--img-content STRING` | Image input pad for multimodal models |
| `--audio-start STRING` | Audio input prefix for multimodal models |
| `--audio-end STRING` | Audio input suffix for multimodal models |
| `--audio-content STRING` | Audio input pad for multimodal models |
| `--img-width N` | Input image width for multimodal models（Some pruned models require configuration, such as qwen3_vl）|
| `--img-height N` | Input image height for multimodal models（Some pruned models require configuration, such as qwen3_vl）|
| `--chat-template-file JINJA_TEMPLATE_FILE` | Set custom Jinja chat template (default: use template from model metadata) |
| `--embedding` | Is it a embedding model |

**Sampling params**

| Argument | Explanation |
| -------- | ----------- |
| `--temp N` | Temperature (default: 0.8) |
| `--top-k N` | Top-k sampling (default: 40, 0 = disabled) |
| `--top-p N` | Top-p sampling (default: 0.9, 1.0 = disabled) |
| `--repeat-penalty N` | Last n tokens to consider for penalize (default: 1.0, 1.0 = disabled) |
| `--presence-penalty N` | Repeat alpha presence penalty (default: 0.0, 0.0 = disabled) |
| `--frequency-penalty N` | Repeat alpha frequency penalty (default: 0.0, 0.0 = disabled) |

**Example-specific params**

| Argument | Explanation |
| -------- | ----------- |
| `-h, --help, --usage` | Print usage and exit |
| `--host HOST` | Listening IP address (default: 127.0.0.1) |
| `--port PORT` | Listening port (default: 8080) |
| `-to, --timeout N` | Server read/write timeout in seconds (default: 600) |
| `--device-id STRING` | Device ID (When using multiple devices, a specific Device ID must be specified) |
| `--log-level N` | Log level (default: 0) |

## Quick Start

Run the following commands to start the server process:

```bash
# LLM Model
./rkllm3-server -m qwen2.5-3b.rknn --vocab qwen2.5-3b.tokenizer.gguf --embed qwen2.5-3b.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8

# Multimodal Model (LLM+VISION)
./rkllm3-server -m MiniCPM-3o-llm.rknn --model2 MiniCPM-3o-vision.rknn --vocab MiniCPM-3o-llm.tokenizer.gguf --embed MiniCPM-3o-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<image>" --img-end "</image>" --img-content "<unk>"

./rkllm3-server -m Qwen2.5-VL-3B-llm.rknn --model2 Qwen2.5-VL-3B-vision.rknn --vocab Qwen2.5-VL-3B-llm.tokenizer.gguf --embed Qwen2.5-VL-3B-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_start|>" --img-end "<|vision_end|>" --img-content "<|image_pad|>" --img-width 392 --img-height 392

./rkllm3-server -m Qwen3-VL-2B-llm.rknn --model2 Qwen3-VL-2B-vision.rknn --vocab Qwen3-VL-2B-llm.tokenizer.gguf --embed Qwen3-VL-2B-llm.embed.bin --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_start|>" --img-end "<|vision_end|>" --img-content "<|image_pad|>" --img-width 384 --img-height 384

# Multimodal Model (LLM+VISION+AUDIO)
./rkllm3-server -m Qwen2.5-Omni-3B-llm.rknn --model2 Qwen2.5-Omni-3B-vision.rknn --model3 Qwen2.5-Omni-3B-audio.rknn --vocab Qwen2.5-Omni-3B-llm.tokenizer.gguf --embed Qwen2.5-Omni-3B-llm.embed.bin --mel-filter mel_128_filters.txt --host 0.0.0.0 --port 8080 -c 768 --n_predict 512  --repeat-penalty 1.1 --presence-penalty 1.0 --frequency-penalty 1.0 --top-k 1 --top-p 0.8 --temp 0.8 --img-start "<|vision_bos|>" --img-end "<|vision_eos|>" --img-content "<|IMAGE|>" --audio-start "<|audio_bos|>" --audio-end "<|audio_eos|>" --audio-content "<|AUDIO|>"

# Embedding Model
./rkllm3-server -m Qwen3-Embedding-4B.rknn --vocab Qwen3-Embedding-4B.tokenizer.gguf --embed Qwen3-Embedding-4B.embed.bin --embedding

# Load multiple models simultaneously (ensure sufficient memory to load multiple models)
./rkllm3-server --params-file params.json
```

When loading multiple models, the basic format of params.json is as follows:
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
      "embedding": false
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
      "embedding": false
    }
  }
}
```
Note: Using the `--params-file` parameter will ignore other command-line arguments; only the parameters within params.json are effective.

Additionally, besides providing the regular rkllm3-server binary executable, RKLLM3 Server also offers a usage method via `librkllm3-server.so`, facilitating the integration of the server into applications. The usage is as follows:
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
      "embedding": false
    }
  }
})";

// Status callback function for the server sub-thread
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
    std::thread server_thread( {
        // start_server blocks until stop_server is called,
        // so it must be executed in a sub-thread.
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
The strings for "model", "weight", "model2", "weight2", "model3", "weight3", "vocab", "embedding", "mel-filter" inside the `const char *json` can be either file paths or file descriptor handles (to address file permission issues on Android systems), as shown below:
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
       ...
    }
  }
})";
```

## Testing with CURL

Use https://curl.se/.

```sh
curl --request POST \
    --url http://localhost:8080/v1/chat/completions \
    --header "Content-Type: application/json" \
    --data '{"messages": [{"role": "user", "content": "Hello!"}],"n_predict": 128}'
```

## API Endpoints

### GET `/health`: Returns health check result

**Response Format**

- HTTP Status Code 503
  - Body: `{"error": {"code": 503, "message": "Loading model", "type": "unavailable_error"}}`
  - Description: Model is being loaded
- HTTP Status Code 200
  - Body: `{"status": "ok" }`
  - Description: The model has been successfully loaded and the server is ready

## OpenAI Compatible API Endpoints

### GET `/v1/models`: OpenAI compatible model information API

Returns information about the loaded model(s). See https://platform.openai.com/docs/api-reference/models.

The returned list can have multiple elements, corresponding to multiple models.

By default, the model `id` field is the model file path specified by `-m`. You can set a custom value for the model `id` field using the `--alias` parameter. For example, `--alias Qwen2.5-3B`.

Example:

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

### POST `/v1/chat/completions`: OpenAI compatible chat completions API


Given a JSON description of CHATML form in `messages`, returns the predicted completion. Supports both synchronous and streaming modes. Although not fully implementing the OpenAI API specification, it is sufficient to support many applications. Only models with a https://github.com/ggml-org/llama.cpp/wiki/templates-supported-by-llama_chat_apply_template can be used relatively normally under this endpoint. By default, the CHATML template will be used.

*Options:*

See https://platform.openai.com/docs/api-reference/chat.

The `response_format` parameter supports plain JSON output (e.g., `{"type": "json_object"}`) and JSON with format constraints (e.g., `{"type": "json_object", "schema": {"type": "string", "minLength": 10, "maxLength": 100}}` or `{"type": "json_schema", "schema": {"properties": { "name": { "title": "Name",  "type": "string" }, "date": { "title": "Date",  "type": "string" }, "participants": { "items": {"type: "string" }, "title": "Participants",  "type": "string" } } } }`).

*Example:*

You can use the Python `openai` library:

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

... or a raw HTTP request:

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

Additionally, for multimodal models, it is recommended to use the openai interface. Example:
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
audio_path = "demo.wav"     # If audio input exists

# Getting the Base64 string
base64_image = encode_base64(image_path)
base64_audio = encode_base64(audio_path)    # If audio input exists

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
                },  # If audio input exists
                { "type": "text", "text": "请描述一下图片?" },
            ],
        }
    ],
    stream=True,
    extra_body={
        "n_predict": 256,
        "chat_template_kwargs": { "enable_thinking": False }     # For Thinking models (like Qwen3), thinking output can be disabled this way
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

### POST `/v1/embeddings`: OpenAI-compatible embeddings API

See [OpenAI Embeddings API documentation](https://platform.openai.com/docs/api-reference/embeddings).

*Examples:*

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

## API Errors

Errors returned by `rkllm3-server` follow the same format as OAI: https://github.com/openai/openai-openapi

Error example:

```json
{
    "error": {
        "code": 401,
        "message": "Invalid API Key",
        "type": "authentication_error"
    }
}
```

In addition to the error types supported by OAI, we have custom types specific to rkllm3-server functionality:

```json
{
    "error": {
        "code": 501,
        "message": "This server does not support metrics endpoint.",
        "type": "not_supported_error"
    }
}
```

**When invalid syntax is received via the /v1/chat/completions endpoint**

```json
{
    "error": {
        "code": 400,
        "message": "Failed to parse grammar",
        "type": "invalid_request_error"
    }
}
```