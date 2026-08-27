// Copyright (c) 2024 by Rockchip Electronics Co., Ltd. All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <string>
#include "easy_timer.h"
#include "rkai_llm.h"

// Enable RKNN3 session backend when headers are available.
// Build system can force-disable by defining RKAI_DISABLE_RKNN3.
#if !defined(RKAI_DISABLE_RKNN3)
#define RKAI_ENABLE_RKNN3 1
#endif

#if RKAI_ENABLE_RKNN3
#include "rknn3_api.h"
#include "float16.h"
#include "Tokenizer.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#else
#include "rkllm/rkllm.h"
#endif

void rkai_llm_get_version(){
    char version[128] = "0.0.1";
    printf("rkai llm version: %s\n",version);
}

int64_t rkai_llm_query_memory(rkaiCtxLLM* ctx) {
    if (!ctx) return 0;
#if RKAI_ENABLE_RKNN3
    if (ctx->backendType == 1 && ctx->rknn3Ctx) {
        rknn3_context rknn_ctx = (rknn3_context)ctx->rknn3Ctx;
        int64_t memory = 0;
        uint32_t model_core_num = 0;
        rknn3_query(rknn_ctx, RKNN3_QUERY_CORE_NUMBER, &model_core_num, sizeof(model_core_num));
        rknn3_allocation_info *alloc = (rknn3_allocation_info *)malloc(sizeof(rknn3_allocation_info) * model_core_num);
        if (alloc) {
            int ret = rknn3_query(rknn_ctx, RKNN3_QUERY_ALLOCATION_INFO, alloc,
                                  sizeof(rknn3_allocation_info) * model_core_num);
            if (ret == 0) {
                for (uint32_t i = 0; i < model_core_num; i++) {
                    memory += alloc[i].weight_mem.size;
                    memory += alloc[i].command_mem.size;
                    memory += alloc[i].kvcache_mem.size;
                }
            }
            free(alloc);
        }
        return memory;
    }
#endif
    return 0;
}

#if !RKAI_ENABLE_RKNN3
static int callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    if (!result) return 0;

    if (state == RKLLM_RUN_FINISH) {
        if (result->perf.generate_time_ms > 0.f) {
            float tps = result->perf.generate_tokens / result->perf.generate_time_ms * 1000.f;
            printf("\n+------------------+--------------------+\n");
            printf("| Stage            | Perf               |\n");
            printf("+------------------+--------------------+\n");
            printf("| %-16s | %-14.3f(ms) |\n", "TTFT", result->perf.prefill_time_ms);
            printf("| %-16s | %-8.3f(tokens/s) |\n", "TPS", tps);
            printf("+------------------+--------------------+\n");
        }
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("\\run error\n");
    } else if (state == RKLLM_RUN_NORMAL) {
        printf("%s", result->text);
    }
    return 0;
}
#endif

#if RKAI_ENABLE_RKNN3
struct rkai_rknn3_embedding_info {
    int fd = -1;
    void* mmap_base = nullptr;
    size_t mmap_size = 0;
    float16* embedding_data = nullptr;
    int embedding_dim = 0;
    int vocab_size = 0;
};

static bool rkai_endswith(const char* s, const char* suffix)
{
    if (!s || !suffix) return false;
    const size_t ls = strlen(s);
    const size_t lf = strlen(suffix);
    if (lf > ls) return false;
    return strcmp(s + (ls - lf), suffix) == 0;
}

static int rkai_rknn3_tokenizer_callback(void *userdata, const char *text, int32_t text_len,
                                        int32_t *tokens, int32_t n_tokens_max)
{
    Tokenizer *tokenizer = (Tokenizer *)userdata;
    if (!tokenizer) return -1;
    return tokenizer->Tokenize(text, text_len, tokens, n_tokens_max);
}

static int rkai_rknn3_embed_callback(void *userdata, int32_t *tokens, uint64_t num_tokens, void *embed, uint64_t len)
{
    rkai_rknn3_embedding_info *embed_info = (rkai_rknn3_embedding_info *)userdata;
    if (!embed_info || !embed_info->embedding_data) return -1;

    const uint64_t expect = num_tokens * (uint64_t)embed_info->embedding_dim * sizeof(float16);
    if (len != expect) {
        printf("rkai_rknn3_embed_callback: invalid embed buffer, len=%lu expect=%lu\n",
               (unsigned long)len, (unsigned long)expect);
        return -1;
    }

    for (uint64_t n = 0; n < num_tokens; n++) {
        if (tokens[n] < 0 || tokens[n] >= embed_info->vocab_size) {
            printf("rkai_rknn3_embed_callback: token id out of range: %d (vocab_size=%d)\n",
                   tokens[n], embed_info->vocab_size);
            return -1;
        }
        memcpy((unsigned char *)embed + n * embed_info->embedding_dim * sizeof(float16),
               embed_info->embedding_data + tokens[n] * embed_info->embedding_dim,
               embed_info->embedding_dim * sizeof(float16));
    }
    return 0;
}

/* RKNN3 LLM performance tracking */
rkaiTimer g_llm_ttft;
int g_llm_first_token = 1;
int g_llm_gen_tokens = 0;

static int rkai_rknn3_result_callback(void *userdata, RKLLMResult *result, LLMCallState state)
{
    Tokenizer *tokenizer = (Tokenizer *)userdata;

    if (state == RKLLM_RUN_ERROR) {
        printf("\nrun error\n");
        fflush(stdout);
        return 0;
    } else if (state == RKLLM_RUN_FINISH) {
        printf("\n");
        fflush(stdout);
        return 0;
    } else if (state == RKLLM_RUN_NORMAL) {
        if (!tokenizer || !result) return 0;
        if (g_llm_first_token) {
            tok(&g_llm_ttft);
            g_llm_first_token = 0;
            g_llm_gen_tokens = 1;
        } else {
            g_llm_gen_tokens++;
        }
        std::string piece;
        if (result->num_tokens == 1) {
            piece = tokenizer->TokenToPiece(result->token_ids[0]);
        } else {
            piece = tokenizer->Decode(result->token_ids, result->num_tokens);
        }
        printf("%s", piece.c_str());
        fflush(stdout);
    }
    return 0;
}

static int rkai_rknn3_open_embedding(const char* embedding_path, const VocabInfo& vocab_info,
                                    rkai_rknn3_embedding_info* out_info)
{
    if (!embedding_path || !out_info) return -1;
    struct stat st;
    memset(&st, 0, sizeof(st));

    out_info->fd = open(embedding_path, O_RDONLY);
    if (out_info->fd == -1) {
        printf("Failed to open embedding file: %s\n", embedding_path);
        return -1;
    }
    if (fstat(out_info->fd, &st) == -1) {
        printf("Failed to stat embedding file: %s\n", embedding_path);
        close(out_info->fd);
        out_info->fd = -1;
        return -1;
    }

    out_info->mmap_size = (size_t)st.st_size;
    out_info->mmap_base = mmap(NULL, out_info->mmap_size, PROT_READ, MAP_PRIVATE, out_info->fd, 0);
    if (out_info->mmap_base == MAP_FAILED) {
        printf("Failed to mmap embedding file: %s\n", embedding_path);
        close(out_info->fd);
        out_info->fd = -1;
        out_info->mmap_base = nullptr;
        out_info->mmap_size = 0;
        return -1;
    }

    out_info->embedding_data = (float16*)out_info->mmap_base;
    out_info->vocab_size = vocab_info.vocab_size;
    if (vocab_info.vocab_size <= 0) {
        printf("rkai_rknn3_open_embedding: invalid vocab_size=%d\n", vocab_info.vocab_size);
        munmap(out_info->mmap_base, out_info->mmap_size);
        close(out_info->fd); out_info->fd = -1;
        return -1;
    }
    out_info->embedding_dim = (int)((out_info->mmap_size / (size_t)vocab_info.vocab_size) / sizeof(float16));
    return 0;
}

static void rkai_rknn3_close_embedding(rkai_rknn3_embedding_info* info)
{
    if (!info) return;
    if (info->mmap_base && info->mmap_size) {
        munmap(info->mmap_base, info->mmap_size);
    }
    if (info->fd != -1) {
        close(info->fd);
    }
    info->fd = -1;
    info->mmap_base = nullptr;
    info->mmap_size = 0;
    info->embedding_data = nullptr;
    info->embedding_dim = 0;
    info->vocab_size = 0;
}
#endif

int rkai_llm_init(rkaiCtxLLM* ctx,rkaiCfg* cfg) {

    if (!ctx) {
        fprintf(stderr, "ctx is NULL\n");
        return -1;
    }

    // init ctx common fields
    ctx->backendType = 0;
    ctx->rknn3Ctx = nullptr;
    ctx->rknn3Session = nullptr;
    ctx->tokenizer = nullptr;
    ctx->rknn3EmbedInfo = nullptr;
    ctx->embeddingMmap = nullptr;
    ctx->embeddingFd = -1;
    ctx->embeddingSize = 0;
    ctx->embeddingDim = 0;
    ctx->vocabSize = 0;

#if RKAI_ENABLE_RKNN3
    // If modelPath ends with ".rknn", use rknn3 session backend.
    if (cfg && rkai_endswith(cfg->modelPath, ".rknn")) {
        if (!cfg->weightPath || !cfg->tokenizerPath || !cfg->embeddingPath) {
            printf("rkai_llm_init(rknn3): missing weightPath/tokenizerPath/embeddingPath\n");
            return -1;
        }

        rknn3_context rknn_ctx = 0;
        int ret = rknn3_init(&rknn_ctx, nullptr);
        if (ret < 0) {
            printf("rknn3_init fail! ret=%d\n", ret);
            return -1;
        }

        ret = rknn3_load_model_from_path(rknn_ctx, cfg->modelPath, cfg->weightPath);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_load_model_from_path failed! ret=%d\n", ret);
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        rknn3_config config;
        memset(&config, 0, sizeof(config));
        config.run_core_mask = cfg->rknn3CoreMask ? cfg->rknn3CoreMask : 0xff;

        ret = rknn3_model_init(rknn_ctx, &config);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_model_init failed! ret=%d\n", ret);
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // tokenizer + vocab
        Tokenizer* tokenizer = new Tokenizer(TOKENIZER_BACKEND_LLAMA, cfg->tokenizerPath);
        VocabInfo vocab_info;
        memset(&vocab_info, 0, sizeof(vocab_info));
        if (!tokenizer->GetVocabInfo(&vocab_info)) {
            printf("tokenizer GetVocabInfo failed\n");
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // embedding mmap
        rkai_rknn3_embedding_info embed_info;
        memset(&embed_info, 0, sizeof(embed_info));
        if (rkai_rknn3_open_embedding(cfg->embeddingPath, vocab_info, &embed_info) != 0) {
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // query llm config
        rknn3_llm_config llm_config;
        memset(&llm_config, 0, sizeof(llm_config));
        ret = rknn3_query(rknn_ctx, RKNN3_QUERY_LLM_CONFIG, &llm_config, sizeof(llm_config));
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_query llm config failed! ret=%d\n", ret);
            rkai_rknn3_close_embedding(&embed_info);
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // set basic session parameters
        rknn3_llm_param params;
        memset(&params, 0, sizeof(params));
        params.logits_name = (char*)"output";
        params.max_context_len = llm_config.max_ctx_len;
        params.sampling_param.temperature = 1.0f;
        params.sampling_param.top_k = cfg->topK > 0 ? (int)cfg->topK : 1;
        params.sampling_param.top_p = 0.9f;
        params.sampling_param.repeat_penalty = 1.1f;
        params.sampling_param.frequency_penalty = 0.0f;
        params.sampling_param.presence_penalty = 0.0f;
        params.vocab_info.vocab_size = vocab_info.vocab_size;
        params.vocab_info.n_special_eos_id = vocab_info.n_special_eos_id;
        params.vocab_info.n_special_bos_id = vocab_info.n_special_bos_id;
        memcpy(params.vocab_info.special_eos_id, vocab_info.special_eos_id, sizeof(vocab_info.special_eos_id));
        memcpy(params.vocab_info.special_bos_id, vocab_info.special_bos_id, sizeof(vocab_info.special_bos_id));
        params.vocab_info.linefeed_id = vocab_info.linefeed_id;
        params.vocab_info.ignore_eos_token = 0;

        rknn3_session* session = rknn3_session_init(rknn_ctx, &params, 1);
        if (!session) {
            printf("rknn3_session_init failed\n");
            rkai_rknn3_close_embedding(&embed_info);
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // store embedding in heap for callback userdata lifetime
        rkai_rknn3_embedding_info* embed_info_heap = new rkai_rknn3_embedding_info(embed_info);

        // set session callback
        RKLLMCallback cb;
        memset(&cb, 0, sizeof(cb));
        cb.result_callback = rkai_rknn3_result_callback;
        cb.result_userdata = tokenizer;
        cb.embed_callback = rkai_rknn3_embed_callback;
        cb.embed_userdata = embed_info_heap;
        cb.tokenizer_callback = rkai_rknn3_tokenizer_callback;
        cb.tokenizer_userdata = tokenizer;

        ret = rknn3_session_set_callback(session, &cb);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_callback failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_rknn3_close_embedding(embed_info_heap);
            delete embed_info_heap;
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // set chat template (prefer cfg; fallback to default)
        const char* sys = (cfg->systemPrompt && cfg->systemPrompt[0]) ? cfg->systemPrompt :
            "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n";
        const char* pre = (cfg->promptPrefix && cfg->promptPrefix[0]) ? cfg->promptPrefix : "<|im_start|>user\n";
        const char* post = (cfg->promptPostfix && cfg->promptPostfix[0]) ? cfg->promptPostfix :
            "<|im_end|>\n<|im_start|>assistant\n";
        ret = rknn3_session_set_chat_template(session, sys, pre, post);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_chat_template failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_rknn3_close_embedding(embed_info_heap);
            delete embed_info_heap;
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // set kvcache policy
        ret = rknn3_session_set_kvcache_policy(session, RKNN3_KVCACHE_POLICY_RECURRENT, nullptr);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_kvcache_policy failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_rknn3_close_embedding(embed_info_heap);
            delete embed_info_heap;
            delete tokenizer;
            rknn3_destroy(rknn_ctx);
            return -1;
        }

        // persist
        ctx->backendType = 1;
        ctx->rknn3Ctx = (void*)rknn_ctx;
        ctx->rknn3Session = (void*)session;
        ctx->tokenizer = (void*)tokenizer;
        ctx->rknn3EmbedInfo = (void*)embed_info_heap;
        ctx->embeddingFd = embed_info.fd;
        ctx->embeddingMmap = embed_info.mmap_base;
        ctx->embeddingSize = embed_info.mmap_size;
        ctx->embeddingDim = embed_info.embedding_dim;
        ctx->vocabSize = embed_info.vocab_size;

        printf("rknn3 session init success\n");
        return 0;
    }
    /* Model is not .rknn — no backend available in RKNN3-only build */
    printf("rkai_llm_init: model is not .rknn, no backend available\n");
    return -1;
#endif

#if !RKAI_ENABLE_RKNN3
    ctx->llmHandle = (LLMHandle*)malloc(sizeof(LLMHandle));
    if (!ctx->llmHandle) {
        printf("malloc llmHandle failed\n");
        return -1;
    }
    memset(ctx->llmHandle, 0, sizeof(LLMHandle));

    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = cfg->modelPath;
    param.top_k = (int)cfg->topK;
    param.max_new_tokens = cfg->maxNewTokens;
    param.max_context_len = cfg->maxContextLen;
    param.skip_special_token = cfg->skipSpecialToken;
    param.extend_param.base_domain_id = cfg->baseDomainID;
    rkaiTimer load_ms;
    tik(&load_ms);
    int ret = rkllm_init(ctx->llmHandle, &param, callback);
    if (ret == 0) {
        printf("rkllm init success\n");
    } else {
        printf("rkllm init failed\n");
        free(ctx->llmHandle); ctx->llmHandle = NULL;
        return -1;
    }
    tok(&load_ms);
    printf(" Load Model Time: %.3f\n",get_time(&load_ms));
#endif
    return 0;
}

int rkai_llm_run(rkaiCtxLLM* ctx, rkaiCfg* cfg) {
#if RKAI_ENABLE_RKNN3
    if (ctx && ctx->backendType == 1) {
        if (!ctx->rknn3Session || !cfg || !cfg->prompt) {
            printf("rkai_llm_run(rknn3): invalid args\n");
            return -1;
        }
        rknn3_session* session = (rknn3_session*)ctx->rknn3Session;

        rknn3_llm_infer_param infer_param;
        memset(&infer_param, 0, sizeof(infer_param));
        infer_param.keep_history = 0;
        infer_param.max_new_tokens = cfg->maxNewTokens;

        rknn3_llm_input input;
        memset(&input, 0, sizeof(input));
        rknn3_llm_tensor input_tensor;
        memset(&input_tensor, 0, sizeof(input_tensor));
        input_tensor.prompt = cfg->prompt;
        input_tensor.enable_thinking = false;
        input.input_type = RKNN3_LLM_INPUT_PROMPT;
        input.llm_input = input_tensor;

        g_llm_first_token = 1;
        g_llm_gen_tokens = 0;
        tik(&g_llm_ttft);

        printf("robot: ");
        int ret = rknn3_session_run(session, &input, 1, &infer_param);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_run failed, ret=%d\n", ret);
            return -1;
        }
        return 0;
    }
#endif

#if !RKAI_ENABLE_RKNN3
    RKLLMInput rkllm_input;
    RKLLMInferParam rkllm_infer_params;

    int ret = 0;
    memset(&rkllm_input, 0, sizeof(RKLLMInput));
    memset(&rkllm_infer_params, 0, sizeof(RKLLMInferParam));

    rkllm_infer_params.mode = RKLLM_INFER_GENERATE;
    rkllm_infer_params.keep_history = 0;

    // ret = rkllm_set_chat_template(*ctx->llmHandle,
    //                 cfg->systemPrompt,
    //                 cfg->promptPrefix,
    //                 cfg->promptPostfix);
    ret = rkllm_set_chat_template(*ctx->llmHandle,
                    "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n",
                    "<|im_start|>user\n",
                    "<|im_end|>\n<|im_start|>assistant\n");
    if(ret != 0) {
        printf("rkllm_set_chat_template set fail.ret = %d\n",ret);
        return -1;
    }

    rkllm_input.prompt_input = cfg->prompt;
    rkllm_input.role = "user";
    rkllm_input.input_type = RKLLM_INPUT_PROMPT;

    // printf("\nuser: %s\n", rkllm_input.prompt_input);
    printf("robot: ");
    ret = rkllm_run(*ctx->llmHandle, &rkllm_input, &rkllm_infer_params, ctx);
    if(ret != 0) {
        printf("%s %d,rkllm_run fail.ret = %d\n",__func__,__LINE__,ret);
        return -1;
    }
#endif

    return 0;
}

void rkai_llm_release(rkaiCtxLLM* ctx) {
    if (!ctx) return;

#if RKAI_ENABLE_RKNN3
    if (ctx->backendType == 1) {
        rknn3_session* session = (rknn3_session*)ctx->rknn3Session;
        rknn3_context rknn_ctx = (rknn3_context)ctx->rknn3Ctx;
        Tokenizer* tokenizer = (Tokenizer*)ctx->tokenizer;
        rkai_rknn3_embedding_info* embed_info_heap = (rkai_rknn3_embedding_info*)ctx->rknn3EmbedInfo;

        if (session) {
            rknn3_session_destroy(session);
        }
        if (rknn_ctx) {
            rknn3_destroy(rknn_ctx);
        }
        if (tokenizer) {
            delete tokenizer;
        }
        if (embed_info_heap) {
            // closes mmap + fd
            rkai_rknn3_close_embedding(embed_info_heap);
            delete embed_info_heap;
        } else {
            // fallback: close from ctx fields
            if (ctx->embeddingMmap && ctx->embeddingSize) {
                munmap(ctx->embeddingMmap, ctx->embeddingSize);
            }
            if (ctx->embeddingFd != -1) {
                close(ctx->embeddingFd);
            }
        }

        ctx->backendType = 0;
        ctx->rknn3Ctx = nullptr;
        ctx->rknn3Session = nullptr;
        ctx->tokenizer = nullptr;
        ctx->rknn3EmbedInfo = nullptr;
        ctx->embeddingMmap = nullptr;
        ctx->embeddingFd = -1;
        ctx->embeddingSize = 0;
        ctx->embeddingDim = 0;
        ctx->vocabSize = 0;
        return;
    }
#endif

#if !RKAI_ENABLE_RKNN3
    if (ctx->llmHandle != NULL) {
        rkllm_destroy(*ctx->llmHandle);
        free(ctx->llmHandle);
        ctx->llmHandle = NULL;
    }
#endif
}
