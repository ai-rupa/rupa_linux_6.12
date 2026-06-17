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

#include "rkllm/rkllm.h"
#include "easy_timer.h"
#include "rkai_vlm_vision.h"
// #include "rkai_common.h"

typedef struct {
    char* visionModelPath;         /**< Path to the vision model file. */
    char* llmModelPath;         /**< Path to the llm model file. */
    char* inputPath;
    int32_t maxContextLen;        /**< Maximum number of tokens in the context window. */
    int32_t maxNewTokens;         /**< Maximum number of new tokens to generate. */
    int32_t topK;                  /**< Top-K sampling parameter for token generation. */
    bool skipSpecialToken;        /**< Whether to skip special tokens during generation. */
    int32_t baseDomainID;
    const char* imgStart;
    const char* imgEnd;
    const char* imgContent;
    const char* systemPrompt;
    const char* promptPrefix;
    const char* promptPostfix;
    char *prompt;

} rkaiVLMCfg;

typedef struct { 
    rkaiTimer ttftCost;
    rkaiTimer tpsCost;
    rkaiTimer initModelCost;
    int32_t isFirstTokens;
    int32_t numGenTokens;
    float visionCost;
}rkaiVLMStatsTime;

typedef struct {
    LLMHandle* llmHandle;
    rkaiVLMStatsTime* vlmStatsTime;
} rkaiCtxVLM;

int rkai_vlm_llm_init(rkaiCtxVLM* ctx,rkaiVLMCfg cfg);
int rkai_vlm_llm_release(rkaiCtxVLM* ctx);                 
// int rkai_vlm_llm_run(rkaiCtxVLM* ctx, const char* prompt,rkaiVisionToLLM *vtl);
int rkai_vlm_llm_run(rkaiCtxVLM* ctx, rkaiVLMCfg cfg,rkaiVisionToLLM *vtl);
void rkai_vlm_get_version();

