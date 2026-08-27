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

#include <stdint.h>
#include <stddef.h>
#include "easy_timer.h"

// Forward declaration for rkllm handle (used when RKNN3 is disabled)
#if defined(RKAI_DISABLE_RKNN3)
// RV1126B: uses rkllm backend
#include "rkllm/rkllm.h"
#else
// RK3572: uses rknn3 backend
#define RKAI_ENABLE_RKNN3 1
typedef void* LLMHandle;
#endif
// #include "rkai_common.h"

typedef struct {
    const char* modelPath;         /**< Path to the model file. */
    // --- RKNN3 (LLM Session) optional fields ---
    // When modelPath points to a ".rknn" model, these fields are used to init rknn3 session.
    const char* weightPath;        /**< Path to the weight file for rknn3 model. */
    const char* tokenizerPath;     /**< Path to tokenizer.gguf for rknn3 prompt mode. */
    const char* embeddingPath;     /**< Path to embedding.bin for rknn3 prompt mode. */
    uint32_t rknn3CoreMask;        /**< NPU core mask for rknn3 (e.g. 0xff). */
    int32_t maxContextLen;        /**< Maximum number of tokens in the context window. */
    int32_t maxNewTokens;         /**< Maximum number of new tokens to generate. */
    float topK;                    /**< Top-K sampling parameter for token generation. */
    bool skipSpecialToken;        /**< Whether to skip special tokens during generation. */
    int32_t baseDomainID;
    char *prompt;
    const char* systemPrompt;
    const char* promptPrefix;
    const char* promptPostfix;
} rkaiCfg;

typedef struct {
    rkaiTimer ttftCost;
    rkaiTimer tpsCost;
    rkaiTimer initModelCost;
    int32_t isFirstTokens;
    int32_t numGenTokens;
}rkaiLLMStatsTime;

typedef struct {
    char system_prompt[128];
    char prompt_prefix[128];
    char prompt_postfix[128]; 
    // rkaiLLMStatsTime* llmStatsTime;
} rkaiSysPrompt;

typedef struct {
    LLMHandle* llmHandle;
    rkaiLLMStatsTime* llmStatsTime;
    rkaiSysPrompt *llmSysPrompt;
    // backend: 0=RKLLM, 1=RKNN3_SESSION
    int backendType;
    // rknn3 opaque handles/resources (only valid when backendType==1)
    void* rknn3Ctx;
    void* rknn3Session;
    void* tokenizer;
    void* rknn3EmbedInfo;    // heap object for callbacks
    void* embeddingMmap;     // mmap base address
    int   embeddingFd;
    size_t embeddingSize;
    int   embeddingDim;
    int   vocabSize;
} rkaiCtxLLM;

int rkai_llm_init(rkaiCtxLLM* ctx,rkaiCfg* cfg);
void rkai_llm_release(rkaiCtxLLM* ctx);
int rkai_llm_run(rkaiCtxLLM* ctx, rkaiCfg* cfg);
int64_t rkai_llm_query_memory(rkaiCtxLLM* ctx);
void rkai_llm_get_version();

#if !defined(RKAI_DISABLE_RKNN3)
/* Performance tracking globals (defined in rkai_llm.cpp) */
extern rkaiTimer g_llm_ttft;
extern int g_llm_first_token;
extern int g_llm_gen_tokens;
#endif

