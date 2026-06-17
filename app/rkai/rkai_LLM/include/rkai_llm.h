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
// #include "rkai_common.h"

typedef struct {
    const char* modelPath;         /**< Path to the model file. */
    int32_t maxContextLen;        /**< Maximum number of tokens in the context window. */
    int32_t maxNewTokens;         /**< Maximum number of new tokens to generate. */
    int32_t topK;                  /**< Top-K sampling parameter for token generation. */
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
} rkaiCtxLLM;

int rkai_llm_init(rkaiCtxLLM* ctx,rkaiCfg* cfg);
void rkai_llm_release(rkaiCtxLLM* ctx);      
int rkai_llm_run(rkaiCtxLLM* ctx, rkaiCfg* cfg);           
// void rkai_llm_run(rkaiCtxLLM* ctx, const char* prompt);
void rkai_llm_get_version();

