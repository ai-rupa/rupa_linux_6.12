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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include "rkai_ini.h"
#include "easy_timer.h"
#include "rkai_vlm_vision.h"
#include "rkai_vlm_llm.h"

#ifdef DEPENDMENT_ROCKIT
#include "rk_mpi_vdec.h"
#include "rk_mpi_tde.h"
#include "rk_comm_vdec.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_mmz.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vdec.h"
#include "jpeglib.h"
#endif

#ifdef RKAI_DEPENDMENT_OPENCV4
#include "opencv2/opencv.hpp"
#endif

#if 0
static int load_config(const char *filename, rkaiVLMCfg *cfg) {
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

    cJSON *visionModelPath = cJSON_GetObjectItem(json, "vision_model_path");
    if (cJSON_IsString(visionModelPath)) {
        strncpy(cfg->visionModelPath, visionModelPath->valuestring, sizeof(cfg->visionModelPath) - 1);
        // printf("modelPath:%s\n",cfg->modelPath);
    }

    cJSON *llmModelPath = cJSON_GetObjectItem(json, "llm_model_path");
    if (cJSON_IsString(llmModelPath)) {
        strncpy(cfg->llmModelPath, llmModelPath->valuestring, sizeof(cfg->llmModelPath) - 1);
        // printf("modelPath:%s\n",cfg->modelPath);
    }

    cJSON *inputPath = cJSON_GetObjectItem(json, "input_path");
    if (cJSON_IsString(inputPath)) {
        strncpy(cfg->inputPath, inputPath->valuestring, sizeof(cfg->inputPath) - 1);
    }

    cJSON *imgStart = cJSON_GetObjectItem(json, "img_start");
    if (cJSON_IsString(imgStart)) {
        strncpy(cfg->imgStart, imgStart->valuestring, sizeof(cfg->imgStart) - 1);
        // printf("imgStart:%s\n",cfg->imgStart);
    }
    cJSON *imgEnd = cJSON_GetObjectItem(json, "img_end");
    if (cJSON_IsString(imgEnd)) {
        strncpy(cfg->imgEnd, imgEnd->valuestring, sizeof(cfg->imgEnd) - 1);
    }
    cJSON *imgContent = cJSON_GetObjectItem(json, "img_content");
    if (cJSON_IsString(imgContent)) {
        strncpy(cfg->imgContent, imgContent->valuestring, sizeof(cfg->imgContent) - 1);
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

#ifdef RKAI_DEPENDMENT_OPENCV4
// Expand the image into a square and fill it with the specified background color
static cv::Mat expand2square(const cv::Mat& img, const cv::Scalar& background_color) {
    int width = img.cols;
    int height = img.rows;

    // If the width and height are equal, return to the original image directly
    if (width == height) {
        return img.clone();
    }

    // Calculate the new size and create a new image
    int size = std::max(width, height);
    cv::Mat result(size, size, img.type(), background_color);

    // Calculate the image paste position
    int x_offset = (size - width) / 2;
    int y_offset = (size - height) / 2;

    // Paste the original image into the center of the new image
    cv::Rect roi(x_offset, y_offset, width, height);
    img.copyTo(result(roi));

    return result;
}
#endif

int rkai_mjpeg_get_resolutin(const char *path, int *width, int *height)
{
    int ret = -1;
    FILE *fp;
    char mark[2];
    char size[2];
    char data[4];

    fp = fopen(path, "rb");
    if (!fp)
        return ret;

    while (!feof(fp)) {
        fread(mark, 1, sizeof(mark), fp);
        if (mark[0] == 0xFF && mark[1] == 0xD8)
            continue;
        if (mark[0] != 0xFF)
            break;
        if (mark[1] == 0xC0 || mark[1] == 0xC2) {
            if(mark[1] == 0xC2) {
                printf("Only support baseline \n");
                printf("if you have ffmeg,you can use command  \
                    'ffmpeg -i <src.jpg> -c:v mjpeg -q:v 2 -pix_fmt yuvj420p <dst.jpg>' to convert it.\n");
                break;
            }
            fseek(fp, 3, SEEK_CUR);
            fread(data, 1, sizeof(data), fp);
            *height = data[0] * 256 + data[1];
            *width = data[2] * 256 + data[3];
            printf("%s: %dx%d\n", __func__, *width, *height);
            ret = 0;
            break;
        } else {
            fread(size, 1, sizeof(size), fp);
            fseek(fp, size[0] * 256 + size[1] - 2, SEEK_CUR);
            continue;
        }
    }

    fclose(fp);
    return ret;
}

#ifdef DEPENDMENT_ROCKIT
unsigned char *decode_jpeg(const char *image_name, int *width, int *height, int *channels)
{
    FILE *fp = fopen(image_name, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    *channels = cinfo.output_components; /* 3 for RGB, 1 for gray */

    size_t row_stride = *width * *channels;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
                        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    unsigned char *pixels = (unsigned char*)malloc(*height * row_stride);
    if (!pixels) {
        fclose(fp);
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }

    int y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(pixels + y * row_stride, buffer[0], row_stride);
        ++y;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    return pixels;
}

static int mjpeg_get_frame_size(int fd) {
	off_t start_pos = lseek(fd, 0, SEEK_CUR);
	if (start_pos < 0) {
		printf("lseek failed: %s", strerror(errno));
		return -1;
	}

	RK_U8 buffer[4] = {0};
	int length = 0;
	int bytes_count = 0;
	ssize_t bytes_read;

	while (1) {
		bytes_read = read(fd, &buffer[bytes_count], 1);
		if (bytes_read != 1) {
			if (bytes_read == 0 && bytes_count >= 2 && buffer[bytes_count - 2] == 0xFF &&
			    buffer[bytes_count - 1] == 0xD9) {
				lseek(fd, start_pos, SEEK_SET);
				return length;
			}
			lseek(fd, start_pos, SEEK_SET);
			return -1;
		}

		length++;
		bytes_count++;
		if (bytes_count >= 4) {
			if (buffer[0] == 0xFF && buffer[1] == 0xD9 && buffer[2] == 0xFF &&
			    buffer[3] == 0xD8) {
				lseek(fd, start_pos, SEEK_SET);
				return length - 2;
			}
			memmove(buffer, buffer + 1, 3);
			bytes_count = 3;
		}
	}

	return -1;
}
#endif

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --vision_model <path>   Vision RKNN model file\n");
    printf("  --llm_model    <path>   LLM RKLLM model file  \n");
    printf("  --image_path   <path>   Input image file      \n");
    printf("  --config       <path>   Configuration ini file\n");
    printf("Example:\n");
    printf("  %s rkai_VLM_Demo --config <path>/rkai_cfg.ini\n", prog);
    printf("  %s rkai_VLM_Demo --vision_model <path>/fastvlm-0.5b-fp16.rv1126b.rknn \\\n", prog);
    printf("     --llm_model <path>/fastvlm-0.5B-w8a8_level0.rv1126b.rkllm \\\n");
    printf("     --image_path <path>/demo.jpg \\\n");
    printf("     --config <path>/rkai_cfg.ini\n");
}

#ifdef DEPENDMENT_ROCKIT
static unsigned char* resize_use_tde(int32_t srcWidth,int32_t srcHeight,int32_t dstWidth,int32_t dstHeight,unsigned char* img_buffer) {

    int ret = -1;
    /* tde open */
	ret = RK_TDE_Open();
	assert(ret == RK_SUCCESS);
    // printf("%s %d\n",__func__,__LINE__);
    TDE_HANDLE hHandle = RK_TDE_BeginJob();
    if (RK_ERR_TDE_INVALID_HANDLE == hHandle) {
        printf("RK_TDE_BeginJob Failure\n");
       return NULL;
    }

    TDE_SURFACE_S stSrc, stDst,stPadDst;
    TDE_RECT_S stSrcRect, stDstRect,stPadDstRect;

    int g_u32TdeDstWidth = dstWidth;
    int g_u32TdeDstHeight = dstHeight;

    stSrc.u32Width = srcWidth;
	stSrc.u32Height = srcHeight;
	stSrc.enColorFmt = RK_FMT_RGB888;
	stSrc.enComprocessMode = COMPRESS_MODE_NONE;
	stSrcRect.s32Xpos = 0;
	stSrcRect.s32Ypos = 0;
	stSrcRect.u32Width = srcWidth;
	stSrcRect.u32Height = srcHeight;
    uint32_t img_size = srcWidth*srcHeight*3;
    ret = RK_MPI_SYS_MmzAlloc(&stSrc.pMbBlk, RK_NULL, RK_NULL,img_size);
    assert(ret == RK_SUCCESS);

    memcpy(RK_MPI_MB_Handle2VirAddr(stSrc.pMbBlk),img_buffer,img_size);
    stDst.u32Width = g_u32TdeDstWidth;
	stDst.u32Height = g_u32TdeDstHeight;
	stDst.enColorFmt = RK_FMT_RGB888;
	stDst.enComprocessMode = COMPRESS_MODE_NONE;
	stDstRect.s32Xpos = 0;
	stDstRect.s32Ypos = 0;
	stDstRect.u32Width = g_u32TdeDstWidth;
	stDstRect.u32Height = g_u32TdeDstHeight;
	RK_FLOAT stScaleWidth  = 1.0f,stScaleHeight = 1.0f,stScale = 1.0f;
	stScaleWidth = (RK_FLOAT)stSrcRect.u32Width /(RK_FLOAT)stDstRect.u32Width;
	stScaleHeight = (RK_FLOAT)stSrcRect.u32Height /(RK_FLOAT)stDstRect.u32Height;
    int32_t dstSize = stDst.u32Width *stDst.u32Height *3;
    ret = RK_MPI_SYS_MmzAlloc(&stDst.pMbBlk, RK_NULL, RK_NULL,dstSize);
    assert(ret == RK_SUCCESS);

    ret = RK_TDE_QuickFill(hHandle, &stDst, &stDstRect,0x808080);
    assert(ret == RK_SUCCESS);

    if(stScaleWidth>stScaleHeight) {
            stScale = stScaleWidth;
            stPadDstRect.u32Width = stDstRect.u32Width;
            stPadDstRect.u32Height = (RK_S32)stSrcRect.u32Height / stScaleWidth;
    }else{
        stScale = stScaleHeight;
        stPadDstRect.u32Height = stDstRect.u32Height;
        stPadDstRect.u32Width = (RK_S32)stSrcRect.u32Width / stScaleWidth;
    }
        //align
        // slight change image size for align
    if (stPadDstRect.u32Width %4 != 0) {
        stPadDstRect.u32Width -= stPadDstRect.u32Width % 4;
    }
    if (stPadDstRect.u32Height %4 != 0) {
        stPadDstRect.u32Height -= stPadDstRect.u32Height % 4;
    }
    // padding
    RK_S32 stPadHeight = stDstRect.u32Height - stPadDstRect.u32Height;
    RK_S32 stPadWidth = stDstRect.u32Width - stPadDstRect.u32Width;
        // center
    if(stScaleWidth > stScaleHeight) {
        stDstRect.s32Ypos = stPadHeight / 2;
        if(stDstRect.s32Ypos %2 !=0) {
            stDstRect.s32Ypos -= stDstRect.s32Ypos%2;
            if(stDstRect.s32Ypos < 0) {
                stDstRect.s32Ypos = 0;
            }
        }
        stDstRect.u32Height =  stPadDstRect.u32Height;	
    }else{
        stDstRect.s32Xpos = stPadWidth / 2;
        if(stDstRect.s32Xpos %2 !=0) {
            stDstRect.s32Xpos -= stDstRect.s32Xpos%2;
            if(stDstRect.s32Xpos < 0) {
                stDstRect.s32Xpos = 0;
            }
        }
        stDstRect.u32Width =  stPadDstRect.u32Width;	
    }
    printf("scale=%f dst_box=(%d %d %d %d) padding_w=%d padding_h=%d\n",
        stScale, stDstRect.s32Xpos, stDstRect.s32Ypos, stDstRect.u32Width, stDstRect.u32Height,
        stPadWidth, stPadHeight);
    ret = RK_TDE_QuickCopy(hHandle, &stSrc, &stSrcRect, &stDst, &stDstRect);
    assert(ret == RK_SUCCESS);

    ret = RK_TDE_EndJob(hHandle, RK_FALSE, RK_TRUE, -1);
    assert(ret == RK_SUCCESS);

    ret = RK_TDE_WaitForDone(hHandle);
    assert(ret == RK_SUCCESS);

    if(stSrc.pMbBlk) {
        RK_MPI_SYS_Free(stSrc.pMbBlk);
    }

    unsigned char *resized_img =NULL;
    resized_img = (unsigned char*)RK_MPI_MB_Handle2VirAddr(stDst.pMbBlk);

    return resized_img;
}
#endif

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

    char *vision_model = NULL;
    char *llm_model    = NULL;
    char *image_path   = NULL;
    char *config_path  = NULL;

    /* get all paramters */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--vision_model") == 0 && i + 1 < argc) {
            vision_model = argv[++i];
        } else if (strcmp(argv[i], "--llm_model") == 0 && i + 1 < argc) {
            llm_model = argv[++i];
        } else if (strcmp(argv[i], "--image_path") == 0 && i + 1 < argc) {
            image_path = argv[++i];
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

    // /* check paramter */
    if (!config_path) {
        fprintf(stderr, "Error: missing mandatory arguments.\n");
        print_usage(argv[0]);
        return -1;
    }

    /* load INI */
    if (ini_load(config_path) != 0) {
        fprintf(stderr, "cannot load ini: %s\n", config_path);
        return -1;
    }

    char* prefix = get_model_dir_from_cfg(config_path);
    // printf("-----------prefix:%s----------\n",prefix);
    if(!vision_model) {
        const char *model_name = ini_get("vlm_cfg", "vision_model","none");
        if(model_name == "none") {
            printf("cannot get vision_model from .ini. please set --vision_model");
            return -1;
        }
        vision_model = path_join(prefix,model_name);
        printf(" vision_model:%s\n", vision_model);
    }

    if(!llm_model) {
        const char *model_name = ini_get("vlm_cfg", "llm_model","none");
        if(model_name == "none") {
            printf("cannot get llm_model from .ini. please set --llm_model");
            return -1;
        }
        llm_model = path_join(prefix,model_name);
        printf(" llm_model:%s\n", llm_model);
    }

    if(!image_path) {
        const char *image_name = ini_get("vlm_cfg", "image_path","none");
        if(image_name == "none") {
            printf("cannot get image_path from .ini. please set --image_path");
            return -1;
        }
        image_path = path_join(prefix,image_name);
        printf(" image_path:%s\n", image_path);
    }

    if (!strstr(vision_model, ".rknn")) {
        printf("--vision_model must be an .rknn file\n");
        return -1;
    }

    if (!strstr(llm_model, ".rkllm")) {
        printf("--llm_model must be an .rkllm file\n");
        return -1;
    }
    int ret = -1;
    #ifdef DEPENDMENT_ROCKIT
    ret = RK_MPI_SYS_Init();
	assert(ret == RK_SUCCESS);
    #endif

    rkaiVLMCfg cfg;
    cfg.visionModelPath = vision_model;
    cfg.llmModelPath    = llm_model;
    cfg.inputPath       = image_path;   // 原来是 .npy，现在指向 .jpg

    rkai_vlm_get_version();
    
    cfg.maxContextLen = ini_get_int("llm_cfg","max_context_len",1024);
    cfg.maxNewTokens = ini_get_int("llm_cfg","max_new_tokens",128); 
    cfg.topK = ini_get_float("llm_cfg","top_k",0.9);
    cfg.skipSpecialToken = ini_get_int("llm_cfg","skip_special_token",1);
    cfg.baseDomainID = ini_get_int("llm_cfg","base_domain_id",1);

    cfg.systemPrompt = ini_get("prompt","system_prompt","");
    cfg.promptPrefix = ini_get("prompt","prompt_prefix","");
    cfg.promptPostfix = ini_get("prompt","prompt_postfix","");

    cfg.imgStart = ini_get("tensor","img_start","");
    cfg.imgEnd = ini_get("tensor","img_end","");
    cfg.imgContent = ini_get("tensor","img_content","");

    rkaiCtxVision *rkai_ctx = (rkaiCtxVision *)malloc(sizeof(rkaiCtxVision));
    memset(rkai_ctx, 0, sizeof(rkaiCtxVision));

    rkaiCtxVLM *llm_ctx = (rkaiCtxVLM *)malloc(sizeof(rkaiCtxVLM));
    memset(llm_ctx,0,sizeof(rkaiCtxVLM));
    
    rkaiTimer t_ms;
    tik(&t_ms);
    ret = rkai_vlm_vision_init(cfg.visionModelPath, rkai_ctx);
    if (ret != 0) {
        printf("rkai_vlm_vision_init fail! ret=%d model_path=%s\n", ret, cfg.visionModelPath);
        return -1;
    }
    tok(&t_ms);
    printf("%s: Model Vision loaded in %8.2f ms\n", __func__, get_time(&t_ms));
    tik(&t_ms);
    // load llm model
    ret = rkai_vlm_llm_init(llm_ctx,cfg);
    if(ret != 0) {
        printf("rkai_vlm_llm_init fail! ret=%d model_path=%s\n", ret, cfg.llmModelPath);
        return -1;
    }
    tok(&t_ms);
    printf("%s: Model LLM loaded in %8.2f ms\n", __func__, get_time(&t_ms));
    // unsigned char *resized_img = NULL;
    int32_t input_len = rkai_ctx->model_channel*rkai_ctx->model_height*rkai_ctx->model_width; 
    unsigned char *data = (unsigned char*)malloc(sizeof(unsigned char)*input_len); 
    memset(data,0,sizeof(unsigned char)*input_len);
    if (strstr(cfg.inputPath, ".npy")) {
        int32_t image_len = load_npy(rkai_ctx,cfg.inputPath,data);
        if(image_len != input_len) {
            printf("input size is not match model inputs.\n");
            return -1;
        }
    } else {
        // #ifdef RKAI_DEPENDMENT_OPENCV4
        // // TODO...
        // cv::Mat img = cv::imread(cfg.inputPath);
        // cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

        // // Expand the image into a square and fill it with the specified background color (According the modeling_minicpmv.py)
        // cv::Scalar background_color(127.5, 127.5, 127.5);
        // cv::Mat square_img = expand2square(img, background_color);

        // cv::Mat resized_img;
        // cv::Size new_size(rkai_ctx->model_width, rkai_ctx->model_height);
        // cv::resize(square_img, resized_img, new_size, 0, 0, cv::INTER_LINEAR);

        // memcpy(data,resized_img.data,sizeof(unsigned char)*input_len);
        #ifdef DEPENDMENT_ROCKIT
        int width =0,height = 0,channel=0;
        unsigned char * img_buffer = decode_jpeg(image_path,&width,&height,&channel);
        if(img_buffer ==NULL) {
            printf("decode jpeg fail.\n");
            return -1;
        }
        
        data = resize_use_tde(width,height,rkai_ctx->model_width,rkai_ctx->model_height,img_buffer);
        if(img_buffer) {
            free(img_buffer);
            img_buffer =NULL;
        }
        #else
        printf("Unsupport *.jpg now,please open rockit and libjpeg. and you can test this demo by .npy\n");
        return -1;
        #endif
    }
    rkaiVisionToLLM *vtol;
    vtol = (rkaiVisionToLLM *)malloc(sizeof(rkaiVisionToLLM));
    memset(vtol,0,sizeof(rkaiVisionToLLM));

    // float* img_vec;
    vtol->img_vec = (float*)malloc(sizeof(float)*rkai_ctx->img_elems);
    memset(vtol->img_vec,0,sizeof(float)*rkai_ctx->img_elems);
    printf("rkai_ctx->img_elems: %d\n",rkai_ctx->img_elems);

    rkaiTimer vision_run_ms;
    tik(&vision_run_ms);
    ret = rkai_vlm_vision_run(rkai_ctx, data, vtol);
    if (ret != 0) {
        printf("rkai_vlm_vision_run fail! ret=%d\n", ret);
        return -1;
    }
    tok(&vision_run_ms);
    printf("%s: Model run in %8.2f ms\n", __func__, get_time(&vision_run_ms));

    
    // pre prompt
    const char *pre_input[] = {
        "<image>What is in the image?",
        "<image>这张图片中有什么元素？",
    };
    int pre_input_count = sizeof(pre_input) / sizeof(pre_input[0]);

    // 打印预置问题
    printf("\n**********************可输入以下问题对应序号获取回答/或自定义输入********************\n");
    for (int i = 0; i < pre_input_count; i++) {
        printf("[%d] %s\n", i, pre_input[i]);
    }
    printf("\n*************************************************************************\n");
    llm_ctx->vlmStatsTime->visionCost = get_time(&vision_run_ms);
    // printf("llm_ctx->vlmStatsTime->visionCost:%f\n",llm_ctx->vlmStatsTime->visionCost);
    char input_buf[1024];
    do {
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
        rkai_vlm_llm_run(llm_ctx, cfg,vtol);
    }while (1);

    ret = rkai_vlm_vision_release(rkai_ctx);
    if (ret != 0) {
        printf("rkai_vlm_vision_release fail! ret=%d\n", ret);
    }

    ret = rkai_vlm_llm_release(llm_ctx);
    if (ret != 0) {
        printf("rkai_vlm_llm_release fail! ret=%d\n", ret);
    }

    if (rkai_ctx) {
        free(rkai_ctx);
        rkai_ctx = NULL;
    }
    if(data) {
        free(data);
        data = NULL;
    }
    if(vtol->img_vec){
        free(vtol->img_vec);
        vtol->img_vec = NULL;
    }
    if(llm_ctx) {
        free(llm_ctx);
        llm_ctx = NULL;
    }
    if(vtol){
        free(vtol);
        vtol = NULL;
    }
    #ifdef DEPENDMENT_ROCKIT
    ret = RK_MPI_SYS_Exit();
    #endif

    if(vision_model) {
        free(vision_model);
        vision_model = NULL;
    }

    if(llm_model) {
        free(llm_model);
        llm_model = NULL;
    }

    if(image_path) {
        free(image_path);
        image_path = NULL;
    }

    return 0;
}
