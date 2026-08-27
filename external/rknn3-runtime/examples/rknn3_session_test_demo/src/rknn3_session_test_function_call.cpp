// Copyright (c) 2025 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Tokenizer.h"
#include "float16.h"
#include "rknn3_api.h"
#include "rknn3_api.h"
#include <json.hpp>

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <regex>

#define LOGW(fmt, ...) printf("\033[33m" fmt "\033[0m", ##__VA_ARGS__)

rknn3_session *session = nullptr;

struct timeval start, first_token, end;
struct timeval decode;

static bool first_decode = true;

std::string model_answer;

static bool tool_call_output = false;

struct embedding_info
{
  int fd;
  float16 *embedding_data;
  int embedding_dim;
  int vocab_size;
};

typedef struct
{
  double temperature;
  std::string location;
  std::string date;
  std::string unit;
} TemperatureResult;

TemperatureResult get_current_temperature(const char *location, const char *unit)
{
  TemperatureResult result;
  result.temperature = 26.1;
  result.location = location;
  if (unit && strlen(unit) > 0)
  {
    result.unit = unit;
  }
  else
  {
    result.unit = "celsius";
  }
  result.date = "";
  return result;
}

TemperatureResult get_temperature_date(const char *location, const char *date, const char *unit)
{
  TemperatureResult result;
  result.temperature = 25.9;
  result.location = location;
  if (unit && strlen(unit) > 0)
  {
    result.unit = unit;
  }
  else
  {
    result.unit = "celsius";
  }
  result.date = date;
  return result;
}

void *get_function_by_name(const char *name)
{
  if (strcmp(name, "get_current_temperature") == 0)
  {
    return (void *)get_current_temperature;
  }
  if (strcmp(name, "get_temperature_date") == 0)
  {
    return (void *)get_temperature_date;
  }
  return NULL;
}

int argmax(const float16 *data, int size)
{
  if (size <= 0 || !data)
    return -1;

  int max_id = 0;
  for (int i = 1; i < size; i++)
  {
    if (fp16_to_fp32(data[i]) > fp16_to_fp32(data[max_id]))
    {
      max_id = i;
    }
  }
  return max_id;
}

int sampling_callback(void *userdata, float16 *logits, char *logits_name)
{
  struct embedding_info *embed_info = (struct embedding_info *)userdata;
  int size = embed_info->vocab_size;
  int max_id = argmax(logits, size);

  return max_id;
}

int result_callback(void *userdata, RKLLMResult *result, LLMCallState state)
{
  Tokenizer *tokenizer = (Tokenizer *)userdata;

  if (state == RKLLM_RUN_ERROR)
  {
    printf("\n\nError occurred during inference\n");
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_FINISH)
  {
    if (tool_call_output)
    {
      printf("\n\n--------------------Finished-------------------- \n");
    }
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_WAITING)
  {
    printf("\n\nWaiting for UTF-8 encoded character\n");
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_MAX_NEW_TOKEN_REACHED)
  {
    if (tool_call_output)
    {
      printf("\n\n--------------Max new token reached------------- \n");
    }
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_STOP)
  {
    if (tool_call_output)
    {
      printf("\n\n-----------------------Stop--------------------- \n");
    }
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_NORMAL)
  {
    // get token text
    std::string piece;
    if (result->num_tokens == 1)
    {
      piece = tokenizer->TokenToPiece(result->token_ids[0]);
    }
    else
    {
      piece = tokenizer->Decode(result->token_ids, result->num_tokens);
    }

    // print token text
    if (tool_call_output)
    {
      printf("%s", piece.c_str());
    }

    model_answer += piece;

    if (first_decode)
    {
      gettimeofday(&first_token, NULL);
      first_decode = false;
    }
    fflush(stdout);
  }
  return 0;
}

int tokenizer_callback(void *userdata, const char *text, int32_t text_len, int32_t *tokens, int32_t n_tokens_max)
{
  Tokenizer *tokenizer = (Tokenizer *)userdata;

  int n_tokens = tokenizer->Tokenize(text, text_len, tokens, n_tokens_max);

  if (n_tokens <= 0)
  {
    printf("tokenizer failed for %s\n", text);
    return n_tokens;
  }

  return n_tokens;
}

int embed_callback(void *userdata, int32_t *tokens, uint64_t num_tokens, void *embed, uint64_t len)
{
  struct embedding_info *embed_info = (struct embedding_info *)userdata;

  if (len != num_tokens * embed_info->embedding_dim * sizeof(float16))
  {
    printf("invalid embed buffer\n");
    return -1;
  }

  for (int n = 0; n < num_tokens; n++)
  {
    memcpy((unsigned char *)embed + n * embed_info->embedding_dim * sizeof(float16),
           embed_info->embedding_data + tokens[n] * embed_info->embedding_dim, embed_info->embedding_dim * sizeof(float16));
  }

  return 0;
}

int init_context_and_model(rknn3_context *p_ctx, const char *model_path, const char *weight_path, uint32_t core_mask, const char *device_id)
{
  int ret = 0;
  rknn3_config config;
  rknn3_context ctx = 0;

  // init context with device_id
  rknn3_init_extend init_extend = {0};
  init_extend.device_id = (char*)device_id;
  ret = rknn3_init(&ctx, &init_extend);
  if (ret < 0)
  {
    printf("rknn3_init failed! ret=%d\n", ret);
    return -1;
  }

  // load model
  ret = rknn3_load_model_from_path(ctx, model_path, weight_path);
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_load_model_from_data failed! ret=%d\n", ret);
    rknn3_destroy(ctx);
    return -1;
  }

  // set config
  memset(&config, 0, sizeof(config));
  config.run_core_mask = core_mask;

  // init model
  ret = rknn3_model_init(ctx, &config);
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_model_init failed! ret=%d\n", ret);
    rknn3_destroy(ctx);
    return -1;
  }

  *p_ctx = ctx;
  return 0;
}

int get_tokenizer_and_embedding(const char *tokenizer_path, VocabInfo *vocab_info, Tokenizer **tokenizer,
                                struct embedding_info *embedding_info, const char *embedding_path, struct stat *emb_st)
{
  *tokenizer = new Tokenizer(TOKENIZER_BACKEND_LLAMA, tokenizer_path);
  (*tokenizer)->GetVocabInfo(vocab_info);

  memset(embedding_info, 0x00, sizeof(struct embedding_info));
  embedding_info->fd = open(embedding_path, O_RDONLY);
  if (embedding_info->fd == -1)
  {
    printf("Failed to open embedding file: %s\n", embedding_path);
    delete *tokenizer;
    *tokenizer = nullptr;
    return -1;
  }

  if (fstat(embedding_info->fd, emb_st) == -1)
  {
    printf("Failed to get file size\n");
    close(embedding_info->fd);
    delete *tokenizer;
    *tokenizer = nullptr;
    return -1;
  }

  embedding_info->embedding_data = (float16 *)mmap(NULL, emb_st->st_size, PROT_READ, MAP_PRIVATE, embedding_info->fd, 0);
  if (embedding_info->embedding_data == MAP_FAILED)
  {
    printf("Failed to mmap file\n");
    close(embedding_info->fd);
    delete *tokenizer;
    *tokenizer = nullptr;
    return -1;
  }

  embedding_info->vocab_size = vocab_info->vocab_size;
  embedding_info->embedding_dim = (emb_st->st_size / vocab_info->vocab_size) / sizeof(float16);

  return 0;
}

void print_performance_statistics(const RKLLMRunState &state, const timeval &start, const timeval &first_token, const timeval &end)
{
  // calculate prefill time
  int prefill_n_tokens = state.n_prefill_tokens;
  float prefill_us = (first_token.tv_sec - start.tv_sec) * 1e6f + (first_token.tv_usec - start.tv_usec);
  float prefill_ms = prefill_us / 1e3f;
  float prefill_s = prefill_us / 1e6f;
  float prefill_tpt = prefill_n_tokens == 0 ? 0.0f : prefill_ms / prefill_n_tokens;
  float prefill_tps = prefill_n_tokens == 0 ? 0.0f : prefill_n_tokens / prefill_s;

  // calculate generate time
  int decode_n_tokens = state.n_decode_tokens;
  float decode_time_us = ((end.tv_sec - first_token.tv_sec) * 1e6f) + (end.tv_usec - first_token.tv_usec);
  float decode_ms = decode_time_us / 1e3f;
  float decode_s = decode_time_us / 1e6f;
  float decode_tpt = decode_n_tokens == 0 ? 0.0f : decode_ms / decode_n_tokens;
  float decode_tps = decode_n_tokens == 0 ? 0.0f : decode_n_tokens / decode_s;

  printf("\nPerformance Statistics:");
  printf("\n-----------------------------------------------------------------------------------------\n");
  printf(" %-10s | %-16s | %-8s | %-20s | %-20s \n", "Stage", "Total Time (ms)", "Tokens", "Time per Token (ms)", "Tokens per Second");
  printf("-----------------------------------------------------------------------------------------\n");
  printf(" %-10s | %-16.2f | %-8d | %-20.2f | %-20.2f \n", "Prefill", prefill_ms, prefill_n_tokens, prefill_tpt, prefill_tps);
  printf(" %-10s | %-16.2f | %-8d | %-20.2f | %-20.2f \n", "Generate", decode_ms, decode_n_tokens, decode_tpt, decode_tps);
  printf("-----------------------------------------------------------------------------------------\n");
}

static std::vector<std::string> extract_tool_call_list(const std::string &model_answer)
{
  // extract all <tool_call>...</tool_call> blocks
  std::vector<std::string> tool_call_list;

  std::string::size_type start_pos = 0;
  while (true)
  {
    // find <tool_call>
    std::string::size_type tag_start = model_answer.find("<tool_call>", start_pos);
    if (tag_start == std::string::npos)
      break;
    tag_start += std::string("<tool_call>").length();
    std::string::size_type tag_end = model_answer.find("</tool_call>", tag_start);
    if (tag_end == std::string::npos)
      break;
    // extract content, remove possible leading and trailing newlines
    std::string block = model_answer.substr(tag_start, tag_end - tag_start);
    // remove leading '\n' or space
    while (!block.empty() && (block[0] == '\n' || block[0] == '\r' || block[0] == ' '))
      block.erase(0, 1);
    // remove trailing '\n' or space
    while (!block.empty() && (block.back() == '\n' || block.back() == '\r' || block.back() == ' '))
      block.pop_back();
    tool_call_list.push_back(block);
    start_pos = tag_end + std::string("</tool_call>").length();
  }

  return tool_call_list;
}

static std::string get_tool_call_results_json(const std::vector<std::string> &tool_call_list)
{
  std::vector<std::string> json_results;

  for (const auto &tool_call : tool_call_list)
  {
    // extract fields using regular expressions
    std::string func_name;
    std::string location = "";
    std::string date = "";
    std::string unit = "";

    std::smatch m;
    std::string s = tool_call;

    std::regex name_re("\"name\"\\s*:\\s*\"([^\"]*)\"");
    std::regex location_re("\"location\"\\s*:\\s*\"([^\"]*)\"");
    std::regex date_re("\"date\"\\s*:\\s*\"([^\"]*)\"");
    std::regex unit_re("\"unit\"\\s*:\\s*\"([^\"]*)\"");

    if (std::regex_search(s, m, name_re) && m.size() > 1)
    {
      func_name = m[1].str();
    }
    if (std::regex_search(s, m, location_re) && m.size() > 1)
    {
      location = m[1].str();
    }
    if (std::regex_search(s, m, date_re) && m.size() > 1)
    {
      date = m[1].str();
    }
    if (std::regex_search(s, m, unit_re) && m.size() > 1)
    {
      unit = m[1].str();
    }

    // find function pointer
    typedef TemperatureResult (*TempFuncType)(const char *, const char *, const char *);
    void *func_ptr = get_function_by_name(func_name.c_str());
    if (func_ptr)
    {
      TempFuncType tool_func = (TempFuncType)func_ptr;
      TemperatureResult result = tool_func(location.c_str(), date.c_str(), unit.c_str());

      // printf("[ToolCall] tool_call %s\n", tool_call.c_str());
      // printf("[ToolCall] arguments: location: %s, date: %s, unit: %s\n", location.c_str(), date.c_str(), unit.c_str());
      // printf("[ToolCall] result: %f, location: %s, date: %s, unit: %s\n", result.temperature, result.location.c_str(),
      //        result.date.c_str(), result.unit.c_str());

      nlohmann::ordered_json ordered_j_item = nlohmann::ordered_json::object();
      ordered_j_item["temperature"] = result.temperature;
      if (!result.location.empty())
      {
        ordered_j_item["location"] = result.location;
      }
      if (!result.date.empty())
      {
        ordered_j_item["date"] = result.date;
      }
      if (!result.unit.empty())
      {
        ordered_j_item["unit"] = result.unit;
      }
      json_results.push_back(ordered_j_item.dump());
    }
    else
    {
      printf("[ToolCall] Function '%s' not found!\n", func_name.c_str());
    }
  }

  std::string result = "[";
  for (size_t i = 0; i < json_results.size(); ++i)
  {
    result += json_results[i];
    if (i + 1 != json_results.size())
      result += ",";
  }
  result += "]";
  return result;
}

int main(int argc, char **argv)
{
  if (argc != 8)
  {
    LOGW("Usage: %s <rknn_path> <weight_path> <tokenizer.gguf> <embedding.bin> <max_context_len> <max_new_tokens> <core_mask>\n ", argv[0]);
    LOGW("Such as: %s Qwen2.5-0.5B.rknn Qwen2.5-0.5B.weight Qwen2.5-0.5B.tokenizer.gguf Qwen2.5-0.5B.embed.bin 1024 256 0xff\n",
         argv[0]);

    return -1;
  }

  int ret = 0;
  char *model_path = argv[1];
  char *weight_path = argv[2];
  const char *tokenizer_path = argv[3];
  const char *embedding_path = argv[4];
  int32_t max_context_len = atoi(argv[5]);
  int32_t max_new_tokens = atoi(argv[6]);
  uint32_t core_mask = strtoul(argv[7], nullptr, 16);

  rknn3_devices devs;
  rknn3_context ctx = 0;
  memset(&devs, 0, sizeof(rknn3_devices));

  VocabInfo vocab_info;
  Tokenizer *tokenizer = nullptr;
  struct stat emb_st;
  struct embedding_info embedding_info;
  RKLLMCallback callback;
  rknn3_llm_param params;
  RKLLMRunState state;
  rknn3_lora loras_enabled[RKNN3_MAX_LORA_NUM];
  rknn3_llm_config llm_config;
  memset(&vocab_info, 0, sizeof(VocabInfo));
  memset(&embedding_info, 0, sizeof(struct embedding_info));
  memset(&callback, 0, sizeof(RKLLMCallback));
  memset(&params, 0, sizeof(rknn3_llm_param));
  memset(&state, 0, sizeof(RKLLMRunState));
  memset(&loras_enabled, 0, sizeof(rknn3_lora) * RKNN3_MAX_LORA_NUM);
  memset(&llm_config, 0, sizeof(rknn3_llm_config));

  std::string function_tools =
      R"([{"type": "function", "function": {"name": "get_current_temperature", "description": "Get current temperature at a location.", "parameters": {"type": "object", "properties": {"location": {"type": "string", "description": "The location to get the temperature for, in the format \"City, State, Country\"."}, "unit": {"type": "string", "enum": ["celsius", "fahrenheit"], "description": "The unit to return the temperature in. Defaults to \"celsius\"."}}, "required": ["location"]}}},{"type": "function", "function": {"name": "get_temperature_date", "description": "Get temperature at a location and date.", "parameters": {"type": "object", "properties": {"location": {"type": "string", "description": "The location to get the temperature for, in the format \"City, State, Country\"."}, "date": {"type": "string", "description": "The date to get the temperature for, in the format \"Year-Month-Day\"."}, "unit": {"type": "string", "enum": ["celsius", "fahrenheit"], "description": "The unit to return the temperature in. Defaults to \"celsius\"."}}, "required": ["location", "date"]}}}])";

  std::vector<std::string> random_prompts = {
      "What's the temperature in San Francisco now? How about tomorrow? Current Date: 2024-09-30.",
      "What's the temperature in Shanghai now? How about tomorrow? Current Date: 2024-09-30.",
  };
  int test_num = random_prompts.size();

  printf("*******************************NEW TEST**********************************\n");

  // Query available devices
  const char *device_id = nullptr;
  memset(&devs, 0, sizeof(devs));
  ret = rknn3_find_devices(&devs);
  if (ret != RKNN3_SUCCESS) {
    printf("rknn3_find_devices failed! ret=%d\n", ret);
    goto exit;
  }
  if (devs.n_devices == 0) {
    printf("No RK182X devices found\n");
    goto exit;
  }
  printf("Found %d RK182X device(s)\n", devs.n_devices);
  for (int i = 0; i < devs.n_devices; i++) {
    printf("  Device %d: transfer_type=%s, id=%s\n", i, devs.devices[i].type, devs.devices[i].id);
  }

  // Use the first device
  device_id = devs.devices[0].id;
  printf("Using device id=%s\n", device_id);

  // init context and model
  ret = init_context_and_model(&ctx, model_path, weight_path, core_mask, device_id);
  if (ret < 0)
  {
    printf("rknn3_session_init_model failed! ret = %d\n", ret);
    goto exit;
  }

  // get tokenizer and embedding
  ret = get_tokenizer_and_embedding(tokenizer_path, &vocab_info, &tokenizer, &embedding_info, embedding_path, &emb_st);
  if (ret < 0)
  {
    printf("get_tokenizer_and_embedding failed, ret = %d\n", ret);
    goto exit;
  }

  // query llm config
  ret = rknn3_query(ctx, RKNN3_QUERY_LLM_CONFIG, &llm_config, sizeof(rknn3_llm_config));
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_query llm config failed! ret=%d", ret);
    goto exit;
  }

  // set basic session parameters
  params.logits_name = (char *)"output";
  params.max_context_len = llm_config.max_ctx_len;
  params.sampling_param.temperature = 1.0f;
  params.sampling_param.top_k = 1; // topk=1 sampling
  params.sampling_param.top_p = 0.9f;
  params.sampling_param.repeat_penalty = 1.1f; // repetition penalty
  params.sampling_param.frequency_penalty = 0.0f;
  params.sampling_param.presence_penalty = 0.0f;
  params.vocab_info.vocab_size = vocab_info.vocab_size;
  params.vocab_info.n_special_eos_id = vocab_info.n_special_eos_id;
  params.vocab_info.n_special_bos_id = vocab_info.n_special_bos_id;
  memcpy(params.vocab_info.special_eos_id, vocab_info.special_eos_id, sizeof(vocab_info.special_eos_id));
  memcpy(params.vocab_info.special_bos_id, vocab_info.special_bos_id, sizeof(vocab_info.special_bos_id));
  params.vocab_info.linefeed_id = vocab_info.linefeed_id;
  params.vocab_info.ignore_eos_token = 0;

  // init session
  session = rknn3_session_init(ctx, &params, 1);
  if (!session)
  {
    printf("rknn3_session_init failed, ret = %d\n", ret);
    goto exit;
  }

  // set session callback
  callback.result_callback = result_callback;
  callback.result_userdata = tokenizer;
  callback.embed_callback = embed_callback;
  callback.embed_userdata = &embedding_info;
  callback.tokenizer_callback = tokenizer_callback;
  callback.tokenizer_userdata = tokenizer;
  // callback.sampling_callback  = sampling_callback;
  // callback.sampling_userdata  = &embedding_info;

  ret = rknn3_session_set_callback(session, &callback);
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_session_set_callback failed, ret = %d\n", ret);
    goto exit;
  }

  // set/update chat template
  if (0)
  {
    std::string system_prompt = "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n";
    std::string prompt_prefix = "<|im_start|>user\n";
    std::string prompt_postfix = "<|im_end|>\n<|im_start|>assistant\n";
    ret = rknn3_session_set_chat_template(session, system_prompt.c_str(), prompt_prefix.c_str(), prompt_postfix.c_str());
    if (ret != RKNN3_SUCCESS)
    {
      printf("rknn3_session_set_chat_template failed, ret = %d\n", ret);
      goto exit;
    }
  }

  // set kvcache policy
  ret = rknn3_session_set_kvcache_policy(session, RKNN3_KVCACHE_POLICY_RECURRENT, nullptr);
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_session_set_kvcache_policy failed, ret=%d\n", ret);
    goto exit;
  }

  ret = rknn3_session_set_function_tools(session, function_tools.c_str());
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_session_set_function_tools failed, ret = %d\n", ret);
    goto exit;
  }

  if (max_context_len != llm_config.max_ctx_len)
  {
    if (max_context_len < llm_config.max_ctx_len)
    {
      LOGW("Warning: max_context_len (%d) is less than llm_config.max_ctx_len (%d).\n", max_context_len, llm_config.max_ctx_len);
      LOGW("It's recommended to set <max_context_len> to %d.\n", llm_config.max_ctx_len);
    }
    else if (max_context_len > llm_config.max_ctx_len)
    {
      LOGW("Error: max_context_len (%d) is greater than llm_config.max_ctx_len (%d).\n", max_context_len, llm_config.max_ctx_len);
      LOGW("Please set <max_context_len> to %d.\n", llm_config.max_ctx_len);
      goto exit;
    }
  }

  printf("\n=============================================================\n");
  printf("%*s\n", 38, "Model Config");
  printf("=============================================================\n");
  printf("%-32s: %-8d\n", "Max Context Length", llm_config.max_ctx_len);
  printf("%-32s: %-8d\n", "Max Position Embeddings", llm_config.max_position_embeddings);
  printf("%-32s: %s\n", "Model Type", llm_config.model_type);
  printf("%-32s: %s\n", "Task Type", llm_config.task_type == RKNN3_LLM_TASK_GENERATE ? "RKNN3_LLM_TASK_GENERATE" : "RKNN3_LLM_TASK_EMBEDDING");
  printf("=============================================================\n\n");

  for (int i = 0; i < test_num; i++)
  {
    printf("\n--------------------Input[%d]-------------------- \n", i);

    int llm_n_inputs = 1;
    rknn3_llm_infer_param llm_infer_param;
    rknn3_llm_input llm_inputs[llm_n_inputs];
    rknn3_llm_input input;
    rknn3_llm_tensor input_tensor;
    memset(&llm_infer_param, 0, sizeof(rknn3_llm_infer_param));
    memset(&llm_inputs, 0, sizeof(rknn3_llm_input) * llm_n_inputs);
    memset(&input, 0, sizeof(rknn3_llm_input));
    memset(&input_tensor, 0, sizeof(rknn3_llm_tensor));

    std::string cur_prompt = random_prompts[i % random_prompts.size()];
    printf("%s\n", cur_prompt.c_str());
    llm_infer_param = {.keep_history = 0, .max_new_tokens = max_new_tokens};
    input_tensor = {.name = NULL, .prompt = cur_prompt.c_str(), .embed = NULL, .tokens = NULL, .n_tokens = 0};
    input.role = "user";
    input.input_type = RKNN3_LLM_INPUT_PROMPT;
    input.llm_input = input_tensor;
    llm_inputs[0] = input;

    printf("\n--------------------Output---------------------- \n");

    gettimeofday(&start, NULL);
    first_decode = true;
    model_answer = "";
    tool_call_output = false;
    ret = rknn3_session_run(session, llm_inputs, llm_n_inputs, &llm_infer_param);
    if (ret != RKNN3_SUCCESS)
    {
      printf("rknn3_session_run failed, ret = %d\n", ret);
      goto exit;
    }
    gettimeofday(&end, NULL);

    // -------------------------------------------------------------

    printf("\n");

    std::vector<std::string> tool_call_list = extract_tool_call_list(model_answer);
    cur_prompt = get_tool_call_results_json(tool_call_list);

    // reference example of the prompt format for tool call:
    // input_tensor.prompt =
    // R"([{"temperature": 26.1, "location": "San Francisco", "unit": "celsius" },{ "temperature": 25.9, "location": "San Francisco",
    // "date": "2024-09-30", "unit": "celsius" }])";
    llm_infer_param = {.keep_history = 0, .max_new_tokens = max_new_tokens};
    input_tensor = {.name = NULL, .prompt = cur_prompt.c_str(), .embed = NULL, .tokens = NULL, .n_tokens = 0};
    input.role = "tool";
    input.input_type = RKNN3_LLM_INPUT_PROMPT;
    input.llm_input = input_tensor;
    llm_inputs[0] = input;

    gettimeofday(&start, NULL);
    first_decode = true;
    model_answer = "";
    tool_call_output = true;
    ret = rknn3_session_run(session, llm_inputs, llm_n_inputs, &llm_infer_param);
    if (ret != RKNN3_SUCCESS)
    {
      printf("rknn3_session_run failed, ret = %d\n", ret);
      goto exit;
    }
    gettimeofday(&end, NULL);

    ret = rknn3_session_query_state(session, &state);
    if (ret != RKNN3_SUCCESS)
    {
      printf("rknn3_session_query_state failed, ret=%d\n", ret);
      goto exit;
    }

    print_performance_statistics(state, start, first_token, end);

    fflush(stdout);
  }

exit:
  if (session)
  {
    rknn3_session_destroy(session);
  }

  if (ctx)
  {
    rknn3_destroy(ctx);
  }

  if (embedding_info.embedding_data)
  {
    munmap(embedding_info.embedding_data, emb_st.st_size);
  }

  if (embedding_info.fd != -1)
  {
    close(embedding_info.fd);
  }

  if (tokenizer)
  {
    delete tokenizer;
  }

  printf("*******************************END TEST**********************************\n");

  fflush(stdout);

  return 0;
}
