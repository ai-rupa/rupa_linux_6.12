#ifndef _RKAI_VLM_VISION_H_
#define _RKAI_VLM_VISION_H_

#if !defined(RKAI_DISABLE_RKNN3)
#include "rknn3_api.h"
#include "float16.h"
#else
#include "rknn_api.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- Common types (both backends) --- */

typedef struct {
    float *img_vec;          /* float32 embeddings (rknpu2 path) */
#if !defined(RKAI_DISABLE_RKNN3)
    float16 *img_vec_fp16;   /* float16 embeddings (rknn3 path) */
#endif
    int32_t n_image_tokens;
    int32_t n_image;
    int32_t image_width;
    int32_t image_height;
} rkaiVisionToLLM;

#if !defined(RKAI_DISABLE_RKNN3)

/* --- rknn3 backend (RK3572) --- */

typedef struct {
    rknn3_context rknn3_ctx;
    rknn3_input_output_num io_num;
    rknn3_tensor* inputs;
    rknn3_tensor* outputs;
    int model_channel;
    int model_width;
    int model_height;
    int img_elems;
    int pruned_version_flag;
    /* Pre-allocated temp buffers for run */
    float16 *nchw_buf;          /* normalized NCHW float16 buffer */
    unsigned char *resize_buf;  /* resized RGB buffer */
} rkaiCtxVisionRknn3;

int rkai_vlm_vision_init_rknn3(const char* model_path, const char* weight_path,
                                rkaiCtxVisionRknn3* ctx, uint32_t core_mask);
int rkai_vlm_vision_run_rknn3(rkaiCtxVisionRknn3* ctx, unsigned char* img_data,
                               int img_width, int img_height, rkaiVisionToLLM* vtol);
int rkai_vlm_vision_release_rknn3(rkaiCtxVisionRknn3* ctx);
int64_t rkai_vlm_vision_query_memory_rknn3(rkaiCtxVisionRknn3* ctx);

#else

/* --- rknpu2 backend (original, RV1126B) --- */

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

int32_t load_npy(rkaiCtxVision* rkai_ctx,const char *input_path,unsigned char *data);

int rkai_vlm_vision_init(const char* model_path, rkaiCtxVision* rkai_ctx);
int rkai_vlm_vision_release(rkaiCtxVision* rkai_ctx);
int rkai_vlm_vision_run(rkaiCtxVision* rkai_ctx, void* img_data, rkaiVisionToLLM *vtol);

#endif /* !RKAI_DISABLE_RKNN3 */

#ifdef __cplusplus
}
#endif

#endif
