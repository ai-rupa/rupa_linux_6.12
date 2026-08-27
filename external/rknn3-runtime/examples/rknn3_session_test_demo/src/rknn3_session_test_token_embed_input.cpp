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

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define LOGW(fmt, ...) printf("\033[33m" fmt "\033[0m", ##__VA_ARGS__)
#define ALIGNED_SIZE(x, bytes) (((x) + (bytes - 1)) & (~(bytes - 1)))

rknn3_session *session = nullptr;

struct timeval start, first_token, end;

static bool first_decode = true;

struct embedding_info
{
  int fd;
  float16 *embedding_data;
  int embedding_dim;
  int vocab_size;
};

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
    printf("\n\n--------------------Finished-------------------- \n");
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
    printf("\n\n--------------Max new token reached------------- \n");
    fflush(stdout);
    return 0;
  }
  else if (state == RKLLM_RUN_STOP)
  {
    printf("\n\n-----------------------Stop--------------------- \n");
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
    printf("%s", piece.c_str());

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
  rknn3_kvcache_policy policy;
  memset(&vocab_info, 0, sizeof(VocabInfo));
  memset(&embedding_info, 0, sizeof(struct embedding_info));
  memset(&callback, 0, sizeof(RKLLMCallback));
  memset(&params, 0, sizeof(rknn3_llm_param));
  memset(&state, 0, sizeof(RKLLMRunState));
  memset(&loras_enabled, 0, sizeof(rknn3_lora) * RKNN3_MAX_LORA_NUM);
  memset(&llm_config, 0, sizeof(rknn3_llm_config));
  memset(&policy, 0, sizeof(rknn3_kvcache_policy));

  rknn3_kvcache_policy_param param;
  memset(&param, 0, sizeof(rknn3_kvcache_policy_param));
  param.recurrent.n_keep = 0;
  param.recurrent.n_keep_aligned = 0;

  float16 *embed = nullptr;

  std::string system_prompt_for_keep =
      "<|im_start|>system\nYou are Qwen, created by Alibaba Cloud. You are a helpful assistant.<|im_end|>\n";
  uint64_t system_prompt_for_keep_n_tokens = 0;
  uint64_t system_prompt_for_keep_n_tokens_aligned = 0;
  std::vector<int32_t> system_prompt_for_keep_tokens;
  system_prompt_for_keep_tokens.resize(system_prompt_for_keep_n_tokens_aligned, 0);

  std::vector<std::string> random_prompts = {
    "<|im_start|>user\n请解释一下相对论的基本概念。<|im_end|>\n<|im_start|>assistant\n",  
    "<|im_start|>user\nPlease explain the basic concept of relativity.<|im_end|>\n<|im_start|>assistant\n",
    "<|im_start|>user\nQuestion: In 2004, there were 60 kids at a cookout. In 2005, half the number of kids came to the cookout as compared to 2004. In 2006, 2/3 as many kids came to the cookout as in 2005. How many kids came to the cookout in 2006?\nLet's think step by step\nIn 2005, 60/2=30 kids came to the cookout.\nIn 2006, 30/3*2=20 kids came to the cookout.\nThe answer is 20\n\nQuestion: James decides to run 3 sprints 3 times a week.  He runs 60 meters each sprint.  How many total meters does he run a week?\nLet's think step by step\n<|im_end|>\n<|im_start|>assistant\n",
    "<|im_start|>user\nQuestion: In 2004, there were 60 kids at a cookout. In 2005, half the number of kids came to the cookout as compared to 2004. In 2006, 2/3 as many kids came to the cookout as in 2005. How many kids came to the cookout in 2006?\nLet's think step by step\nIn 2005, 60/2=30 kids came to the cookout.\nIn 2006, 30/3*2=20 kids came to the cookout.\nThe answer is 20\n\nQuestion: A robe takes 2 bolts of blue fiber and half that much white fiber.  How many bolts in total does it take?\nLet's think step by step\n<|im_end|>\n<|im_start|>assistant\n",
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

  // Set basic session parameters
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

  // set kvcache policy for token/embed input, i.e. set system prompt in kvcache
  if (1)
  {
    int32_t system_prompt_for_keep_len = system_prompt_for_keep.length();
    int32_t system_prompt_for_keep_max_tokens = system_prompt_for_keep_len + 2;
    system_prompt_for_keep_tokens.resize(system_prompt_for_keep_max_tokens, 0);

    system_prompt_for_keep_n_tokens = tokenizer_callback(tokenizer, system_prompt_for_keep.c_str(), system_prompt_for_keep_len,
                                                         system_prompt_for_keep_tokens.data(), system_prompt_for_keep_max_tokens);
    if (system_prompt_for_keep_n_tokens <= 0)
    {
      printf("get system_prompt_for_keep tokens failed\n");
      goto exit;
    }

    system_prompt_for_keep_n_tokens_aligned = ALIGNED_SIZE(system_prompt_for_keep_n_tokens, llm_config.kvcache_group_size);
    system_prompt_for_keep_tokens.resize(system_prompt_for_keep_n_tokens_aligned, 0);

    policy = RKNN3_KVCACHE_POLICY_RECURRENT; // RKNN3_KVCACHE_POLICY_NORMAL
    param.recurrent.n_keep = system_prompt_for_keep_n_tokens;
    param.recurrent.n_keep_aligned = system_prompt_for_keep_n_tokens_aligned;
    ret = rknn3_session_set_kvcache_policy(session, policy, &param);
    if (ret != RKNN3_SUCCESS)
    {
      printf("rknn3_session_set_kvcache_policy failed, ret=%d\n", ret);
      goto exit;
    }
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
    printf("%s\n", random_prompts[i % random_prompts.size()].c_str());

    int llm_n_inputs = 1;
    rknn3_llm_infer_param llm_infer_param;
    rknn3_llm_input llm_inputs[llm_n_inputs];
    rknn3_llm_input input;
    rknn3_llm_tensor input_tensor;
    memset(&llm_infer_param, 0, sizeof(rknn3_llm_infer_param));
    memset(&llm_inputs, 0, sizeof(rknn3_llm_input) * llm_n_inputs);
    memset(&input, 0, sizeof(rknn3_llm_input));
    memset(&input_tensor, 0, sizeof(rknn3_llm_tensor));

    std::string prompt = random_prompts[i % random_prompts.size()];
    int32_t prompt_len = prompt.length();
    int32_t max_tokens = prompt_len + 2;
    std::vector<int32_t> tokens(max_tokens, 0);

    uint64_t n_tokens = tokenizer_callback(tokenizer, prompt.c_str(), prompt_len, tokens.data(), max_tokens);
    if (n_tokens <= 0)
    {
      printf("get prompt tokens failed\n");
      goto exit;
    }
    tokens.resize(n_tokens);

#if 1 // token input
    n_tokens = n_tokens + system_prompt_for_keep_n_tokens_aligned;
    tokens.insert(tokens.begin(), system_prompt_for_keep_tokens.begin(), system_prompt_for_keep_tokens.end());

    llm_infer_param = {.keep_history = 0, .max_new_tokens = max_new_tokens};
    input_tensor = {.name = NULL, .prompt = NULL, .embed = NULL, .tokens = tokens.data(), .n_tokens = n_tokens, .enable_thinking = false};
    input.input_type = RKNN3_LLM_INPUT_TOKEN;
    input.llm_input = input_tensor;
    llm_inputs[0] = input;
#else // embed input
    n_tokens = n_tokens + system_prompt_for_keep_n_tokens_aligned;
    tokens.insert(tokens.begin(), system_prompt_for_keep_tokens.begin(), system_prompt_for_keep_tokens.end());

    embed = (float16 *)malloc(n_tokens * embedding_info.embedding_dim * sizeof(float16));
    if (!embed)
    {
      printf("malloc embed failed\n");
      goto exit;
    }

    ret = embed_callback(&embedding_info, tokens.data(), n_tokens, embed, n_tokens * embedding_info.embedding_dim * sizeof(float16));
    if (ret != 0)
    {
      printf("embed_callback failed, ret = %d\n", ret);
      goto exit;
    }

    llm_infer_param = {.keep_history = 0, .max_new_tokens = max_new_tokens};
    input_tensor = {.name = NULL, .prompt = NULL, .embed = embed, .tokens = NULL, .n_tokens = n_tokens, .enable_thinking = false};
    input.input_type = RKNN3_LLM_INPUT_EMBED;
    input.llm_input = input_tensor;
    llm_inputs[0] = input;
#endif

    printf("\n--------------------Output---------------------- \n");

    gettimeofday(&start, NULL);
    first_decode = true;
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

    // clear kvcache
    if (state.n_total_tokens >= (state.n_max_tokens - max_new_tokens))
    {
      ret = rknn3_session_clear_kvcache(session, RKNN3_KVCACHE_CLEAR_ALL);
      if (ret != RKNN3_SUCCESS)
      {
        printf("rknn3_session_clear_kvcache failed, ret=%d\n", ret);
        goto exit;
      }
    }

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

    printf("\nPerformance Statistics: ");
    printf("\n-----------------------------------------------------------------------------------------\n");
    printf(" %-10s | %-16s | %-8s | %-20s | %-20s \n", "Stage", "Total Time (ms)", "Tokens", "Time per Token (ms)", "Tokens per Second");
    printf("-----------------------------------------------------------------------------------------\n");
    printf(" %-10s | %-16.2f | %-8d | %-20.2f | %-20.2f \n", "Prefill", prefill_ms, prefill_n_tokens, prefill_tpt, prefill_tps);
    printf(" %-10s | %-16.2f | %-8d | %-20.2f | %-20.2f \n", "Generate", decode_ms, decode_n_tokens, decode_tpt, decode_tps);
    printf("-----------------------------------------------------------------------------------------\n");

    fflush(stdout);

    if (embed)
    {
      free(embed);
      embed = nullptr;
    }
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
