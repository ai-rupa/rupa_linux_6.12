#include "rknn_api.h"

#ifndef _RKAI_VLM_VISION_H_
#define _RKAI_VLM_VISION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rknn_context rknn_ctx;
    rknn_input_output_num io_num;
    rknn_tensor_attr* input_attrs;
    rknn_tensor_attr* output_attrs;
    int model_channel;
    int model_width;
    int model_height;
    int img_elems;
} rkaiCtxVision;

typedef struct {
    float *img_vec;
    int32_t n_image_tokens;
    int32_t n_image;
    int32_t image_width;
    int32_t image_height;
} rkaiVisionToLLM;

int32_t load_npy(rkaiCtxVision* rkai_ctx,const char *input_path,unsigned char *data);

int rkai_vlm_vision_init(const char* model_path, rkaiCtxVision* rkai_ctx);

int rkai_vlm_vision_release(rkaiCtxVision* rkai_ctx);

int rkai_vlm_vision_run(rkaiCtxVision* rkai_ctx, void* img_data, rkaiVisionToLLM *vtol);


#ifdef __cplusplus
}
#endif

#endif