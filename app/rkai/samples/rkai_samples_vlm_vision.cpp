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
#ifdef RKAI_DEPENDMENT_OPENCV4
#include "opencv2/opencv.hpp"
#endif

#include "easy_timer.h"
#include "rkai_vlm_vision.h"

// Expand the image into a square and fill it with the specified background color
#ifdef RKAI_DEPENDMENT_OPENCV4
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

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --model <path>  RKNN model file (mandatory)\n");
    printf("  --image <path>  Input image file (mandatory)\n");
    printf("Example:\n");
    printf("  %s rkai_VLM_Vision_Demo --model <path>/fastvlm-0.5b-fp16.rv1126b.rknn --image <path>/demo.jpg\n", prog);
}

static int rkai_mjpeg_get_resolutin(const char *path, int *width, int *height)
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

int main(int argc, char **argv) {
    char *model_path = NULL;
    char *image_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            image_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option or missing value: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    if (!model_path || !image_path) {
        fprintf(stderr, "Error: missing mandatory arguments.\n");
        print_usage(argv[0]);
        return -1;
    }

    if (!strstr(model_path, ".rknn")) {
        printf("--model must be an .rknn file\n");
        return 1;
    }

    int ret = 0;
    rkaiCtxVision *rkai_ctx = (rkaiCtxVision *)malloc(sizeof(rkaiCtxVision));
    memset(rkai_ctx, 0, sizeof(rkaiCtxVision));

    rkaiTimer t_ms;
    tik(&t_ms);
    ret = rkai_vlm_vision_init(model_path, rkai_ctx);
    if (ret != 0) {
        printf("rkai_vlm_vision_init fail! ret=%d model_path=%s\n", ret, model_path);
        return -1;
    }
    tok(&t_ms);
    printf("%s: Model loaded in %8.2f ms\n", __func__, get_time(&t_ms));

    int32_t input_len = rkai_ctx->model_channel*rkai_ctx->model_height*rkai_ctx->model_width; 
    unsigned char *data = (unsigned char*)malloc(sizeof(unsigned char)*input_len); 
    memset(data,0,sizeof(unsigned char)*input_len);
    if (strstr(image_path, ".npy")) {
        int32_t image_len = load_npy(rkai_ctx,image_path,data);
        if(image_len != input_len) {
            printf("input size is not match model inputs.\n");
            return -1;
        }
    } else {
        #ifdef RKAI_DEPENDMENT_OPENCV4
        cv::Mat img = cv::imread(image_path);
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);

        // Expand the image into a square and fill it with the specified background color (According the modeling_minicpmv.py)
        cv::Scalar background_color(127.5, 127.5, 127.5);
        cv::Mat square_img = expand2square(img, background_color);

        cv::Mat resized_img;
        cv::Size new_size(rkai_ctx->model_width, rkai_ctx->model_height);
        cv::resize(square_img, resized_img, new_size, 0, 0, cv::INTER_LINEAR);

        memcpy(data,resized_img.data,sizeof(unsigned char)*input_len);
        #else
        printf("Unspport input fomrat now.\n");
        return -1;
        #endif

    }
    rkaiVisionToLLM *vtol;
    vtol = (rkaiVisionToLLM *)malloc(sizeof(rkaiVisionToLLM));

    // float* img_vec;
    vtol->img_vec = (float*)malloc(sizeof(float)*rkai_ctx->img_elems);
    memset(vtol->img_vec,0,sizeof(float)*rkai_ctx->img_elems);

    rkaiTimer vision_run_ms;
    tik(&vision_run_ms);
    ret = rkai_vlm_vision_run(rkai_ctx, data, vtol);
    if (ret != 0) {
        printf("rkai_vlm_vision_run fail! ret=%d\n", ret);
    }
    tok(&vision_run_ms);
    printf("%s: Model run in %8.2f ms\n", __func__, get_time(&vision_run_ms));

    ret = rkai_vlm_vision_release(rkai_ctx);
    if (ret != 0) {
        printf("release_imgenc fail! ret=%d\n", ret);
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

    return 0;
}