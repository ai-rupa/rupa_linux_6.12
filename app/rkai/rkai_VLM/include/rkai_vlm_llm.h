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

#include "rkai_vlm_vision.h"

#if !defined(RKAI_DISABLE_RKNN3)
#include "rknn3_api.h"
#include "float16.h"
#include "Tokenizer.h"
#else
#include "rkllm/rkllm.h"
#endif
#include "easy_timer.h"

typedef struct {
    char* visionModelPath;
    char* llmModelPath;
    char* inputPath;
    int32_t maxContextLen;
    int32_t maxNewTokens;
    float topK;
    bool skipSpecialToken;
    int32_t baseDomainID;
    const char* imgStart;
    const char* imgEnd;
    const char* imgContent;
    const char* systemPrompt;
    const char* promptPrefix;
    const char* promptPostfix;
    char *prompt;

#if !defined(RKAI_DISABLE_RKNN3)
    /* RKNN3-specific paths */
    const char* visionWeightPath;
    const char* llmWeightPath;
    const char* tokenizerPath;
    const char* embeddingPath;
    uint32_t visionCoreMask;
    uint32_t llmCoreMask;
#endif
} rkaiVLMCfg;

typedef struct {
    rkaiTimer ttftCost;
    rkaiTimer tpsCost;
    rkaiTimer initModelCost;
    int32_t isFirstTokens;
    int32_t numGenTokens;
    float visionCost;
} rkaiVLMStatsTime;

typedef struct {
#if !defined(RKAI_DISABLE_RKNN3)
    int backendType;            /* 0=RKLLM, 1=RKNN3 */
    void* rknn3Ctx;             /* rknn3_context */
    void* rknn3Session;         /* rknn3_session* */
    void* tokenizer;            /* Tokenizer* */
    void* rknn3EmbedInfo;       /* rkai_rknn3_embedding_info* (heap) */
    void* embeddingMmap;
    int   embeddingFd;
    size_t embeddingSize;
    int   embeddingDim;
    int   vocabSize;
#else
    LLMHandle* llmHandle;
#endif
    rkaiVLMStatsTime* vlmStatsTime;
} rkaiCtxVLM;

int rkai_vlm_llm_init(rkaiCtxVLM* ctx, rkaiVLMCfg cfg);
int rkai_vlm_llm_release(rkaiCtxVLM* ctx);
int rkai_vlm_llm_run(rkaiCtxVLM* ctx, rkaiVLMCfg cfg, rkaiVisionToLLM *vtl);
int64_t rkai_vlm_llm_query_memory(rkaiCtxVLM* ctx);

#if !defined(RKAI_DISABLE_RKNN3)
/* Performance tracking (defined in rkai_vlm_llm.cpp) */
extern rkaiTimer g_vlm_llm_ttft;
extern rkaiTimer g_vlm_llm_tps;
extern int g_vlm_first_token;
extern int g_vlm_gen_tokens;
extern float g_vlm_vision_cost_ms;
#endif

void rkai_vlm_get_version();
