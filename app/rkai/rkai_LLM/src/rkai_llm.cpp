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
#include "rkllm/rkllm.h"
#include "easy_timer.h"
#include "rkai_llm.h"

void rkai_llm_get_version(){
    char version[128] = "0.0.1";
    printf("rkai llm version: %s\n",version);
}

static int callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    // if (userdata != NULL) {
    //     rkaiCtxLLM* ctx = (rkaiCtxLLM*) userdata;
    // }
    // else {
    //     rkaiCtxLLM* ctx = NULL;
    // }
    
    if (state == RKLLM_RUN_FINISH) {

        float tps = result->perf.generate_tokens / result->perf.generate_time_ms *1000.f;
        printf("\n+------------------+--------------------+\n");
        printf("| Stage            | Perf               |\n");
        printf("+------------------+--------------------+\n");
        // printf("| %-16s | %-10.d(tokens) |\n", "SeqLen",result->perf.prefill_tokens);
        // printf("| %-16s | %-10.d(tokens) |\n", "GenLen",result->perf.generate_tokens);
        // printf("| %-16s | %-8.3f(tokens/s) |\n", "Pefill",result->perf.prefill_tokens/result->perf.prefill_time_ms*1000.);
        printf("| %-16s | %-14.3f(ms) |\n", "TTFT",result->perf.prefill_time_ms);       
        printf("| %-16s | %-8.3f(tokens/s) |\n", "TPS", tps);
        printf("+------------------+--------------------+\n");

        // if(ctx) {
        //     g_isFirstTokens = 1;
        // }
        
        // tok(&ctx->tpsCost);
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("\\run error\n");
    } else if (state == RKLLM_RUN_NORMAL) {
        // g_numGenTokens += 1;
        // if(g_isFirstTokens) {
        //     tok(&ctx->llmStatsTime->ttftCost);
        //     g_isFirstTokens = 0;
        // }
        printf("%s", result->text);
    }
    return 0;
}

int rkai_llm_init(rkaiCtxLLM* ctx,rkaiCfg* cfg) {

    if (!ctx) {
        fprintf(stderr, "ctx is NULL\n");
        return -1;
    }
    ctx->llmHandle = (LLMHandle*)malloc(sizeof(LLMHandle));
    memset(ctx->llmHandle,0,sizeof(LLMHandle));
    // ctx->llmStatsTime = (rkaiLLMStatsTime *)malloc(sizeof(rkaiLLMStatsTime));
    // memset(ctx->llmStatsTime,0,sizeof(rkaiLLMStatsTime));

    // printf("rkllm init start\n");
    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = cfg->modelPath;
    param.top_k = cfg->topK;
    param.max_new_tokens = cfg->maxNewTokens;
    param.max_context_len = cfg->maxContextLen;
    param.skip_special_token = cfg->skipSpecialToken;
    param.extend_param.base_domain_id = cfg->baseDomainID;
    // ctx->llmStatsTime->isFirstTokens = 1;
    // ctx->llmStatsTime->numGenTokens = 0;
    rkaiTimer load_ms;
    tik(&load_ms);
    // printf("%s %d,base_domain_id:%d\n",__func__,__LINE__,param.extend_param.base_domain_id);
    // LLMHandle* llmHandle;
    int ret = rkllm_init(ctx->llmHandle, &param, callback);
    if (ret == 0) {
        printf("rkllm init success\n");
    } else {
        printf("rkllm init failed\n");
        // exit_handler(-1);
        return -1;
    }
    tok(&load_ms);
    printf(" Load Model Time: %.3f\n",get_time(&load_ms));

    // // 
    // ctx->llmSysPrompt = (rkaiSysPrompt *)malloc(sizeof(rkaiSysPrompt));
    // memset(ctx->llmSysPrompt,0,sizeof(rkaiSysPrompt));

    // ctx->llmSysPrompt->system_prompt = cfg.system_prompt;
    // ctx->llmSysPrompt->prompt_prefix = 
    return 0;
}

int rkai_llm_run(rkaiCtxLLM* ctx, rkaiCfg* cfg) {
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
    // tik(&ctx->llmStatsTime->ttftCost);
    // tik(&ctx->llmStatsTime->tpsCost);
    ret =rkllm_run(*ctx->llmHandle, &rkllm_input, &rkllm_infer_params, ctx);
    if(ret != 0) {
        printf("%s %d,rkllm_run fail.ret = %d\n",__func__,__LINE__,ret);
        return -1;
    }

    // tok(&ctx->llmStatsTime->tpsCost);
    #if 0 // abondon
    printf("+----------------+-------------------+\n");
    printf("| Stage          | Perf              |\n");
    printf("+----------------+-------------------+\n");
    printf("| %-14s | %-13.3f(ms) |\n", "TTFT",get_time(&ctx->llmStatsTime->ttftCost));
    // printf("+----------------+-------------------+\n");
    printf("| %-14s | %-7.3f(tokens/s) |\n", "TPS", (float)g_numGenTokens/get_time(&ctx->llmStatsTime->tpsCost)*1000.);
    printf("+----------------+-------------------+\n");
    g_numGenTokens = 0;
    #endif

    return 0;
}

void rkai_llm_release(rkaiCtxLLM* ctx) {
    if (ctx->llmHandle != NULL) {
        rkllm_destroy(*ctx->llmHandle);
        ctx->llmHandle = NULL;
    }

    // if(ctx->llmSysPrompt) {
    //     free(ctx->llmSysPrompt);
    //     ctx->llmSysPrompt = NULL;
    // }
    // if(ctx->llmStatsTime) {
    //     free(ctx->llmStatsTime);
    //     ctx->llmStatsTime = NULL;
    // }
}
