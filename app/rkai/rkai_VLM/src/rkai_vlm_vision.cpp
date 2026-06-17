#include "rkai_vlm_vision.h"

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
    int file_size = ftell(fp);
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

#if 0
static void dump_tensor_attr(rknn_tensor_attr* attr)
{
    printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
            "zp=%d, scale=%f\n",
            attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
            attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
            get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}
#else
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
#endif

// static unsigned char *load_npy(const char *input_path, rknn_tensor_attr *input_attr, int *input_type, int *input_size) {
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

    // TODO: copy
    memcpy(data, npy_data.data<unsigned char>(), npy_data.num_bytes());

    // *input_size = npy_data.num_bytes();
    return npy_data.num_bytes();
}

int rkai_vlm_vision_init(const char *model_path, rkaiCtxVision *rkai_ctx) {
    int ret;

    int model_len = 0;
    char *model;
    rknn_context ctx = 0;
    int is_crypt = 0;

#if 0
    // Load RKNN Model
    model_len = rkai_read_model_from_file(model_path, &model);
    if (model == NULL) {
        printf("load_model fail!\n");
        return -1;
    }
#endif

    ret = rknn_init(&ctx, (void *)model_path, 0, 0, NULL);
    free(model);
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
    memcpy(vtol->img_vec, outputs[0].buf, outputs[0].size);
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