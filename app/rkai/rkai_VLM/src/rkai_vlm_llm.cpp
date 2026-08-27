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
#include "easy_timer.h"
#include "rkai_vlm_llm.h"

#if !defined(RKAI_DISABLE_RKNN3)
#define RKAI_ENABLE_RKNN3 1
#endif

#if RKAI_ENABLE_RKNN3
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string>
#endif

void rkai_vlm_get_version(){
    char version[128] = "0.0.1";
    printf("rkai vlm version: %s\n",version);
}

int64_t rkai_vlm_llm_query_memory(rkaiCtxVLM* ctx) {
    if (!ctx) return 0;
#if !defined(RKAI_DISABLE_RKNN3)
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

/* RKLLM callback (only for rkllm backend) */
#if !RKAI_ENABLE_RKNN3
static int rkllm_callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    rkaiCtxVLM* ctx_cb = (rkaiCtxVLM*) userdata;
    if (!result) return 0;
    if (state == RKLLM_RUN_FINISH) {
        float tps = 0.f;
        if (result->perf.generate_time_ms > 0.f) {
            tps = result->perf.generate_tokens / result->perf.generate_time_ms * 1000.f;
        }
        printf("\n+------------------+--------------------+\n");
        printf("| Stage            | Perf               |\n");
        printf("+------------------+--------------------+\n");
        printf("| %-16s | %-14.f(ms) |\n", "Vision",ctx_cb->vlmStatsTime->visionCost);
        printf("| %-16s | %-14.3f(ms) |\n", "TTFT",result->perf.prefill_time_ms);
        printf("| %-16s | %-8.3f(tokens/s) |\n", "TPS", tps);
        printf("+------------------+--------------------+\n");
        ctx_cb->vlmStatsTime->isFirstTokens = 1;
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("\\run error\n");
    } else if (state == RKLLM_RUN_NORMAL) {
        ctx_cb->vlmStatsTime->numGenTokens += 1;
        if(ctx_cb->vlmStatsTime->isFirstTokens) {
            tok(&ctx_cb->vlmStatsTime->ttftCost);
            ctx_cb->vlmStatsTime->isFirstTokens = 0;
        }
        printf("%s", result->text);
    }
    return 0;
}
#endif

/* ===================================================================
 * RKNN3 backend: embedding / tokenizer / callback helpers
 * ================================================================== */
#if RKAI_ENABLE_RKNN3

struct rkai_vlm_rknn3_embedding_info {
    int fd = -1;
    void* mmap_base = nullptr;
    size_t mmap_size = 0;
    float16* embedding_data = nullptr;
    int embedding_dim = 0;
    int vocab_size = 0;
};

static bool rkai_vlm_endswith(const char* s, const char* suffix)
{
    if (!s || !suffix) return false;
    const size_t ls = strlen(s);
    const size_t lf = strlen(suffix);
    if (lf > ls) return false;
    return strcmp(s + (ls - lf), suffix) == 0;
}

static int rkai_vlm_tokenizer_cb(void *userdata, const char *text, int32_t text_len,
                                  int32_t *tokens, int32_t n_tokens_max)
{
    Tokenizer *tokenizer = (Tokenizer *)userdata;
    if (!tokenizer) return -1;
    return tokenizer->Tokenize(text, text_len, tokens, n_tokens_max);
}

static int rkai_vlm_embed_cb(void *userdata, int32_t *tokens, uint64_t num_tokens,
                              void *embed, uint64_t len)
{
    rkai_vlm_rknn3_embedding_info *info = (rkai_vlm_rknn3_embedding_info *)userdata;
    if (!info || !info->embedding_data) return -1;
    const uint64_t expect = num_tokens * (uint64_t)info->embedding_dim * sizeof(float16);
    if (len != expect) return -1;
    for (uint64_t n = 0; n < num_tokens; n++) {
        if (tokens[n] < 0 || tokens[n] >= info->vocab_size) {
            printf("rkai_vlm_embed_cb: token id out of range: %d (vocab_size=%d)\n",
                   tokens[n], info->vocab_size);
            return -1;
        }
        memcpy((unsigned char *)embed + n * info->embedding_dim * sizeof(float16),
               info->embedding_data + tokens[n] * info->embedding_dim,
               info->embedding_dim * sizeof(float16));
    }
    return 0;
}

/* RKNN3 performance tracking */
rkaiTimer g_vlm_llm_ttft;
rkaiTimer g_vlm_llm_tps;
int g_vlm_first_token = 1;
int g_vlm_gen_tokens = 0;
float g_vlm_vision_cost_ms = 0.f;

static int rkai_vlm_result_cb(void *userdata, RKLLMResult *result, LLMCallState state)
{
    Tokenizer *tokenizer = (Tokenizer *)userdata;
    if (state == RKLLM_RUN_ERROR) {
        printf("\nrun error\n"); fflush(stdout); return 0;
    } else if (state == RKLLM_RUN_FINISH) {
        tok(&g_vlm_llm_tps);
        printf("\n"); fflush(stdout);
        return 0;
    } else if (state == RKLLM_RUN_NORMAL) {
        if (!tokenizer || !result) return 0;
        if (g_vlm_first_token) {
            tok(&g_vlm_llm_ttft);
            tik(&g_vlm_llm_tps);
            g_vlm_first_token = 0;
            g_vlm_gen_tokens = 1;
        } else {
            g_vlm_gen_tokens++;
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

static int rkai_vlm_open_embedding(const char *path, const VocabInfo &vocab_info,
                                    rkai_vlm_rknn3_embedding_info *out)
{
    if (!path || !out) return -1;
    struct stat st;
    memset(&st, 0, sizeof(st));
    out->fd = open(path, O_RDONLY);
    if (out->fd == -1) { printf("Failed to open embedding: %s\n", path); return -1; }
    if (fstat(out->fd, &st) == -1) { close(out->fd); out->fd = -1; return -1; }
    out->mmap_size = (size_t)st.st_size;
    out->mmap_base = mmap(NULL, out->mmap_size, PROT_READ, MAP_PRIVATE, out->fd, 0);
    if (out->mmap_base == MAP_FAILED) { out->mmap_base = nullptr; close(out->fd); out->fd = -1; return -1; }
    out->embedding_data = (float16 *)out->mmap_base;
    out->vocab_size = vocab_info.vocab_size;
    if (vocab_info.vocab_size <= 0) {
        printf("rkai_vlm_open_embedding: invalid vocab_size=%d\n", vocab_info.vocab_size);
        munmap(out->mmap_base, out->mmap_size); out->mmap_base = nullptr;
        close(out->fd); out->fd = -1;
        return -1;
    }
    out->embedding_dim = (int)((out->mmap_size / (size_t)vocab_info.vocab_size) / sizeof(float16));
    return 0;
}

static void rkai_vlm_close_embedding(rkai_vlm_rknn3_embedding_info *info)
{
    if (!info) return;
    if (info->mmap_base && info->mmap_size) munmap(info->mmap_base, info->mmap_size);
    if (info->fd != -1) close(info->fd);
    info->fd = -1; info->mmap_base = nullptr; info->mmap_size = 0;
    info->embedding_data = nullptr; info->embedding_dim = 0; info->vocab_size = 0;
}

#endif /* RKAI_ENABLE_RKNN3 */

/* ===================================================================
 * Init
 * ================================================================== */
int rkai_vlm_llm_init(rkaiCtxVLM* ctx, rkaiVLMCfg cfg) {
    if (!ctx) { fprintf(stderr, "ctx is NULL\n"); return -1; }

    /* Caller must pass a zero-initialized ctx (e.g. via calloc or memset before call).
     * We do not memset here to avoid overwriting any pre-allocated fields. */

#if RKAI_ENABLE_RKNN3
    /* Try RKNN3 backend if llm model is .rknn */
    if (cfg.llmModelPath && rkai_vlm_endswith(cfg.llmModelPath, ".rknn")) {
        if (!cfg.llmWeightPath || !cfg.tokenizerPath || !cfg.embeddingPath) {
            printf("rkai_vlm_llm_init(rknn3): missing llmWeightPath/tokenizerPath/embeddingPath\n");
            return -1;
        }

        rknn3_context rknn_ctx = 0;
        int ret = rknn3_init(&rknn_ctx, nullptr);
        if (ret < 0) { printf("rknn3_init fail! ret=%d\n", ret); return -1; }

        ret = rknn3_load_model_from_path(rknn_ctx, cfg.llmModelPath, cfg.llmWeightPath);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_load_model_from_path fail! ret=%d\n", ret);
            rknn3_destroy(rknn_ctx); return -1;
        }

        rknn3_config config;
        memset(&config, 0, sizeof(config));
        config.run_core_mask = cfg.llmCoreMask ? cfg.llmCoreMask : 0xff;

        ret = rknn3_model_init(rknn_ctx, &config);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_model_init fail! ret=%d\n", ret);
            rknn3_destroy(rknn_ctx); return -1;
        }

        /* Query all input names and auto-map to session expected names via alias.
         * Session expects: input_embeds, attention_mask, position_ids, Th, Tc, Ts, Tsr
         * Model may use different names (e.g. input_embedding, kvcache_Th, etc.)
         * Strategy: if model input name ends with the session target name (with a
         * separator like '_' or '.' before it), map it. Also match by keyword prefix. */
        {
            static const char *session_names[] = {
                "input_embeds", "attention_mask", "position_ids", "Th", "Tc", "Ts", "Tsr"
            };
            int n_names = sizeof(session_names) / sizeof(session_names[0]);

            rknn3_input_output_num io_num;
            memset(&io_num, 0, sizeof(io_num));
            ret = rknn3_query(rknn_ctx, RKNN3_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
            if (ret != RKNN3_SUCCESS) {
                printf("rknn3_query IN_OUT_NUM fail! ret=%d\n", ret);
                rknn3_destroy(rknn_ctx); return -1;
            }
            for (uint32_t i = 0; i < io_num.n_input; i++) {
                rknn3_tensor_attr attr;
                memset(&attr, 0, sizeof(attr));
                attr.index = i;
                ret = rknn3_query(rknn_ctx, RKNN3_QUERY_INPUT_ATTR, &attr, sizeof(attr));
                if (ret != RKNN3_SUCCESS) {
                    printf("rknn3_query INPUT_ATTR[%u] fail! ret=%d\n", i, ret);
                    rknn3_destroy(rknn_ctx); return -1;
                }
                printf("llm input[%u]: name='%s'\n", i, attr.name);

                for (int j = 0; j < n_names; j++) {
                    const char *target = session_names[j];
                    size_t tlen = strlen(target);
                    size_t nlen = strlen(attr.name);

                    /* exact match -> no alias needed */
                    if (strcmp(attr.name, target) == 0) break;

                    /* model name ends with target (e.g. "kvcache_Th" ends with "Th") */
                    if (nlen > tlen) {
                        const char *suffix = attr.name + nlen - tlen;
                        if (strcmp(suffix, target) == 0) {
                            char sep = attr.name[nlen - tlen - 1];
                            if (sep == '_' || sep == '.' || sep == '-') {
                                rknn3_set_input_name_alias(rknn_ctx, attr.name, target);
                                printf("  -> alias '%s' -> '%s'\n", attr.name, target);
                                break;
                            }
                        }
                    }

                    /* keyword prefix match for long names (input_embeds, attention_mask, position_ids) */
                    if (tlen > 3 && strstr(attr.name, target) != NULL) {
                        rknn3_set_input_name_alias(rknn_ctx, attr.name, target);
                        printf("  -> alias '%s' -> '%s'\n", attr.name, target);
                        break;
                    }
                }
            }
        }

        Tokenizer *tokenizer = new Tokenizer(TOKENIZER_BACKEND_LLAMA, cfg.tokenizerPath);
        VocabInfo vocab_info;
        memset(&vocab_info, 0, sizeof(vocab_info));
        if (!tokenizer->GetVocabInfo(&vocab_info)) {
            printf("tokenizer GetVocabInfo failed\n");
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        rkai_vlm_rknn3_embedding_info embed_info;
        memset(&embed_info, 0, sizeof(embed_info));
        if (rkai_vlm_open_embedding(cfg.embeddingPath, vocab_info, &embed_info) != 0) {
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        rknn3_llm_config llm_config;
        memset(&llm_config, 0, sizeof(llm_config));
        ret = rknn3_query(rknn_ctx, RKNN3_QUERY_LLM_CONFIG, &llm_config, sizeof(llm_config));
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_query LLM_CONFIG fail! ret=%d\n", ret);
            rkai_vlm_close_embedding(&embed_info); delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        rknn3_llm_param params;
        memset(&params, 0, sizeof(params));
        params.logits_name = (char *)"output";
        params.max_context_len = llm_config.max_ctx_len;
        params.sampling_param.temperature = 1.0f;
        params.sampling_param.top_k = cfg.topK > 0 ? (int)cfg.topK : 1;
        params.sampling_param.top_p = 0.9f;
        params.sampling_param.repeat_penalty = 1.1f;
        params.vocab_info.vocab_size = vocab_info.vocab_size;
        params.vocab_info.n_special_eos_id = vocab_info.n_special_eos_id;
        params.vocab_info.n_special_bos_id = vocab_info.n_special_bos_id;
        memcpy(params.vocab_info.special_eos_id, vocab_info.special_eos_id, sizeof(vocab_info.special_eos_id));
        memcpy(params.vocab_info.special_bos_id, vocab_info.special_bos_id, sizeof(vocab_info.special_bos_id));
        params.vocab_info.linefeed_id = vocab_info.linefeed_id;
        params.vocab_info.ignore_eos_token = 0;

        rknn3_session *session = rknn3_session_init(rknn_ctx, &params, 1);
        if (!session) {
            printf("rknn3_session_init failed\n");
            rkai_vlm_close_embedding(&embed_info);
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        rkai_vlm_rknn3_embedding_info *embed_heap = new rkai_vlm_rknn3_embedding_info(embed_info);

        RKLLMCallback cb;
        memset(&cb, 0, sizeof(cb));
        cb.result_callback = rkai_vlm_result_cb;
        cb.result_userdata = tokenizer;
        cb.embed_callback = rkai_vlm_embed_cb;
        cb.embed_userdata = embed_heap;
        cb.tokenizer_callback = rkai_vlm_tokenizer_cb;
        cb.tokenizer_userdata = tokenizer;

        ret = rknn3_session_set_callback(session, &cb);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_callback failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_vlm_close_embedding(embed_heap); delete embed_heap;
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        const char *sys = (cfg.systemPrompt && cfg.systemPrompt[0]) ? cfg.systemPrompt :
            "<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n";
        const char *pre = (cfg.promptPrefix && cfg.promptPrefix[0]) ? cfg.promptPrefix : "<|im_start|>user\n";
        const char *post = (cfg.promptPostfix && cfg.promptPostfix[0]) ? cfg.promptPostfix :
            "<|im_end|>\n<|im_start|>assistant\n";
        ret = rknn3_session_set_chat_template(session, sys, pre, post);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_chat_template failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_vlm_close_embedding(embed_heap); delete embed_heap;
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }
        ret = rknn3_session_set_kvcache_policy(session, RKNN3_KVCACHE_POLICY_RECURRENT, nullptr);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_set_kvcache_policy failed, ret=%d\n", ret);
            rknn3_session_destroy(session);
            rkai_vlm_close_embedding(embed_heap); delete embed_heap;
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }

        ctx->backendType = 1;
        ctx->rknn3Ctx = (void *)rknn_ctx;
        ctx->rknn3Session = (void *)session;
        ctx->tokenizer = (void *)tokenizer;
        ctx->rknn3EmbedInfo = (void *)embed_heap;
        ctx->embeddingFd = embed_info.fd;
        ctx->embeddingMmap = embed_info.mmap_base;
        ctx->embeddingSize = embed_info.mmap_size;
        ctx->embeddingDim = embed_info.embedding_dim;
        ctx->vocabSize = embed_info.vocab_size;

        ctx->vlmStatsTime = (rkaiVLMStatsTime *)malloc(sizeof(rkaiVLMStatsTime));
        if (!ctx->vlmStatsTime) {
            printf("malloc vlmStatsTime failed\n");
            rknn3_session_destroy(session);
            rkai_vlm_close_embedding(embed_heap); delete embed_heap;
            delete tokenizer; rknn3_destroy(rknn_ctx); return -1;
        }
        memset(ctx->vlmStatsTime, 0, sizeof(rkaiVLMStatsTime));

        printf("rknn3 vlm llm session init success\n");
        return 0;
    }
    /* Model is not .rknn — no backend available in RKNN3-only build */
    printf("rkai_vlm_llm_init: model is not .rknn, no backend available\n");
    return -1;
#endif

#if !RKAI_ENABLE_RKNN3
    /* RKLLM backend */
    ctx->llmHandle = (LLMHandle*)malloc(sizeof(LLMHandle));
    if (!ctx->llmHandle) {
        printf("malloc llmHandle failed\n");
        return -1;
    }
    memset(ctx->llmHandle, 0, sizeof(LLMHandle));
    ctx->vlmStatsTime = (rkaiVLMStatsTime *)malloc(sizeof(rkaiVLMStatsTime));
    if (!ctx->vlmStatsTime) {
        printf("malloc vlmStatsTime failed\n");
        free(ctx->llmHandle);
        ctx->llmHandle = NULL;
        return -1;
    }
    memset(ctx->vlmStatsTime, 0, sizeof(rkaiVLMStatsTime));

    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = cfg.llmModelPath;
    param.top_k = (int)cfg.topK;
    param.max_new_tokens = cfg.maxNewTokens;
    param.max_context_len = cfg.maxContextLen;
    param.skip_special_token = cfg.skipSpecialToken;
    param.img_start = cfg.imgStart;
    param.img_end = cfg.imgEnd;
    param.img_content = cfg.imgContent;
    param.extend_param.base_domain_id = cfg.baseDomainID;

    tik(&ctx->vlmStatsTime->initModelCost);
    int ret = rkllm_init(ctx->llmHandle, &param, rkllm_callback);
    if (ret == 0) {
        printf("rkllm init success\n");
    } else {
        printf("rkllm init failed\n");
        free(ctx->llmHandle); ctx->llmHandle = NULL;
        free(ctx->vlmStatsTime); ctx->vlmStatsTime = NULL;
        return -1;
    }
    tok(&ctx->vlmStatsTime->initModelCost);

    ctx->vlmStatsTime->isFirstTokens = 1;
    ctx->vlmStatsTime->numGenTokens = 0;
    ctx->vlmStatsTime->visionCost = 0.f;
#endif

    return 0;
}

/* ===================================================================
 * Run
 * ================================================================== */
int rkai_vlm_llm_run(rkaiCtxVLM* ctx, rkaiVLMCfg cfg, rkaiVisionToLLM *vtl) {
    if (!ctx) return -1;

#if RKAI_ENABLE_RKNN3
    if (ctx->backendType == 1) {
        rknn3_session *session = (rknn3_session *)ctx->rknn3Session;
        if (!session) return -1;

        /* Prepare multimodal input */
        rknn3_llm_multimodal_tensor tensor;
        memset(&tensor, 0, sizeof(tensor));
        tensor.prompt = (char *)cfg.prompt;
        tensor.enable_thinking = false;
        tensor.image.image_embed = vtl->img_vec_fp16;
        tensor.image.n_image_tokens = vtl->n_image_tokens;
        tensor.image.n_image = vtl->n_image;
        tensor.image.image_start = (char *)cfg.imgStart;
        tensor.image.image_end = (char *)cfg.imgEnd;
        tensor.image.image_content = (char *)cfg.imgContent;
        tensor.image.image_width = vtl->image_width;
        tensor.image.image_height = vtl->image_height;

        rknn3_llm_input input;
        memset(&input, 0, sizeof(input));
        input.input_type = RKNN3_LLM_INPUT_MULTIMODAL;
        input.multimodal_input = tensor;

        rknn3_llm_infer_param infer_param;
        memset(&infer_param, 0, sizeof(infer_param));
        infer_param.keep_history = 0;
        infer_param.max_new_tokens = cfg.maxNewTokens;

        /* Store vision cost for performance reporting */
        g_vlm_vision_cost_ms = ctx->vlmStatsTime ? ctx->vlmStatsTime->visionCost : 0.f;
        g_vlm_first_token = 1;
        g_vlm_gen_tokens = 0;
        tik(&g_vlm_llm_ttft);

        printf("robot: ");
        int ret = rknn3_session_run(session, &input, 1, &infer_param);
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_session_run fail! ret=%d\n", ret);
            return -1;
        }
        return 0;
    }
#endif

#if !RKAI_ENABLE_RKNN3
    RKLLMInput rkllm_input;
    RKLLMInferParam rkllm_infer_params;

    memset(&rkllm_input, 0, sizeof(RKLLMInput));
    memset(&rkllm_infer_params, 0, sizeof(RKLLMInferParam));
    rkllm_infer_params.mode = RKLLM_INFER_GENERATE;
    rkllm_infer_params.keep_history = 0;
    int ret = 0;
    ret = rkllm_set_chat_template(*ctx->llmHandle,
        cfg.systemPrompt,
        cfg.promptPrefix,
        cfg.promptPostfix);
    if(ret != 0) {
        printf("rkllm_set_chat_template fail:ret = %d\n",ret);
        return ret;
    }

    rkllm_input.input_type = RKLLM_INPUT_MULTIMODAL;
    rkllm_input.multimodal_input.prompt = (char*)cfg.prompt;
    rkllm_input.multimodal_input.image_embed = vtl->img_vec;
    rkllm_input.multimodal_input.n_image_tokens = vtl->n_image_tokens;
    rkllm_input.multimodal_input.n_image = vtl->n_image;
    rkllm_input.multimodal_input.image_height = vtl->image_height;
    rkllm_input.multimodal_input.image_width = vtl->image_width;

    printf("\nuser: %s\n", rkllm_input.multimodal_input.prompt);
    printf("robot: ");
    ret = rkllm_run(*ctx->llmHandle, &rkllm_input, &rkllm_infer_params, ctx);
    if(ret != 0) {
        printf("rkllm_run fail:ret = %d\n",ret);
        return ret;
    }

    ctx->vlmStatsTime->numGenTokens = 0;
#endif

    return 0;
}

/* ===================================================================
 * Release
 * ================================================================== */
int rkai_vlm_llm_release(rkaiCtxVLM* ctx) {
    if (!ctx) return 0;

#if RKAI_ENABLE_RKNN3
    if (ctx->backendType == 1) {
        rknn3_session *session = (rknn3_session *)ctx->rknn3Session;
        rknn3_context rknn_ctx = (rknn3_context)ctx->rknn3Ctx;
        Tokenizer *tokenizer = (Tokenizer *)ctx->tokenizer;
        rkai_vlm_rknn3_embedding_info *embed_heap = (rkai_vlm_rknn3_embedding_info *)ctx->rknn3EmbedInfo;

        if (session) rknn3_session_destroy(session);
        if (rknn_ctx) rknn3_destroy(rknn_ctx);
        if (tokenizer) delete tokenizer;
        if (embed_heap) { rkai_vlm_close_embedding(embed_heap); delete embed_heap; }

        if (ctx->vlmStatsTime) { free(ctx->vlmStatsTime); ctx->vlmStatsTime = NULL; }

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
        return 0;
    }
#endif

#if !RKAI_ENABLE_RKNN3
    if(ctx->vlmStatsTime != NULL) {
        free(ctx->vlmStatsTime);
        ctx->vlmStatsTime = NULL;
    }
    if (ctx->llmHandle != NULL) {
        rkllm_destroy(*ctx->llmHandle);
        free(ctx->llmHandle);
        ctx->llmHandle = NULL;
    }
#endif

    return 0;
}
