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
#include <dirent.h>

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
#elif !defined(RKAI_DISABLE_RKNN3)
/* RKNN3 path: use libjpeg directly (no rockit dependency) */
#include <jpeglib.h>
#endif

#ifdef RKAI_DEPENDMENT_OPENCV4
#include "opencv2/opencv.hpp"
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
        if (fread(mark, 1, sizeof(mark), fp) != sizeof(mark))
            break;
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
            if (fread(data, 1, sizeof(data), fp) != sizeof(data))
                break;
            *height = data[0] * 256 + data[1];
            *width = data[2] * 256 + data[3];
            printf("%s: %dx%d\n", __func__, *width, *height);
            ret = 0;
            break;
        } else {
            if (fread(size, 1, sizeof(size), fp) != sizeof(size))
                break;
            fseek(fp, size[0] * 256 + size[1] - 2, SEEK_CUR);
            continue;
        }
    }

    fclose(fp);
    return ret;
}

#if defined(DEPENDMENT_ROCKIT) || !defined(RKAI_DISABLE_RKNN3)
static unsigned char *decode_jpeg_lib(const char *image_name, int *width, int *height, int *channels)
{
    FILE *fp = fopen(image_name, "rb");
    if (!fp) { perror("fopen"); return NULL; }

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
    *channels = cinfo.output_components;

    size_t row_stride = *width * *channels;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
                        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    unsigned char *pixels = (unsigned char*)malloc(*height * row_stride);
    if (!pixels) { fclose(fp); jpeg_destroy_decompress(&cinfo); return NULL; }

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
#endif

#ifdef DEPENDENT_ROCKIT
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
    printf("  --vision_model <path>   Vision model directory or .rknn file\n");
    printf("  --llm_model    <path>   LLM model directory or .rkllm/.rknn file\n");
    printf("  --image_path   <path>   Input image file      \n");
    printf("  --config       <path>   Configuration ini file\n");
    printf("Example:\n");
    printf("  %s --config <path>/rkai_cfg.ini --vision_model <vision_dir> --llm_model <llm_dir>\n", prog);
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
    if (!right) return NULL;
    if (right[0] == '/') return strdup(right);
    if (!left) return NULL;

    size_t len = strlen(left) + strlen(right) + 2;
    char  *out = (char *)malloc(len);
    if (!out) return NULL;

    snprintf(out, len, "%s/%s", left, right);

    char *p = out;
    while ((p = strstr(p, "//")) != NULL) memmove(p, p + 1, strlen(p));

    return out;
}

/* Find the first file in dir matching the given suffix. */
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

/* Find the first file in dir whose name contains keyword and ends with suffix. */
static char *find_file_in_dir_contains(const char *dir, const char *keyword, const char *suffix)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;

    struct dirent *ent;
    char *result = NULL;
    size_t suffix_len = strlen(suffix);

    while ((ent = readdir(d)) != NULL) {
        size_t namelen = strlen(ent->d_name);
        if (namelen >= suffix_len &&
            strstr(ent->d_name, keyword) != NULL &&
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

/* If path points to a directory, auto-detect a model file matching suffix inside it.
 * Returns the resolved path (may be the original if it's already a file). */
static char *resolve_model_path(const char *path, const char *suffix)
{
    if (!path) return NULL;

    DIR *dir = opendir(path);
    if (dir) {
        closedir(dir);
        char *found = find_file_in_dir(path, suffix);
        if (found) {
            printf(" auto-detected: %s\n", found);
        } else {
            printf("No %s file found in %s\n", suffix, path);
        }
        return found;
    }
    return strdup(path);
}

int main(int argc, char **argv) {

    char *vision_model = NULL;
    char *llm_model    = NULL;
    char *image_path   = NULL;
    char *config_path  = NULL;
    bool vision_model_owned = false;
    bool llm_model_owned    = false;
    bool image_path_owned   = false;

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
        if(strcmp(model_name, "none") == 0) {
            printf("cannot get vision_model from .ini. please set --vision_model");
            return -1;
        }
        vision_model = path_join(prefix,model_name);
        vision_model_owned = true;
        printf(" vision_model:%s\n", vision_model);
    }

    if(!llm_model) {
        const char *model_name = ini_get("vlm_cfg", "llm_model","none");
        if(strcmp(model_name, "none") == 0) {
            printf("cannot get llm_model from .ini. please set --llm_model");
            return -1;
        }
        llm_model = path_join(prefix,model_name);
        llm_model_owned = true;
        printf(" llm_model:%s\n", llm_model);
    }

    if(!image_path) {
        const char *image_name = ini_get("vlm_cfg", "image_path","none");
        if(strcmp(image_name, "none") == 0) {
            printf("cannot get image_path from .ini. please set --image_path");
            return -1;
        }
        image_path = path_join(prefix,image_name);
        image_path_owned = true;
        printf(" image_path:%s\n", image_path);
    }

    /* If --vision_model or --llm_model points to a directory, auto-detect files.
     * Use "vision"/"llm" keyword in filename to distinguish the two models. */
    DIR *vdir = opendir(vision_model);
    if (vdir) {
        closedir(vdir);
        char *found = find_file_in_dir_contains(vision_model, "vision", ".rknn");
        if (!found) found = find_file_in_dir(vision_model, ".rknn");
        if (!found) {
            printf("--vision_model: no vision .rknn file found in %s\n", vision_model);
            return -1;
        }
        printf(" vision auto-detected: %s\n", found);
        if (vision_model_owned) free(vision_model);
        vision_model = found;
        vision_model_owned = true;
    }

    DIR *ldir = opendir(llm_model);
    if (ldir) {
        closedir(ldir);
        char *found = find_file_in_dir_contains(llm_model, "llm", ".rknn");
        if (!found) found = resolve_model_path(llm_model, ".rknn");
        if (!found) found = resolve_model_path(llm_model, ".rkllm");
        if (!found) {
            printf("--llm_model: no llm .rknn/.rkllm file found in %s\n", llm_model);
            return -1;
        }
        printf(" llm auto-detected: %s\n", found);
        if (llm_model_owned) free(llm_model);
        llm_model = found;
        llm_model_owned = true;
    }

    if (!strstr(vision_model, ".rknn")) {
        printf("--vision_model must be an .rknn file or directory\n");
        return -1;
    }

    bool is_rkllm = strstr(llm_model, ".rkllm") != NULL;
    bool is_rknn  = strstr(llm_model, ".rknn")  != NULL;
    if (!is_rkllm && !is_rknn) {
        printf("--llm_model must be an .rkllm or .rknn file or directory\n");
        return -1;
    }
    int ret = -1;
    #ifdef DEPENDMENT_ROCKIT
    ret = RK_MPI_SYS_Init();
	assert(ret == RK_SUCCESS);
    #endif

    rkaiVLMCfg cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.visionModelPath = vision_model;
    cfg.llmModelPath    = llm_model;
    cfg.inputPath       = image_path;

    /* Auto-detect RKNN3 companion files from model directory */
    if (is_rknn) {
        /* Get directory of llm model file */
        char *llm_dir = strdup(llm_model);
        char *slash = strrchr(llm_dir, '/');
        if (slash) *slash = '\0'; else { free(llm_dir); llm_dir = strdup("."); }
        cfg.llmWeightPath = find_file_in_dir_contains(llm_dir, "llm", ".weight");
        if (!cfg.llmWeightPath) cfg.llmWeightPath = find_file_in_dir(llm_dir, ".weight");
        cfg.tokenizerPath = find_file_in_dir(llm_dir, ".tokenizer.gguf");
        cfg.embeddingPath = find_file_in_dir_contains(llm_dir, "llm", ".embed.bin");
        if (!cfg.embeddingPath) cfg.embeddingPath = find_file_in_dir(llm_dir, ".embed.bin");
        free(llm_dir);
    }

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


    /* ============================================================
     * Vision init + image load + vision run (dual backend)
     * ============================================================ */
#if !defined(RKAI_DISABLE_RKNN3)
    /* RKNN3 path */
    {
        /* Auto-detect vision weight from same directory as vision model */
        char *vision_dir = strdup(vision_model);
        char *p = strrchr(vision_dir, '/');
        if (p) *p = '\0';
        char *vision_weight_path = find_file_in_dir_contains(vision_dir, "vision", ".weight");
        if (!vision_weight_path) vision_weight_path = find_file_in_dir(vision_dir, ".weight");
        free(vision_dir);
        if (!vision_weight_path) {
            printf("No .weight file found for vision model\n");
            return -1;
        }
        printf(" vision weight: %s\n", vision_weight_path);

        rkaiCtxVisionRknn3 *rknn3_vision_ctx = (rkaiCtxVisionRknn3 *)malloc(sizeof(rkaiCtxVisionRknn3));
        memset(rknn3_vision_ctx, 0, sizeof(rkaiCtxVisionRknn3));

        rkaiCtxVLM *llm_ctx = (rkaiCtxVLM *)malloc(sizeof(rkaiCtxVLM));
        memset(llm_ctx, 0, sizeof(rkaiCtxVLM));

        /* Load LLM first (needs larger contiguous memory) before vision model */
        rkaiTimer t_ms;
        tik(&t_ms);
        ret = rkai_vlm_llm_init(llm_ctx, cfg);
        if (ret != 0) {
            printf("rkai_vlm_llm_init fail! ret=%d model_path=%s\n", ret, cfg.llmModelPath);
            free(rknn3_vision_ctx); free(llm_ctx); free(vision_weight_path);
            return -1;
        }
        tok(&t_ms);
        printf("%s: Model LLM loaded in %8.2f ms\n", __func__, get_time(&t_ms));

        tik(&t_ms);
        ret = rkai_vlm_vision_init_rknn3(cfg.visionModelPath, vision_weight_path,
                                          rknn3_vision_ctx, 0xff);
        if (ret != 0) {
            printf("rkai_vlm_vision_init fail! ret=%d model_path=%s\n", ret, cfg.visionModelPath);
            rkai_vlm_llm_release(llm_ctx);
            free(rknn3_vision_ctx); free(llm_ctx); free(vision_weight_path);
            return -1;
        }
        tok(&t_ms);
        printf("%s: Model Vision loaded in %8.2f ms\n", __func__, get_time(&t_ms));

        /* Image loading: decode JPEG, vision_run_rknn3 does resize+normalize internally */
        unsigned char *data = NULL;
        int img_width = 0, img_height = 0;
        if (strstr(cfg.inputPath, ".npy")) {
            printf("RKNN3 backend does not support .npy input, use .jpg\n");
            rkai_vlm_llm_release(llm_ctx);
            rkai_vlm_vision_release_rknn3(rknn3_vision_ctx);
            free(rknn3_vision_ctx); free(llm_ctx); free(vision_weight_path);
            return -1;
        }
        int channel = 0;
        unsigned char *img_buffer = decode_jpeg_lib(cfg.inputPath, &img_width, &img_height, &channel);
        if (!img_buffer) {
            printf("decode jpeg fail: %s\n", cfg.inputPath);
            rkai_vlm_llm_release(llm_ctx);
            rkai_vlm_vision_release_rknn3(rknn3_vision_ctx);
            free(rknn3_vision_ctx); free(llm_ctx); free(vision_weight_path);
            return -1;
        }
        data = img_buffer;
        printf("image decoded: %dx%d %d channels\n", img_width, img_height, channel);

        rkaiVisionToLLM *vtol = (rkaiVisionToLLM *)malloc(sizeof(rkaiVisionToLLM));
        memset(vtol, 0, sizeof(rkaiVisionToLLM));
        vtol->img_vec_fp16 = (float16 *)malloc(sizeof(float16) * rknn3_vision_ctx->img_elems);
        memset(vtol->img_vec_fp16, 0, sizeof(float16) * rknn3_vision_ctx->img_elems);
        printf("rknn3_vision_ctx->img_elems: %d\n", rknn3_vision_ctx->img_elems);

        rkaiTimer vision_run_ms;
        tik(&vision_run_ms);
        ret = rkai_vlm_vision_run_rknn3(rknn3_vision_ctx, data, img_width, img_height, vtol);
        if (ret != 0) {
            printf("rkai_vlm_vision_run fail! ret=%d\n", ret);
            rkai_vlm_llm_release(llm_ctx);
            rkai_vlm_vision_release_rknn3(rknn3_vision_ctx);
            free(vtol->img_vec_fp16); free(vtol); free(data);
            free(rknn3_vision_ctx); free(llm_ctx); free(vision_weight_path);
            return -1;
        }
        tok(&vision_run_ms);
        printf("%s: Vision run in %8.2f ms\n", __func__, get_time(&vision_run_ms));

        /* Interactive loop */
        const char *pre_input[] = {
            "<image>What is in the image?",
            "<image>\xe8\xbf\x99\xe5\xbc\xa0\xe5\x9b\xbe\xe7\x89\x87\xe4\xb8\xad\xe6\x9c\x89\xe4\xbb\x80\xe4\xb9\x88\xe5\x85\x83\xe7\xb4\xa0\xef\xbc\x9f",
        };
        int pre_input_count = sizeof(pre_input) / sizeof(pre_input[0]);

        printf("\n**********************\xe5\x8f\xaf\xe8\xbe\x93\xe5\x85\xa5\xe4\xbb\xa5\xe4\xb8\x8b\xe9\x97\xae\xe9\xa2\x98\xe5\xaf\xb9\xe5\xba\x94\xe5\xba\x8f\xe5\x8f\xb7\xe8\x8e\xb7\xe5\x8f\x96\xe5\x9b\x9e\xe7\xad\x94/\xe6\x88\x96\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe8\xbe\x93\xe5\x85\xa5********************\n");
        for (int i = 0; i < pre_input_count; i++) {
            printf("[%d] %s\n", i, pre_input[i]);
        }
        printf("\n*************************************************************************\n");
        llm_ctx->vlmStatsTime->visionCost = get_time(&vision_run_ms);

        char input_buf[1024];
        do {
            printf("\nuser: ");
            if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
                continue;
            }
            input_buf[strcspn(input_buf, "\n")] = 0;

            if (strcmp(input_buf, "exit") == 0) {
                break;
            }

            int idx = atoi(input_buf);
            if ((idx >= 0 && idx < pre_input_count) &&
                (strcmp(input_buf, "0") == 0 || idx > 0)) {
                snprintf(input_buf, sizeof(input_buf), "%s", pre_input[idx]);
                printf("%s\n", input_buf);
            }
            cfg.prompt = input_buf;
            rkaiTimer total_timer;
            tik(&total_timer);
            rkai_vlm_llm_run(llm_ctx, cfg, vtol);
            tok(&total_timer);

            /* Print summary after each answer */
            {
                float ttft_ms = get_time(&g_vlm_llm_ttft);
                float total_ms = get_time(&total_timer);
                float tps = (total_ms > 0.f) ? (float)g_vlm_gen_tokens / total_ms * 1000.f : 0.f;
                int64_t vision_mem = rkai_vlm_vision_query_memory_rknn3(rknn3_vision_ctx);
                int64_t llm_mem = rkai_vlm_llm_query_memory(llm_ctx);

                printf("\n===== RKNN3_VLM Results Summary =====\n\n");
                printf("VisionCost:     %.3f ms\n", g_vlm_vision_cost_ms);
                printf("TTFT:           %.3f ms\n", ttft_ms);
                printf("GenLen:         %d tokens\n", g_vlm_gen_tokens);
                printf("TPS:            %.3f tokens/s\n", tps);
                printf("Total:          %.3f ms\n", total_ms);
                printf("VisionMemory:   %.3f MB\n", (float)vision_mem / (1024.0 * 1024.0));
                printf("LLMMemory:      %.3f MB\n", (float)llm_mem / (1024.0 * 1024.0));
                printf("TotalMemory:    %.3f MB\n", (float)(vision_mem + llm_mem) / (1024.0 * 1024.0));
                printf("\n=====================================\n\n");
                fflush(stdout);
                g_vlm_first_token = 1;
                g_vlm_gen_tokens = 0;
            }
        } while (1);

        /* Cleanup */
        rkai_vlm_vision_release_rknn3(rknn3_vision_ctx);
        free(rknn3_vision_ctx);
        rkai_vlm_llm_release(llm_ctx);
        if (data) free(data);
        if (vtol->img_vec_fp16) free(vtol->img_vec_fp16);
        if (llm_ctx) free(llm_ctx);
        if (vtol) free(vtol);
        if (vision_weight_path) free(vision_weight_path);
        if (cfg.llmWeightPath) free((void*)cfg.llmWeightPath);
        if (cfg.tokenizerPath) free((void*)cfg.tokenizerPath);
        if (cfg.embeddingPath) free((void*)cfg.embeddingPath);
    }
#else
    /* rknpu2 / RKLLM path (original, for RV1126B) */
    {
        rkaiCtxVision *rkai_ctx = (rkaiCtxVision *)malloc(sizeof(rkaiCtxVision));
        memset(rkai_ctx, 0, sizeof(rkaiCtxVision));

        rkaiCtxVLM *llm_ctx = (rkaiCtxVLM *)malloc(sizeof(rkaiCtxVLM));
        memset(llm_ctx, 0, sizeof(rkaiCtxVLM));

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
        ret = rkai_vlm_llm_init(llm_ctx, cfg);
        if (ret != 0) {
            printf("rkai_vlm_llm_init fail! ret=%d model_path=%s\n", ret, cfg.llmModelPath);
            return -1;
        }
        tok(&t_ms);
        printf("%s: Model LLM loaded in %8.2f ms\n", __func__, get_time(&t_ms));

        int32_t input_len = rkai_ctx->model_channel * rkai_ctx->model_height * rkai_ctx->model_width;
        unsigned char *data = (unsigned char *)malloc(sizeof(unsigned char) * input_len);
        memset(data, 0, sizeof(unsigned char) * input_len);
        int img_width = 0, img_height = 0;

        if (strstr(cfg.inputPath, ".npy")) {
            int32_t image_len = load_npy(rkai_ctx, cfg.inputPath, data);
            if (image_len != input_len) {
                printf("input size is not match model inputs.\n");
                return -1;
            }
        } else {
#ifdef DEPENDENT_ROCKIT
            int channel = 0;
            unsigned char *img_buffer = decode_jpeg(image_path, &img_width, &img_height, &channel);
            if (img_buffer == NULL) {
                printf("decode jpeg fail.\n");
                return -1;
            }
            unsigned char *resized = resize_use_tde(img_width, img_height,
                                                     rkai_ctx->model_width, rkai_ctx->model_height, img_buffer);
            free(img_buffer);
            if (resized) {
                memcpy(data, resized, input_len);
            } else {
                printf("resize failed\n");
                return -1;
            }
#else
            printf("Unsupport *.jpg without rockit. Use .npy or enable rockit.\n");
            return -1;
#endif
        }

        rkaiVisionToLLM *vtol = (rkaiVisionToLLM *)malloc(sizeof(rkaiVisionToLLM));
        memset(vtol, 0, sizeof(rkaiVisionToLLM));
        vtol->img_vec = (float *)malloc(sizeof(float) * rkai_ctx->img_elems);
        memset(vtol->img_vec, 0, sizeof(float) * rkai_ctx->img_elems);
        printf("rkai_ctx->img_elems: %d\n", rkai_ctx->img_elems);

        rkaiTimer vision_run_ms;
        tik(&vision_run_ms);
        ret = rkai_vlm_vision_run(rkai_ctx, data, vtol);
        if (ret != 0) {
            printf("rkai_vlm_vision_run fail! ret=%d\n", ret);
            return -1;
        }
        tok(&vision_run_ms);
        printf("%s: Vision run in %8.2f ms\n", __func__, get_time(&vision_run_ms));

        /* Interactive loop */
        const char *pre_input[] = {
            "<image>What is in the image?",
            "<image>\xe8\xbf\x99\xe5\xbc\xa0\xe5\x9b\xbe\xe7\x89\x87\xe4\xb8\xad\xe6\x9c\x89\xe4\xbb\x80\xe4\xb9\x88\xe5\x85\x83\xe7\xb4\xa0\xef\xbc\x9f",
        };
        int pre_input_count = sizeof(pre_input) / sizeof(pre_input[0]);

        printf("\n**********************\xe5\x8f\xaf\xe8\xbe\x93\xe5\x85\xa5\xe4\xbb\xa5\xe4\xb8\x8b\xe9\x97\xae\xe9\xa2\x98\xe5\xaf\xb9\xe5\xba\x94\xe5\xba\x8f\xe5\x8f\xb7\xe8\x8e\xb7\xe5\x8f\x96\xe5\x9b\x9e\xe7\xad\x94/\xe6\x88\x96\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe8\xbe\x93\xe5\x85\xa5********************\n");
        for (int i = 0; i < pre_input_count; i++) {
            printf("[%d] %s\n", i, pre_input[i]);
        }
        printf("\n*************************************************************************\n");
        llm_ctx->vlmStatsTime->visionCost = get_time(&vision_run_ms);

        char input_buf[1024];
        do {
            printf("\nuser: ");
            if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
                continue;
            }
            input_buf[strcspn(input_buf, "\n")] = 0;

            if (strcmp(input_buf, "exit") == 0) {
                break;
            }

            int idx = atoi(input_buf);
            if ((idx >= 0 && idx < pre_input_count) &&
                (strcmp(input_buf, "0") == 0 || idx > 0)) {
                snprintf(input_buf, sizeof(input_buf), "%s", pre_input[idx]);
                printf("%s\n", input_buf);
            }
            cfg.prompt = input_buf;
            rkai_vlm_llm_run(llm_ctx, cfg, vtol);
        } while (1);

        /* Cleanup */
        rkai_vlm_vision_release(rkai_ctx);
        free(rkai_ctx);
        rkai_vlm_llm_release(llm_ctx);
        if (data) free(data);
        if (vtol->img_vec) free(vtol->img_vec);
        if (llm_ctx) free(llm_ctx);
        if (vtol) free(vtol);
    }
#endif /* RKAI_DISABLE_RKNN3 */

#ifdef DEPENDENT_ROCKIT
    RK_MPI_SYS_Exit();
#endif

    if (vision_model_owned) { free(vision_model); vision_model = NULL; }
    if (llm_model_owned) { free(llm_model); llm_model = NULL; }
    if (image_path_owned) { free(image_path); image_path = NULL; }
    if (prefix) { free(prefix); prefix = NULL; }

    return 0;
}
