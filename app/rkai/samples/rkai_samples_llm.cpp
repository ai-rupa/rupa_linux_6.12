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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "rkai_llm.h"
#if defined(RKAI_DISABLE_RKNN3)
#include "rkllm/rkllm.h"
#endif
#include "rkai_ini.h"

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --llm_model <dir>    LLM model directory (auto-detect .rknn/.weight/.tokenizer.gguf/.embed.bin)\n");
    printf("  --config    <path>   Configuration ini file \n");
    printf("Example:\n");
    printf("  %s --config <path>/rkai_cfg.ini\n", prog);
    printf("  %s --config <path>/rkai_cfg.ini --llm_model <model_dir>\n", prog);
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
    if (!right) return NULL;

    /* If right is an absolute path, return it directly */
    if (right[0] == '/') {
        return strdup(right);
    }

    if (!left) return NULL;

    size_t len = strlen(left) + strlen(right) + 2;
    char  *out = (char *)malloc(len);
    if (!out) return NULL;

    snprintf(out, len, "%s/%s", left, right);

    char *p = out;
    while ((p = strstr(p, "//")) != NULL) memmove(p, p + 1, strlen(p));

    return out;
}

/* Find the first file in dir matching the given suffix.
 * Returns allocated path string or NULL if not found.
 */
static char *find_file_in_dir(const char *dir, const char *suffix)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;

    struct dirent *ent;
    char *result = NULL;
    size_t suffix_len = strlen(suffix);

    while ((ent = readdir(d)) != NULL) {
        size_t namelen = strlen(ent->d_name);
        if (namelen >= suffix_len &&
            strcmp(ent->d_name + namelen - suffix_len, suffix) == 0) {
            size_t path_len = strlen(dir) + 1 + namelen + 1;
            result = (char *)malloc(path_len);
            if (result) {
                snprintf(result, path_len, "%s/%s", dir, ent->d_name);
            }
            break;
        }
    }
    closedir(d);
    return result;
}

int main(int argc, char **argv) {
    char *llm_model  = NULL;
    char *config_path = NULL;
    bool llm_model_owned = false;  /* true when llm_model was heap-allocated (path_join) */

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

    char *model_dir = NULL;

    if (!llm_model) {
        const char *model_name = ini_get("llm_cfg", "llm_model","none");
        if(strcmp(model_name, "none") == 0) {
            printf("cannot get llm_model from .ini. please set --llm_model");
            return -1;
        }
        llm_model = path_join(prefix, model_name);
        llm_model_owned = true;
        printf(" llm_model:%s\n", llm_model);
    }

    /* If --llm_model points to a directory, auto-detect model files inside */
    DIR *dir_check = opendir(llm_model);
    if (dir_check) {
        closedir(dir_check);
        model_dir = strdup(llm_model);

        char *found = find_file_in_dir(model_dir, ".rkllm");
        if (found) {
            if (llm_model_owned) free(llm_model);
            llm_model = found;
            llm_model_owned = true;
        } else {
            found = find_file_in_dir(model_dir, ".rknn");
            if (found) {
                if (llm_model_owned) free(llm_model);
                llm_model = found;
                llm_model_owned = true;
            } else {
                printf("No .rkllm or .rknn file found in %s\n", model_dir);
                free(model_dir);
                return -1;
            }
        }
        printf(" llm_model:%s\n", llm_model);
    }

    bool is_rkllm = strstr(llm_model, ".rkllm") != NULL;
    bool is_rknn  = strstr(llm_model, ".rknn")  != NULL;
    if (!is_rkllm && !is_rknn) {
        printf("--llm_model must be an .rkllm or .rknn file or a directory containing one\n");
        return 1;
    }

    rkaiCfg cfg;
    cfg.modelPath = llm_model;
    cfg.weightPath = NULL;
    cfg.tokenizerPath = NULL;
    cfg.embeddingPath = NULL;
    cfg.rknn3CoreMask = 0;

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

    if (is_rknn) {
        cfg.rknn3CoreMask = (uint32_t)ini_get_int("llm_cfg", "rknn3_core_mask", 0xff);

        /* When --llm_model is a directory or was derived from one, auto-detect
         * companion files in that directory. Otherwise fall back to ini config. */
        if (model_dir) {
            cfg.weightPath = find_file_in_dir(model_dir, ".weight");
            cfg.tokenizerPath = find_file_in_dir(model_dir, ".tokenizer.gguf");
            cfg.embeddingPath = find_file_in_dir(model_dir, ".embed.bin");

            if (!cfg.weightPath || !cfg.tokenizerPath || !cfg.embeddingPath) {
                printf("Missing companion files in %s:\n", model_dir);
                if (!cfg.weightPath)    printf("  - no .weight file found\n");
                if (!cfg.tokenizerPath) printf("  - no .tokenizer.gguf file found\n");
                if (!cfg.embeddingPath) printf("  - no .embed.bin file found\n");
                free(model_dir);
                return -1;
            }
        } else {
            const char* weight_name = ini_get("llm_cfg", "llm_weight", "none");
            const char* tokenizer_name = ini_get("llm_cfg", "llm_tokenizer", "none");
            const char* embedding_name = ini_get("llm_cfg", "llm_embedding", "none");

            if (strcmp(weight_name, "none") == 0 || strcmp(tokenizer_name, "none") == 0 || strcmp(embedding_name, "none") == 0) {
                printf("For .rknn model, please set llm_weight/llm_tokenizer/llm_embedding in ini\n");
                return -1;
            }
            cfg.weightPath = path_join(prefix, weight_name);
            cfg.tokenizerPath = path_join(prefix, tokenizer_name);
            cfg.embeddingPath = path_join(prefix, embedding_name);
        }
        printf(" weightPath:%s\n", cfg.weightPath);
        printf(" tokenizerPath:%s\n", cfg.tokenizerPath);
        printf(" embeddingPath:%s\n", cfg.embeddingPath);
    }
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
        rkaiTimer total_timer;
        tik(&total_timer);
        ret = rkai_llm_run(ctx, &cfg);
        if(ret) {
            printf("rkai_llm_run fail,ret = %d\n",ret);
            rkai_llm_release(ctx);
            if(ctx) { free(ctx); ctx = NULL; }
            return ret;
        }
        tok(&total_timer);

        /* Print summary after each answer */
        if (is_rknn) {
            float ttft_ms = get_time(&g_llm_ttft);
            float total_ms = get_time(&total_timer);
            float tps = (total_ms > 0.f) ? (float)g_llm_gen_tokens / total_ms * 1000.f : 0.f;
            int64_t llm_mem = rkai_llm_query_memory(ctx);

            printf("\n===== RKNN3_LLM Results Summary =====\n\n");
            printf("TTFT:           %.3f ms\n", ttft_ms);
            printf("GenLen:         %d tokens\n", g_llm_gen_tokens);
            printf("TPS:            %.3f tokens/s\n", tps);
            printf("Total:          %.3f ms\n", total_ms);
            printf("LLMMemory:      %.3f MB\n", (float)llm_mem / (1024.0 * 1024.0));
            printf("\n=====================================\n\n");
            fflush(stdout);
        }
    }

    rkai_llm_release(ctx);
    if(ctx) {
        free(ctx);
        ctx = NULL;
    }

    if (is_rknn) {
        if (cfg.weightPath) free((void*)cfg.weightPath);
        if (cfg.tokenizerPath) free((void*)cfg.tokenizerPath);
        if (cfg.embeddingPath) free((void*)cfg.embeddingPath);
    }
    if (model_dir) free(model_dir);

    return 0;
}