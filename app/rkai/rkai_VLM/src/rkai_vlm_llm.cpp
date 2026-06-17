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
#include "rkai_vlm_llm.h"

void rkai_vlm_get_version(){
    char version[128] = "0.0.1";
    printf("rkai vlm version: %s\n",version);
}

static int callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    rkaiCtxVLM* ctx = (rkaiCtxVLM*) userdata;

    if (state == RKLLM_RUN_FINISH) {
        float tps = result->perf.generate_tokens / result->perf.generate_time_ms *1000.f;
        printf("\n+------------------+--------------------+\n");
        printf("| Stage            | Perf               |\n");
        printf("+------------------+--------------------+\n");
        printf("| %-16s | %-14.f(ms) |\n", "Vision",ctx->vlmStatsTime->visionCost);
        printf("| %-16s | %-14.3f(ms) |\n", "TTFT",result->perf.prefill_time_ms);       
        printf("| %-16s | %-8.3f(tokens/s) |\n", "TPS", tps);
        printf("+------------------+--------------------+\n");

        ctx->vlmStatsTime->isFirstTokens = 1;
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("\\run error\n");
    } else if (state == RKLLM_RUN_NORMAL) {
        ctx->vlmStatsTime->numGenTokens += 1;
        if(ctx->vlmStatsTime->isFirstTokens) {
            tok(&ctx->vlmStatsTime->ttftCost);
            ctx->vlmStatsTime->isFirstTokens = 0;
        }
        printf("%s", result->text);
    }

    return 0;
}

int rkai_vlm_llm_init(rkaiCtxVLM* ctx,rkaiVLMCfg cfg) {
    if (!ctx) {
        fprintf(stderr, "ctx is NULL\n");
        return -1;
    }

    ctx->llmHandle = (LLMHandle*)malloc(sizeof(LLMHandle));
    memset(ctx->llmHandle,0,sizeof(ctx->llmHandle));
    ctx->vlmStatsTime = (rkaiVLMStatsTime *)malloc(sizeof(rkaiVLMStatsTime));
    memset(ctx->vlmStatsTime,0,sizeof(rkaiVLMStatsTime));

    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = cfg.llmModelPath;
    param.top_k = cfg.topK;
    param.max_new_tokens = cfg.maxNewTokens;
    param.max_context_len = cfg.maxContextLen;
    param.skip_special_token = cfg.skipSpecialToken;
    param.img_start = cfg.imgStart;
    param.img_end = cfg.imgEnd;
    param.img_content = cfg.imgContent;
    param.extend_param.base_domain_id = cfg.baseDomainID;
    
    tik(&ctx->vlmStatsTime->initModelCost);
    int ret = rkllm_init(ctx->llmHandle, &param, callback);
    if (ret == 0) {
        printf("rkllm init success\n");
    } else {
        printf("rkllm init failed\n");
        // exit_handler(-1);
        return -1;
    }
    tok(&ctx->vlmStatsTime->initModelCost);

    ctx->vlmStatsTime->isFirstTokens = 1;
    ctx->vlmStatsTime->numGenTokens = 0;
    ctx->vlmStatsTime->visionCost = 0.f;

    return 0;
}

int rkai_vlm_llm_run(rkaiCtxVLM* ctx, rkaiVLMCfg cfg,rkaiVisionToLLM *vtl) {

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
    // tik(&ctx->vlmStatsTime->ttftCost);
    // tik(&ctx->vlmStatsTime->tpsCost);
    ret = rkllm_run(*ctx->llmHandle, &rkllm_input, &rkllm_infer_params, ctx);
    if(ret != 0) {
        printf("rkllm_run fail:ret = %d\n",ret);
        return ret;
    }
    // tok(&ctx->vlmStatsTime->tpsCost);
   
    ctx->vlmStatsTime->numGenTokens = 0;

    return 0;
}

int rkai_vlm_llm_release(rkaiCtxVLM* ctx) {
    if(ctx->vlmStatsTime != NULL) {
        free(ctx->vlmStatsTime);
        ctx->vlmStatsTime = NULL;
    }

    if (ctx->llmHandle != NULL) {
        rkllm_destroy(*ctx->llmHandle);
        ctx->llmHandle = NULL;
    }

    return 0;
}
