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
#include "rkai_llm.h"
#include "rkllm/rkllm.h"
#include "cJSON.h"
#include "rkai_ini.h"

#if 0
static int load_config(const char *filename, rkaiCfg *cfg) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Cannot open config file");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    rewind(file);

    // printf("json len: %d\n",length);
    char *data = (char *)malloc(length + 1);
    fread(data, 1, length, file);
    data[length] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free(data);
        return -1;
    }

    cJSON *modelPath = cJSON_GetObjectItem(json, "model_path");
    if (cJSON_IsString(modelPath)) {
        strncpy(cfg->modelPath, modelPath->valuestring, sizeof(cfg->modelPath) - 1);
    }
    cJSON *systemPrompt = cJSON_GetObjectItem(json, "system_prompt");
    if (cJSON_IsString(systemPrompt)) {
        strncpy(cfg->systemPrompt, systemPrompt->valuestring, sizeof(cfg->systemPrompt) - 1);
    }
    cJSON *promptPrefix = cJSON_GetObjectItem(json, "prompt_prefix");
    if (cJSON_IsString(promptPrefix)) {
        strncpy(cfg->promptPrefix, promptPrefix->valuestring, sizeof(cfg->promptPrefix) - 1);
    }
    cJSON *promptPostfix = cJSON_GetObjectItem(json, "prompt_postfix");
    if (cJSON_IsString(promptPostfix)) {
        strncpy(cfg->promptPostfix, promptPostfix->valuestring, sizeof(cfg->promptPostfix) - 1);
    }
    cJSON *maxContextLen = cJSON_GetObjectItem(json, "max_context_len");
    if (cJSON_IsNumber(maxContextLen)) {
        cfg->maxContextLen = maxContextLen->valueint;
    }
    cJSON *maxNewTokens = cJSON_GetObjectItem(json, "max_new_tokens");
    if (cJSON_IsNumber(maxNewTokens)) {
        cfg->maxNewTokens = maxNewTokens->valueint;
    }
    cJSON *topK = cJSON_GetObjectItem(json, "top_k");
    if (cJSON_IsNumber(topK)) {
        cfg->topK = topK->valueint;
    }
    cJSON *skipSpecialToken = cJSON_GetObjectItem(json, "skip_special_token");
    if (cJSON_IsNumber(skipSpecialToken)) {
        cfg->skipSpecialToken = skipSpecialToken->valueint;
    }
    cJSON *baseDomainID = cJSON_GetObjectItem(json, "base_domain_id");
    if (cJSON_IsNumber(baseDomainID)) {
        cfg->baseDomainID = baseDomainID->valueint;
    }
    cJSON_Delete(json);
    free(data);

    return 0;
}

#endif

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --llm_model <path>   RKLLM model file \n");
    printf("  --config    <path>   Configuration ini file \n");
    printf("Example:\n");
    printf("  %s rkai_LLM_Demo --config <path>/rkai_cfg.ini\n", prog);
    printf("  %s rkai_LLM_Demo --config <path>/rkai_cfg.ini --llm_model <LLM_model_path>\n", prog);
}
static char *get_model_dir_from_cfg(const char *cfg_path)
{
    if (!cfg_path) {
        return NULL;
    } 

    char *tmp = strdup(cfg_path);
    if (!tmp){
        return NULL;
    } 

    char *p = strrchr(tmp, '/');
    if (p) *p = '\0';

    p = strrchr(tmp, '/');
    if (p) *p = '\0';
    else { 
        free(tmp);
        return NULL; 
    }

    return tmp;
}

static char *path_join(const char *left, const char *right)
{
    if (!left || !right) return NULL;

    size_t len = strlen(left) + strlen(right) + 2;
    char  *out = (char *)malloc(len);
    if (!out) return NULL;

    snprintf(out, len, "%s/%s", left, right);

    char *p = out;
    while ((p = strstr(p, "//")) != NULL) memmove(p, p + 1, strlen(p));

    return out;
}

int main(int argc, char **argv) {
    char *llm_model  = NULL;
    char *config_path = NULL;

    /* get paramters */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--llm_model") == 0 && i + 1 < argc) {
            llm_model = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option or missing value: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    if (!config_path) {
        fprintf(stderr, "Error: missing mandatory arguments.\n");
        print_usage(argv[0]);
        return -1;
    }
    /* load  INI */
    if (ini_load(config_path) != 0) {
        fprintf(stderr, "cannot load ini: %s\n", config_path);
        return -1;
    }
    char* prefix = get_model_dir_from_cfg(config_path);

    if(!llm_model) {
        const char *model_name = ini_get("llm_cfg", "llm_model","none");
        if(model_name == "none") {
            printf("cannot get llm_model from .ini. please set --llm_model");
            return -1;
        }
        llm_model = path_join(prefix,model_name);
        printf(" llm_model:%s\n", llm_model);
    }


    if (!strstr(llm_model, ".rkllm")) {
        printf("--llm_model must be an .rkllm file\n");
        return 1;
    }

    rkaiCfg cfg;
    cfg.modelPath = llm_model;

    int ret = 0;

    rkai_llm_get_version();

    cfg.maxContextLen = ini_get_int("llm_cfg","max_context_len",1024);
    cfg.maxNewTokens = ini_get_int("llm_cfg","max_new_tokens",128); 
    cfg.topK = ini_get_float("llm_cfg","top_k",0.9);
    cfg.skipSpecialToken = ini_get_int("llm_cfg","skip_special_token",1);
    cfg.baseDomainID = ini_get_int("llm_cfg","base_domain_id",1);

    cfg.systemPrompt = ini_get("prompt","system_prompt","");
    cfg.promptPrefix = ini_get("prompt","prompt_prefix","");
    cfg.promptPostfix = ini_get("prompt","prompt_postfix","");
    // int32_t print_result = ini_get_int("results","print_result",1);

    // pre prompt
    const char *pre_input[] = {
        "把下面的现代文翻译成文言文: 到了春风和煦，阳光明媚的时候...",
        "以咏梅为题目，帮我写一首古诗，要求包含梅花、白雪等元素。",
        "上联: 江边惯看千帆过",
        "把这句话翻译成中文: Knowledge can be acquired from many sources...",
        "把这句话翻译成英文: RK3588是新一代高端处理器..."
    };
    int pre_input_count = sizeof(pre_input) / sizeof(pre_input[0]);

    // 打印预置问题
    printf("\n**********************可输入以下问题对应序号获取回答/或自定义输入********************\n");
    for (int i = 0; i < pre_input_count; i++) {
        printf("[%d] %s\n", i, pre_input[i]);
    }
    printf("\n*************************************************************************\n");

    rkaiCtxLLM* ctx =(rkaiCtxLLM*) malloc(sizeof(rkaiCtxLLM));
    memset(ctx,0,sizeof(rkaiCtxLLM));
    ret = rkai_llm_init(ctx, &cfg);
    
    char input_buf[1024];
    while (1) {
        printf("\nuser: ");
        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            continue;
        }
        // 去掉换行符
        input_buf[strcspn(input_buf, "\n")] = 0;

        if (strcmp(input_buf, "exit") == 0) {
            break;
        }

        // 判断是否输入了数字索引
        int idx = atoi(input_buf);
        if ((idx >= 0 && idx < pre_input_count) &&
            (strcmp(input_buf, "0") == 0 || idx > 0)) {
            strcpy(input_buf, pre_input[idx]);
            printf("%s\n", input_buf);
        }
        cfg.prompt = input_buf;
        // strncpy(cfg.prompt,input_buf,strlen(input_buf)-1);
        ret = rkai_llm_run(ctx, &cfg);
        if(ret) {
            printf("rkai_llm_run fail,ret = %d\n",ret);
            return ret;
        }
    }

    rkai_llm_release(ctx);
    if(ctx) {
        free(ctx);
        ctx = NULL;
    }

    return 0;
}