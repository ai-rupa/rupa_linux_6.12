#include "rkai_vlm_vision.h"

#if !defined(RKAI_DISABLE_RKNN3)
/* RKNN3 backend: rkai_vlm_vision.h already includes rknn3_api.h and float16.h */
#else

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cnpy.h"

using namespace cnpy;

static int rkai_read_model_from_file(const char *path, char **out_data) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("fopen %s fail!\n", path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    char *data = (char *)malloc(file_size + 1);
    data[file_size] = 0;
    fseek(fp, 0, SEEK_SET);
    if (file_size != fread(data, 1, file_size, fp)) {
        printf("fread %s fail!\n", path);
        free(data);
        fclose(fp);
        return -1;
    }
    if (fp) {
        fclose(fp);
    }
    *out_data = data;
    return file_size;
}

static void dump_tensor_attr(rknn_tensor_attr *attr, int32_t is_inputs) {
    if (is_inputs) {
        printf("========== Inputs Attribute ==========\n");
    } else {
        printf("========== Outputs Attribute ==========\n");
    }
    printf(" Index       : %-4d\n", attr->index);
    printf(" Name        : %s\n", attr->name);
    printf(" Num Dims    : %-4d\n", attr->n_dims);

    printf(" Dims        : [");
    for (int i = 0; i < attr->n_dims; i++) {
        printf("%d", attr->dims[i]);
        if (i < attr->n_dims - 1) printf(", ");
    }
    printf("]\n");

    printf(" Num Elems   : %-8d\n", attr->n_elems);
    printf(" Size        : %-8d bytes\n", attr->size);
    printf(" Format      : %s\n", get_format_string(attr->fmt));
    printf(" Type        : %s\n", get_type_string(attr->type));
    printf(" Qnt Type    : %s\n", get_qnt_type_string(attr->qnt_type));
    printf(" Zero Point  : %-8d\n", attr->zp);
    printf(" Scale       : %-8f\n", attr->scale);
    printf("======================================\n\n");
}

int32_t load_npy(rkaiCtxVision *rkai_ctx, const char *input_path, unsigned char *data) {
    int req_height = 0;
    int req_width = 0;
    int req_channel = 0;

    printf("Loading %s\n", input_path);

    switch (rkai_ctx->input_attrs->fmt) {
        case RKNN_TENSOR_NHWC:
            req_height = rkai_ctx->input_attrs->dims[1];
            req_width = rkai_ctx->input_attrs->dims[2];
            req_channel = rkai_ctx->input_attrs->dims[3];
            break;
        case RKNN_TENSOR_NCHW:
            req_height = rkai_ctx->input_attrs->dims[2];
            req_width = rkai_ctx->input_attrs->dims[3];
            req_channel = rkai_ctx->input_attrs->dims[1];
            break;
        case RKNN_TENSOR_UNDEFINED:
            break;
        default:
            printf("meet unsupported layout\n");
            return -1;
    }

    NpyArray npy_data = npy_load(input_path);

    int type_bytes = npy_data.word_size;
    std::string typeName = npy_data.typeName;

    printf("npy data type:%s\n", typeName.c_str());

    // if (typeName == "int8") {
    //     *input_type = RKNN_TENSOR_INT8;
    // } else if (typeName == "uint8") {
    //     *input_type = RKNN_TENSOR_UINT8;
    // } else if (typeName == "float16") {
    //     *input_type = RKNN_TENSOR_FLOAT16;
    // } else if (typeName == "float32") {
    //     *input_type = RKNN_TENSOR_FLOAT32;
    // } else if (typeName == "8") {
    //     *input_type = RKNN_TENSOR_BOOL;
    // } else if (typeName == "int64") {
    //     *input_type = RKNN_TENSOR_INT64;
    // }

    // npy shape = NHWC
    int npy_shape[4] = { 1, 1, 1, 1 };

    int start = npy_data.shape.size() == 4 ? 0 : 1;
    for (size_t i = 0; i < npy_data.shape.size() && i < 4; ++i) {
        npy_shape[start + i] = npy_data.shape[i];
    }

    int height = npy_shape[1];
    int width = npy_shape[2];
    int channel = npy_shape[3];

    if ((rkai_ctx->input_attrs->fmt != RKNN_TENSOR_UNDEFINED) &&
        (width != req_width || height != req_height || channel != req_channel)) {
        printf("npy shape match failed!, (%d, %d, %d) != (%d, %d, %d)\n", height, width, channel, req_height, req_width,
               req_channel);
        // return NULL;
        return -1;
    }

    int32_t npy_bytes = npy_data.num_bytes();
    int32_t buf_size = rkai_ctx->model_channel * rkai_ctx->model_height * rkai_ctx->model_width;
    if (npy_bytes > buf_size) {
        printf("npy data size (%d) exceeds expected input size (%d)\n", npy_bytes, buf_size);
        return -1;
    }

    memcpy(data, npy_data.data<unsigned char>(), npy_bytes);
    return npy_bytes;
}

int rkai_vlm_vision_init(const char *model_path, rkaiCtxVision *rkai_ctx) {
    int ret;

    rknn_context ctx = 0;

    ret = rknn_init(&ctx, (void *)model_path, 0, 0, NULL);
    if (ret < 0) {
        printf("rknn_init fail! ret=%d\n", ret);
        return -1;
    }

    // Get Model Input Output Number
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }

    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);
    // Get Model Input Info
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]), 1);
    }

    // Get Model Output Info
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(output_attrs[i]), 0);
    }

    // Set to context
    rkai_ctx->rknn_ctx = ctx;
    rkai_ctx->io_num = io_num;
    rkai_ctx->input_attrs = (rknn_tensor_attr *)malloc(io_num.n_input * sizeof(rknn_tensor_attr));
    memcpy(rkai_ctx->input_attrs, input_attrs, io_num.n_input * sizeof(rknn_tensor_attr));
    rkai_ctx->output_attrs = (rknn_tensor_attr *)malloc(io_num.n_output * sizeof(rknn_tensor_attr));
    memcpy(rkai_ctx->output_attrs, output_attrs, io_num.n_output * sizeof(rknn_tensor_attr));

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        // printf("model is NCHW input fmt\n");
        rkai_ctx->model_channel = input_attrs[0].dims[1];
        rkai_ctx->model_height = input_attrs[0].dims[2];
        rkai_ctx->model_width = input_attrs[0].dims[3];
    } else {
        // printf("model is NHWC input fmt\n");
        rkai_ctx->model_height = input_attrs[0].dims[1];
        rkai_ctx->model_width = input_attrs[0].dims[2];
        rkai_ctx->model_channel = input_attrs[0].dims[3];
    }
    rkai_ctx->img_elems = output_attrs[0].n_elems;
    printf("model input height=%d, width=%d, channel=%d,out_elems=%d\n",
           rkai_ctx->model_height, rkai_ctx->model_width, rkai_ctx->model_channel, rkai_ctx->img_elems);

    return 0;
}

int rkai_vlm_vision_release(rkaiCtxVision *rkai_ctx) {
    if (rkai_ctx->input_attrs != NULL) {
        free(rkai_ctx->input_attrs);
        rkai_ctx->input_attrs = NULL;
    }
    if (rkai_ctx->output_attrs != NULL) {
        free(rkai_ctx->output_attrs);
        rkai_ctx->output_attrs = NULL;
    }
    if (rkai_ctx->rknn_ctx != 0) {
        rknn_destroy(rkai_ctx->rknn_ctx);
        rkai_ctx->rknn_ctx = 0;
    }
    return 0;
}

int rkai_vlm_vision_run(rkaiCtxVision *rkai_ctx, void *img_data, rkaiVisionToLLM *vtol) {
    int ret;
    rknn_input inputs[1];
    rknn_output outputs[1];

    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    // Set Input Data
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].size = rkai_ctx->model_width * rkai_ctx->model_height * rkai_ctx->model_channel;
    inputs[0].buf = img_data;

    ret = rknn_inputs_set(rkai_ctx->rknn_ctx, 1, inputs);
    if (ret < 0) {
        printf("rknn_input_set fail! ret=%d\n", ret);
        return -1;
    }

    // Run
    ret = rknn_run(rkai_ctx->rknn_ctx, nullptr);
    if (ret < 0) {
        printf("rknn_run fail! ret=%d\n", ret);
        return -1;
    }

    // Get Output
    outputs[0].want_float = 1;
    ret = rknn_outputs_get(rkai_ctx->rknn_ctx, 1, outputs, NULL);
    if (ret < 0) {
        printf("rknn_outputs_get fail! ret=%d\n", ret);
        return -1;
    }

    // Post Process
    int32_t output_bytes = outputs[0].size;
    int32_t expected_bytes = sizeof(float) * rkai_ctx->img_elems;
    if (output_bytes > expected_bytes) {
        printf("output size (%d) exceeds expected (%d)\n", output_bytes, expected_bytes);
        rknn_outputs_release(rkai_ctx->rknn_ctx, 1, outputs);
        return -1;
    }
    memcpy(vtol->img_vec, outputs[0].buf, output_bytes);
    if (rkai_ctx->output_attrs[0].n_dims == 2) {
        vtol->n_image_tokens = rkai_ctx->output_attrs[0].dims[0];
    } else {  // is 3
        vtol->n_image_tokens = rkai_ctx->output_attrs[0].dims[1];
    }

    // printf("vtol->n_image_tokens:%d\n",vtol->n_image_tokens);
    vtol->n_image = 1;
    vtol->image_width = rkai_ctx->model_width;
    vtol->image_height = rkai_ctx->model_height;
    // Remeber to release rknn output
    rknn_outputs_release(rkai_ctx->rknn_ctx, 1, outputs);

    return ret;
}

#endif /* RKAI_DISABLE_RKNN3 rest of file is rknn3 backend */

#if !defined(RKAI_DISABLE_RKNN3)

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ---- RKNN3 Vision Implementation ---- */

/* Normalize uint8 RGB image and convert to NCHW float16.
 * Returns 0 on success, -1 on allocation failure. */
static int normalize_and_to_nchw(const unsigned char *src, float16 *dst, int width, int height)
{
    float mean[3] = {122.7f, 116.74f, 104.09f};
    float std_[3] = {68.5f, 66.6f, 70.3f};
    int num_pixels = width * height;

    /* normalize to NHWC float16, then transpose to NCHW */
    float16 *temp = (float16 *)malloc(num_pixels * 3 * sizeof(float16));
    if (!temp) return -1;

    for (int i = 0; i < num_pixels; ++i) {
        int idx = i * 3;
        temp[idx + 0] = fp32_to_fp16(((float)src[idx + 0] - mean[0]) / std_[0]);
        temp[idx + 1] = fp32_to_fp16(((float)src[idx + 1] - mean[1]) / std_[1]);
        temp[idx + 2] = fp32_to_fp16(((float)src[idx + 2] - mean[2]) / std_[2]);
    }

    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                int nhwc_idx = (h * width + w) * 3 + c;
                int nchw_idx = c * num_pixels + h * width + w;
                dst[nchw_idx] = temp[nhwc_idx];
            }
        }
    }
    free(temp);
    return 0;
}

/* Simple bilinear resize for RGB uint8 image */
static void resize_rgb(const unsigned char *src, int src_w, int src_h,
                       unsigned char *dst, int dst_w, int dst_h)
{
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            float sx = (float)x * src_w / dst_w;
            float sy = (float)y * src_h / dst_h;
            int x0 = (int)sx, y0 = (int)sy;
            int x1 = (x0 + 1 < src_w) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < src_h) ? y0 + 1 : y0;
            float fx = sx - x0, fy = sy - y0;
            for (int c = 0; c < 3; c++) {
                float v = (1 - fx) * (1 - fy) * src[(y0 * src_w + x0) * 3 + c]
                        + fx * (1 - fy) * src[(y0 * src_w + x1) * 3 + c]
                        + (1 - fx) * fy * src[(y1 * src_w + x0) * 3 + c]
                        + fx * fy * src[(y1 * src_w + x1) * 3 + c];
                dst[(y * dst_w + x) * 3 + c] = (unsigned char)(v + 0.5f);
            }
        }
    }
}

int rkai_vlm_vision_init_rknn3(const char *model_path, const char *weight_path,
                                rkaiCtxVisionRknn3 *ctx, uint32_t core_mask)
{
    if (!ctx || !model_path || !weight_path) return -1;

    int ret;
    rknn3_context rknn_ctx = 0;

    ret = rknn3_init(&rknn_ctx, nullptr);
    if (ret < 0) {
        printf("rknn3_init fail! ret=%d\n", ret);
        return -1;
    }

    ret = rknn3_load_model_from_path(rknn_ctx, model_path, weight_path);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_load_model_from_path fail! ret=%d\n", ret);
        rknn3_destroy(rknn_ctx);
        return -1;
    }

    /* Auto-generate core_mask if not specified */
    if (core_mask == 0) {
        uint32_t model_core_num = 0;
        rknn3_query(rknn_ctx, RKNN3_QUERY_CORE_NUMBER, &model_core_num, sizeof(model_core_num));
        for (uint32_t i = 0; i < model_core_num; i++) {
            core_mask |= (1U << i);
        }
    }

    rknn3_config config;
    memset(&config, 0, sizeof(config));
    config.run_core_mask = core_mask;

    ret = rknn3_model_init(rknn_ctx, &config);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_model_init fail! ret=%d\n", ret);
        rknn3_destroy(rknn_ctx);
        return -1;
    }

    /* Query IO info */
    rknn3_input_output_num io_num;
    ret = rknn3_query(rknn_ctx, RKNN3_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_query IN_OUT_NUM fail! ret=%d\n", ret);
        rknn3_destroy(rknn_ctx);
        return -1;
    }
    printf("vision model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    /* Query input attributes */
    rknn3_tensor_attr *input_attrs = (rknn3_tensor_attr *)malloc(io_num.n_input * sizeof(rknn3_tensor_attr));
    if (!input_attrs) { printf("vision init: malloc input_attrs failed\n"); rknn3_destroy(rknn_ctx); return -1; }
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn3_query(rknn_ctx, RKNN3_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn3_tensor_attr));
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_query INPUT_ATTR fail! ret=%d\n", ret);
            free(input_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
    }

    /* Query output attributes */
    rknn3_tensor_attr *output_attrs = (rknn3_tensor_attr *)malloc(io_num.n_output * sizeof(rknn3_tensor_attr));
    if (!output_attrs) { printf("vision init: malloc output_attrs failed\n"); free(input_attrs); rknn3_destroy(rknn_ctx); return -1; }
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn3_query(rknn_ctx, RKNN3_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn3_tensor_attr));
        if (ret != RKNN3_SUCCESS) {
            printf("rknn3_query OUTPUT_ATTR fail! ret=%d\n", ret);
            free(input_attrs); free(output_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
    }

    /* Allocate tensors */
    ctx->inputs = (rknn3_tensor *)malloc(io_num.n_input * sizeof(rknn3_tensor));
    ctx->outputs = (rknn3_tensor *)malloc(io_num.n_output * sizeof(rknn3_tensor));
    if (!ctx->inputs || !ctx->outputs) {
        printf("vision init: failed to allocate tensor arrays\n");
        free(ctx->inputs); free(ctx->outputs);
        rknn3_destroy(rknn_ctx);
        return -1;
    }
    memset(ctx->inputs, 0, io_num.n_input * sizeof(rknn3_tensor));
    memset(ctx->outputs, 0, io_num.n_output * sizeof(rknn3_tensor));

    for (int i = 0; i < io_num.n_input; i++) {
        ctx->inputs[i].attr = (rknn3_tensor_attr *)malloc(sizeof(rknn3_tensor_attr));
        if (!ctx->inputs[i].attr) {
            printf("vision init: malloc attr failed for input[%d]\n", i);
            for (int j = 0; j < i; j++) { rknn3_destroy_mem(rknn_ctx, ctx->inputs[j].mem); free(ctx->inputs[j].attr); }
            free(ctx->inputs); free(ctx->outputs);
            free(input_attrs); free(output_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
        memcpy(ctx->inputs[i].attr, &input_attrs[i], sizeof(rknn3_tensor_attr));
        ctx->inputs[i].mem = rknn3_create_mem(rknn_ctx, input_attrs[i].aligned_size,
                                               input_attrs[i].core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
        if (!ctx->inputs[i].mem) {
            printf("vision init: rknn3_create_mem failed for input[%d]\n", i);
            free(ctx->inputs[i].attr);
            for (int j = 0; j < i; j++) { rknn3_destroy_mem(rknn_ctx, ctx->inputs[j].mem); free(ctx->inputs[j].attr); }
            free(ctx->inputs); free(ctx->outputs);
            free(input_attrs); free(output_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
    }
    for (int i = 0; i < io_num.n_output; i++) {
        ctx->outputs[i].attr = (rknn3_tensor_attr *)malloc(sizeof(rknn3_tensor_attr));
        if (!ctx->outputs[i].attr) {
            printf("vision init: malloc attr failed for output[%d]\n", i);
            for (int j = 0; j < io_num.n_input; j++) { rknn3_destroy_mem(rknn_ctx, ctx->inputs[j].mem); free(ctx->inputs[j].attr); }
            for (int j = 0; j < i; j++) { rknn3_destroy_mem(rknn_ctx, ctx->outputs[j].mem); free(ctx->outputs[j].attr); }
            free(ctx->inputs); free(ctx->outputs);
            free(input_attrs); free(output_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
        memcpy(ctx->outputs[i].attr, &output_attrs[i], sizeof(rknn3_tensor_attr));
        ctx->outputs[i].mem = rknn3_create_mem(rknn_ctx, output_attrs[i].aligned_size,
                                                output_attrs[i].core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
        if (!ctx->outputs[i].mem) {
            printf("vision init: rknn3_create_mem failed for output[%d]\n", i);
            free(ctx->outputs[i].attr);
            for (int j = 0; j < io_num.n_input; j++) { rknn3_destroy_mem(rknn_ctx, ctx->inputs[j].mem); free(ctx->inputs[j].attr); }
            for (int j = 0; j < i; j++) { rknn3_destroy_mem(rknn_ctx, ctx->outputs[j].mem); free(ctx->outputs[j].attr); }
            free(ctx->inputs); free(ctx->outputs);
            free(input_attrs); free(output_attrs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
    }

    /* Extract model dimensions */
    ctx->pruned_version_flag = 0;
    if (input_attrs[0].layout == RKNN3_TENSOR_NCHW) {
        ctx->model_channel = input_attrs[0].shape[1];
        ctx->model_height = input_attrs[0].shape[2];
        ctx->model_width = input_attrs[0].shape[3];
    } else if (input_attrs[0].layout == RKNN3_TENSOR_NHWC) {
        ctx->model_channel = input_attrs[0].shape[3];
        ctx->model_height = input_attrs[0].shape[1];
        ctx->model_width = input_attrs[0].shape[2];
    } else {
        /* Unknown layout — try to use shape[2]/shape[3] as fallback for 4D tensors */
        if (input_attrs[0].n_dims >= 4) {
            ctx->model_channel = input_attrs[0].shape[1];
            ctx->model_height = input_attrs[0].shape[2];
            ctx->model_width = input_attrs[0].shape[3];
        } else {
            printf("vision init: unsupported input layout (n_dims=%d), cannot determine model dimensions\n",
                   input_attrs[0].n_dims);
            for (int i = 0; i < io_num.n_input; i++) {
                if (ctx->inputs[i].mem) rknn3_destroy_mem(rknn_ctx, ctx->inputs[i].mem);
                if (ctx->inputs[i].attr) free(ctx->inputs[i].attr);
            }
            for (int i = 0; i < io_num.n_output; i++) {
                if (ctx->outputs[i].mem) rknn3_destroy_mem(rknn_ctx, ctx->outputs[i].mem);
                if (ctx->outputs[i].attr) free(ctx->outputs[i].attr);
            }
            free(ctx->inputs);
            free(ctx->outputs);
            rknn3_destroy(rknn_ctx);
            return -1;
        }
    }

    ctx->img_elems = output_attrs[0].n_elems;
    ctx->rknn3_ctx = rknn_ctx;
    free(input_attrs); free(output_attrs);
    ctx->io_num = io_num;

    /* Pre-allocate temp buffers for run */
    int npixels = ctx->model_width * ctx->model_height;
    ctx->nchw_buf = (float16 *)malloc(npixels * 3 * sizeof(float16));
    ctx->resize_buf = (unsigned char *)malloc(ctx->model_width * ctx->model_height * 3);
    if (!ctx->nchw_buf || !ctx->resize_buf) {
        printf("failed to allocate vision temp buffers\n");
        for (int i = 0; i < io_num.n_input; i++) {
            if (ctx->inputs[i].mem) rknn3_destroy_mem(rknn_ctx, ctx->inputs[i].mem);
            if (ctx->inputs[i].attr) free(ctx->inputs[i].attr);
        }
        for (int i = 0; i < io_num.n_output; i++) {
            if (ctx->outputs[i].mem) rknn3_destroy_mem(rknn_ctx, ctx->outputs[i].mem);
            if (ctx->outputs[i].attr) free(ctx->outputs[i].attr);
        }
        free(ctx->inputs);
        free(ctx->outputs);
        if (ctx->nchw_buf) { free(ctx->nchw_buf); ctx->nchw_buf = nullptr; }
        if (ctx->resize_buf) { free(ctx->resize_buf); ctx->resize_buf = nullptr; }
        rknn3_destroy(rknn_ctx);
        return -1;
    }

    printf("vision model: %dx%d ch=%d out_elems=%d\n",
           ctx->model_width, ctx->model_height, ctx->model_channel, ctx->img_elems);

    return 0;
}

int rkai_vlm_vision_run_rknn3(rkaiCtxVisionRknn3 *ctx, unsigned char *img_data,
                               int img_width, int img_height, rkaiVisionToLLM *vtol)
{
    if (!ctx || !img_data || !vtol) return -1;
    int ret;

    /* Resize image if needed */
    unsigned char *processed = img_data;
    if (img_width != ctx->model_width || img_height != ctx->model_height) {
        resize_rgb(img_data, img_width, img_height, ctx->resize_buf, ctx->model_width, ctx->model_height);
        processed = ctx->resize_buf;
    }

    if (!ctx->inputs[0].mem || !ctx->inputs[0].mem->virt_addr) {
        printf("input virt_addr is NULL\n");
        return -1;
    }
    if (ctx->pruned_version_flag) {
        /* Normalize and convert to NCHW float16 using pre-allocated buffer */
        int npixels = ctx->model_width * ctx->model_height;
        if (normalize_and_to_nchw(processed, ctx->nchw_buf, ctx->model_width, ctx->model_height) != 0) {
            printf("normalize_and_to_nchw failed\n");
            return -1;
        }
        memcpy(ctx->inputs[0].mem->virt_addr, ctx->nchw_buf, npixels * 3 * sizeof(float16));
    } else {
        /* Direct copy (uint8 input) */
        memcpy(ctx->inputs[0].mem->virt_addr, processed,
               ctx->model_width * ctx->model_height * ctx->model_channel);
    }

    /* Sync input to device */
    for (int i = 0; i < ctx->io_num.n_input; i++) {
        rknn3_mem_sync(ctx->rknn3_ctx, ctx->inputs[i].mem, RKNN3_MEMORY_SYNC_TO_DEVICE);
    }

    /* Run */
    ret = rknn3_run(ctx->rknn3_ctx, ctx->inputs, ctx->io_num.n_input,
                    ctx->outputs, ctx->io_num.n_output);
    if (ret != RKNN3_SUCCESS) {
        printf("rknn3_run fail! ret=%d\n", ret);
        return -1;
    }

    /* Sync output from device */
    for (int i = 0; i < ctx->io_num.n_output; i++) {
        rknn3_mem_sync(ctx->rknn3_ctx, ctx->outputs[i].mem, RKNN3_MEMORY_SYNC_FROM_DEVICE);
    }

    /* Copy vision output to float16 embeddings */
    if (vtol->img_vec_fp16) {
        if (!ctx->outputs[0].mem || !ctx->outputs[0].mem->virt_addr) {
            printf("output virt_addr is NULL\n");
            return -1;
        }
        size_t copy_size = ctx->outputs[0].attr->n_elems * sizeof(float16);
        memcpy(vtol->img_vec_fp16, ctx->outputs[0].mem->virt_addr, copy_size);
    }

    /* Set metadata */
    if (ctx->outputs[0].attr->n_dims >= 2) {
        if (ctx->outputs[0].attr->n_dims == 2) {
            vtol->n_image_tokens = ctx->outputs[0].attr->shape[0];
        } else {
            vtol->n_image_tokens = ctx->outputs[0].attr->shape[1];
        }
    } else {
        printf("vision run: unexpected output n_dims=%d, expected >= 2\n", ctx->outputs[0].attr->n_dims);
        return -1;
    }
    vtol->n_image = 1;
    vtol->image_width = ctx->model_width;
    vtol->image_height = ctx->model_height;

    return 0;
}

int rkai_vlm_vision_release_rknn3(rkaiCtxVisionRknn3 *ctx)
{
    if (!ctx) return -1;

    for (int i = 0; i < ctx->io_num.n_input; i++) {
        if (ctx->inputs[i].mem) rknn3_destroy_mem(ctx->rknn3_ctx, ctx->inputs[i].mem);
        if (ctx->inputs[i].attr) free(ctx->inputs[i].attr);
    }
    for (int i = 0; i < ctx->io_num.n_output; i++) {
        if (ctx->outputs[i].mem) rknn3_destroy_mem(ctx->rknn3_ctx, ctx->outputs[i].mem);
        if (ctx->outputs[i].attr) free(ctx->outputs[i].attr);
    }
    free(ctx->inputs);
    free(ctx->outputs);
    if (ctx->nchw_buf) { free(ctx->nchw_buf); ctx->nchw_buf = nullptr; }
    if (ctx->resize_buf) { free(ctx->resize_buf); ctx->resize_buf = nullptr; }

    if (ctx->rknn3_ctx) {
        rknn3_destroy(ctx->rknn3_ctx);
        ctx->rknn3_ctx = 0;
    }
    return 0;
}

int64_t rkai_vlm_vision_query_memory_rknn3(rkaiCtxVisionRknn3 *ctx)
{
    if (!ctx || !ctx->rknn3_ctx) return 0;
    int64_t memory = 0;
    uint32_t model_core_num = 0;
    rknn3_query(ctx->rknn3_ctx, RKNN3_QUERY_CORE_NUMBER, &model_core_num, sizeof(model_core_num));
    rknn3_allocation_info *alloc = (rknn3_allocation_info *)malloc(sizeof(rknn3_allocation_info) * model_core_num);
    if (alloc) {
        int ret = rknn3_query(ctx->rknn3_ctx, RKNN3_QUERY_ALLOCATION_INFO, alloc,
                              sizeof(rknn3_allocation_info) * model_core_num);
        if (ret == 0) {
            for (uint32_t i = 0; i < model_core_num; i++) {
                memory += alloc[i].weight_mem.size;
                memory += alloc[i].command_mem.size;
            }
        }
        free(alloc);
    }
    return memory;
}

#endif /* !RKAI_DISABLE_RKNN3 */