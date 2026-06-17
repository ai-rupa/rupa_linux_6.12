#include "cnpy.h"
#include "float16.h"
#include "rknn3_api.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#define DEFAULT_LOOP_COUNT 1

#define RKNPU_MAX_INPUT_NUM 16
#define RKNPU_MAX_OUTPUT_NUM 16

#define RKNPU_B16_SUBC 16
#define RKNPU_B8_SUBC  32

#define STRINGIFY(x) #x

static int getAlignHW(int hw)
{
  if (hw == 1) {
    return 1;
  } else {
    return (hw + 3) / 4 * 4;
  }
}

// 添加自定义的strdup实现
static char* _strdup(const char* str)
{
  size_t len     = strlen(str) + 1;
  char*  new_str = (char*)malloc(len);
  if (new_str) {
    memcpy(new_str, str, len);
  }
  return new_str;
}
// #if 0
static void dump_tensor_attr(rknn3_tensor_attr* attrs)
{
  std::string shape_str = "";
  for (uint32_t j = 0; j < attrs->n_dims; j++) {
    shape_str += std::to_string(attrs->shape[j]);
    if (j < attrs->n_dims - 1) {
      shape_str += ", ";
    }
  }

  std::string stride_str = "";
  for (uint32_t j = 0; j < attrs->n_stride; j++) {
    stride_str += std::to_string(attrs->stride[j]);
    if (j < attrs->n_stride - 1) {
      stride_str += ", ";
    }
  }

  printf("  name=%s, n_dims=%d, shape=[%s], stride=[%s], aligned_size=%ld, layout=%s, dtype=%s, "
             "qnt_type=%s, scale=%.5f, zero_point=%d\n",
             attrs->name, attrs->n_dims, shape_str.c_str(), stride_str.c_str(), attrs->aligned_size, rknn3_get_layout_string(attrs->layout),
             rknn3_get_type_string(attrs->dtype), rknn3_get_qnt_type_string(attrs->qnt_type), attrs->qnt_info.scale, attrs->qnt_info.zero_point);
}

static void reverse(char* start, char* end)
{
  while (start < end) {
    char temp = *start;
    *start    = *end;
    *end      = temp;
    start++;
    end--;
  }
}

static int int2str(int num, char* str, int d)
{
  int i = 0;

  do {
    str[i++] = (num % 10) + '0';
    num      = num / 10;
  } while (num > 0);

  while (i < d) {
    str[i++] = '0';
  }

  reverse(str, &str[i - 1]);
  str[i] = '\0';

  return i;
}

/**
 * @brief 打印数据值，根据数据类型
 *
 * @param data 数据指针
 * @param type 数据类型
 * @param index 数据索引
 */
static void print_data_value(void* data, rknn3_tensor_type type, size_t index)
{
  switch (type) {
  case RKNN3_TENSOR_FLOAT16: {
    float16* fp16_data = (float16*)data;
    uint16_t fp16_val  = *(uint16_t*)&fp16_data[index];
    printf("[%d] FP16: %d \n", index, fp16_val);
    break;
  }
  case RKNN3_TENSOR_FLOAT32: {
    float* fp32_data = (float*)data;
    printf("[%d] FP32: %f\n", index, fp32_data[index]);
    break;
  }
  case RKNN3_TENSOR_INT8: {
    int8_t* int8_data = (int8_t*)data;
    printf("[%d] INT8: %d\n", index, int8_data[index]);
    break;
  }
  case RKNN3_TENSOR_INT16: {
    int16_t* int16_data = (int16_t*)data;
    printf("[%d] INT16: %d\n", index, int16_data[index]);
    break;
  }
  case RKNN3_TENSOR_INT32: {
    int32_t* int32_data = (int32_t*)data;
    printf("[%d] INT32: %d\n", index, int32_data[index]);
    break;
  }
  case RKNN3_TENSOR_UINT8: {
    uint8_t* uint8_data = (uint8_t*)data;
    printf("[%d] UINT8: %u\n", index, uint8_data[index]);
    break;
  }
  case RKNN3_TENSOR_UINT16: {
    uint16_t* uint16_data = (uint16_t*)data;
    printf("[%d] UINT16: %u\n", index, uint16_data[index]);
    break;
  }
  case RKNN3_TENSOR_UINT32: {
    uint32_t* uint32_data = (uint32_t*)data;
    printf("[%d] UINT32: %u\n", index, uint32_data[index]);
    break;
  }
  default:
    printf("[%d] Unknown data type\n", index);
    break;
  }
}

/**
 * @brief 打印数据前N个值
 *
 * @param data 数据指针
 * @param type 数据类型
 * @param size 数据大小（字节）
 * @param count 打印数量
 */
static void print_data_values(void* data, rknn3_tensor_type type, size_t size, size_t count)
{
  size_t element_size = 0;
  switch (type) {
  case RKNN3_TENSOR_FLOAT16:
    element_size = sizeof(float16);
    break;
  case RKNN3_TENSOR_FLOAT32:
    element_size = sizeof(float);
    break;
  case RKNN3_TENSOR_INT8:
  case RKNN3_TENSOR_UINT8:
    element_size = sizeof(int8_t);
    break;
  case RKNN3_TENSOR_INT16:
  case RKNN3_TENSOR_UINT16:
    element_size = sizeof(int16_t);
    break;
  case RKNN3_TENSOR_INT32:
  case RKNN3_TENSOR_UINT32:
    element_size = sizeof(int32_t);
    break;
  case RKNN3_TENSOR_INT64:
  case RKNN3_TENSOR_UINT64:
    element_size = sizeof(int64_t);
    break;
  default:
    element_size = sizeof(float16);
    break;
  }

  size_t max_elements = size / element_size;
  size_t print_count  = (count < max_elements) ? count : max_elements;

  for (size_t i = 0; i < print_count; i++) {
    print_data_value(data, type, i);
  }
}

static int get_core_num(uint32_t core_mask)
{
  int core_num = 0;
  for (int i = 0; i < 32; i++) {
    if (core_mask & (1 << i)) {
      core_num++;
    }
  }
  return core_num;
}

// 添加计算余弦相似度的函数
static float cosine_similarity(const float* a, const float* b, size_t size)
{
  float dot_product = 0.0f;
  float norm_a      = 0.0f;
  float norm_b      = 0.0f;
  for (size_t i = 0; i < size; i++) {
    float val_a = a[i];
    float val_b = b[i];

    if (!isfinite(val_a) || !isfinite(val_b))
      return 0.0f;

    dot_product += val_a * val_b;
    norm_a += val_a * val_a;
    norm_b += val_b * val_b;
  }

  if (norm_a < FLT_EPSILON || norm_b < FLT_EPSILON) {
    return 0.0f;
  }

  float norm_product = sqrtf(norm_a) * sqrtf(norm_b);

  if (norm_product < FLT_EPSILON) {
    return 0.0f;
  }

  float similarity = dot_product / norm_product;

  // 限制在 [-1.0, 1.0] 范围内
  if (similarity > 1.0f)
    similarity = 1.0f;
  if (similarity < -1.0f)
    similarity = -1.0f;

  return similarity;
}

static int convert_fp32_to_any_type(const float* src, void* dst, int n_elems, rknn3_tensor_type type)
{
  printf("convert_fp32_to_any_type\n");
  printf("n_elems=%d, type=%s\n", n_elems, rknn3_get_type_string(type));

  switch (type) {
  case RKNN3_TENSOR_FLOAT16:
    for (int i = 0; i < n_elems; i++) {
      ((float16*)dst)[i] = fp32_to_fp16(src[i]);
    }
    break;
  case RKNN3_TENSOR_FLOAT32:
    memcpy(dst, src, n_elems * sizeof(float));
    break;
  case RKNN3_TENSOR_INT64:
    for (int i = 0; i < n_elems; i++) {
      ((int64_t*)dst)[i] = (int64_t)src[i];
    }
    break;
  case RKNN3_TENSOR_INT32:
    for (int i = 0; i < n_elems; i++) {
      ((int32_t*)dst)[i] = (int32_t)src[i];
    }
    break;
  case RKNN3_TENSOR_INT16:
    for (int i = 0; i < n_elems; i++) {
      ((int16_t*)dst)[i] = (int16_t)src[i];
    }
    break;
  case RKNN3_TENSOR_INT8:
    for (int i = 0; i < n_elems; i++) {
      ((int8_t*)dst)[i] = (int8_t)src[i];
    }
    break;
  case RKNN3_TENSOR_UINT8:
    for (int i = 0; i < n_elems; i++) {
      ((uint8_t*)dst)[i] = (uint8_t)src[i];
    }
    break;
  default:
    printf("Unsupported tensor type: %d\n", type);
    return -1;
  }
  return 0;
}

static int convert_fp16_to_fp32(const float16* src, float* dst, int n_elems)
{
  for (int i = 0; i < n_elems; i++) {
    dst[i] = fp16_to_fp32(src[i]);
  }
  return 0;
}

static int convert_any_type_to_fp32(const void* src, float* dst, int n_elems, rknn3_tensor_type type)
{
  printf("convert_any_type_to_fp32\n");
  printf("n_elems=%d, type=%s\n", n_elems, rknn3_get_type_string(type));

  switch (type) {
  case RKNN3_TENSOR_FLOAT16:
    convert_fp16_to_fp32((const float16*)src, dst, n_elems);
    break;
  case RKNN3_TENSOR_FLOAT32:
    memcpy(dst, src, n_elems * sizeof(float));
    break;
  case RKNN3_TENSOR_INT64:
    for (int i = 0; i < n_elems; i++) {
      dst[i] = (float)((int64_t*)src)[i];
    }
    break;
  case RKNN3_TENSOR_INT32:
    for (int i = 0; i < n_elems; i++) {
      dst[i] = (float)((int32_t*)src)[i];
    }
    break;
  case RKNN3_TENSOR_INT16:
    for (int i = 0; i < n_elems; i++) {
      dst[i] = (float)((int16_t*)src)[i];
    }
    break;
  case RKNN3_TENSOR_INT8:
    for (int i = 0; i < n_elems; i++) {
      dst[i] = (float)((int8_t*)src)[i];
    }
    break;
  case RKNN3_TENSOR_UINT8:
    for (int i = 0; i < n_elems; i++) {
      dst[i] = (float)((uint8_t*)src)[i];
    }
    break;
  default:
    printf("Unsupported tensor type: %d\n", type);
    return -1;
  }
  return 0;
}

static int convert_int8_to_fp32(const void *src, float *dst, int n_elems, rknn3_tensor_type type, float scale, int32_t zero_point)
{
  printf("convert_int8_to_fp32\n");
  printf("n_elems=%d, type=%s\n", n_elems, rknn3_get_type_string(type));
  printf("scale=%f, zero_point=%d\n", scale, zero_point);

  for (int i = 0; i < n_elems; i++)
  {
    dst[i] = ((float)((int8_t *)src)[i] - zero_point) * scale;
  }

  return 0;
}

static int NCHW_fp32_to_NC1HWC2_fp16(const float* src, float16* dst, int batch, int h, int w, int channel, int sub_c, int align_stride,
                                     int align_hw)
{
  printf("NCHW_fp32_to_NC1HWC2_fp16\n");
  printf("batch=%d, h=%d, w=%d, channel=%d, sub_c=%d, align_stride=%d, "
             "align_hw=%d\n",
             batch, h, w, channel, sub_c, align_stride, align_hw);
  int hw      = w * h;
  int align_c = (channel + sub_c - 1) / sub_c * sub_c;
  for (int b = 0; b < batch; b++) {
    const float* src_b = src + b * channel * hw;
    float16*     dst_b = dst + b * align_c * align_hw;
    for (int c = 0; c < channel; ++c) {
      int      plane    = c / sub_c;
      float16* dstPlane = plane * align_hw * sub_c + dst_b;
      int      offset   = c % sub_c;
      for (int cur_h = 0; cur_h < h; ++cur_h)
        for (int cur_w = 0; cur_w < w; ++cur_w) {
          int cur_hw                        = cur_h * align_stride + cur_w;
          dstPlane[sub_c * cur_hw + offset] = fp32_to_fp16(src_b[c * hw + cur_h * w + cur_w]);
        }
    }
  }

  return 0;
}

static int NCHW_fp32_to_NHWC_fp16(const float* src, float16* dst, int batch, int h, int w, int channel, int w_stride)
{
  printf("NCHW_fp32_to_NHWC_fp16\n");
  printf("batch=%d, h=%d, w=%d, channel=%d, w_stride=%d\n", batch, h, w, channel, w_stride);
  int hw = w * h;
  for (int b = 0; b < batch; b++) {
    const float* src_b = src + b * channel * hw;
    float16*     dst_b = dst + b * hw * channel;
    for (int c = 0; c < channel; ++c) {
      for (int cur_h = 0; cur_h < h; ++cur_h)
        for (int cur_w = 0; cur_w < w_stride; ++cur_w) {
          if (cur_w < w) {
            dst_b[cur_h * w_stride * channel + cur_w * channel + c] = fp32_to_fp16(src_b[c * hw + cur_h * w + cur_w]);
          } else {
            dst_b[cur_h * w_stride * channel + cur_w * channel + c] = fp32_to_fp16(0.0);
          }
        }
    }
  }
  return 0;
}

static int NC1HWC2_fp16_to_NCHW_fp32(const float16* src, float* dst, int batch, int C1, int C2, int hw_src, int channel, int h, int w)
{
  printf("NC1HWC2_fp16_to_NCHW_fp32\n");
  printf("batch=%d, C1=%d, C2=%d, hw_src=%d, channel=%d, h=%d, w=%d\n", batch, C1, C2, hw_src, channel, h, w);
  int hw_dst = h * w;
  for (int i = 0; i < batch; i++) {
    const float16* src_b = src + i * C1 * hw_src * C2;
    float*         dst_b = dst + i * channel * hw_dst;
    for (int c = 0; c < channel; ++c) {
      int            plane  = c / C2;
      const float16* src_bc = plane * hw_src * C2 + src_b;
      int            offset = c % C2;
      for (int cur_h = 0; cur_h < h; ++cur_h)
        for (int cur_w = 0; cur_w < w; ++cur_w) {
          int cur_hw                 = cur_h * w + cur_w;
          dst_b[c * hw_dst + cur_hw] = fp16_to_fp32(src_bc[C2 * cur_hw + offset]); // float16-->float
        }
    }
  }

  return 0;
}

static int convert_uint8_NCHW_to_NHWC(const uint8_t* src, void* dst, int batch, int h, int w, int channel, int w_stride,
                                      rknn3_tensor_type dst_type)
{
  printf("convert_uint8_NCHW_to_NHWC\n");
  printf("batch=%d, h=%d, w=%d, channel=%d, w_stride=%d, dst_type=%s\n", batch, h, w, channel, w_stride, rknn3_get_type_string(dst_type));

  int hw = w * h;
  for (int b = 0; b < batch; b++) {
    const uint8_t* src_b = src + b * channel * hw;

    for (int c = 0; c < channel; ++c) {
      for (int cur_h = 0; cur_h < h; ++cur_h) {
        for (int cur_w = 0; cur_w < w_stride; ++cur_w) {
          int dst_idx = b * h * w_stride * channel + cur_h * w_stride * channel + cur_w * channel + c;

          if (cur_w < w) {
            int src_idx = c * hw + cur_h * w + cur_w;
            switch (dst_type) {
            case RKNN3_TENSOR_FLOAT16:
              ((float16*)dst)[dst_idx] = fp32_to_fp16((float)src_b[src_idx]);
              break;
            case RKNN3_TENSOR_FLOAT32:
              ((float*)dst)[dst_idx] = (float)src_b[src_idx];
              break;
            case RKNN3_TENSOR_UINT8:
              ((uint8_t*)dst)[dst_idx] = src_b[src_idx];
              break;
            case RKNN3_TENSOR_INT8:
              ((int8_t*)dst)[dst_idx] = (int8_t)src_b[src_idx];
              break;
            default:
              printf("Unsupported dst_type in NHWC conversion: %s\n", rknn3_get_type_string(dst_type));
              return -1;
            }
          } else {
            // 填充0
            switch (dst_type) {
            case RKNN3_TENSOR_FLOAT16:
              ((float16*)dst)[dst_idx] = fp32_to_fp16(0.0f);
              break;
            case RKNN3_TENSOR_FLOAT32:
              ((float*)dst)[dst_idx] = 0.0f;
              break;
            case RKNN3_TENSOR_UINT8:
              ((uint8_t*)dst)[dst_idx] = 0;
              break;
            case RKNN3_TENSOR_INT8:
              ((int8_t*)dst)[dst_idx] = 0;
              break;
            default:
              break;
            }
          }
        }
      }
    }
  }
  return 0;
}

static int convert_uint8_NCHW_to_NC1HWC2(const uint8_t* src, void* dst_ptr, int batch, int h, int w, int channel, int align_stride,
                                         int align_hw, rknn3_tensor_attr* attr)
{
  // 仅mean=0, std=1的输入会被gongga-toolkit标记成NC1HWC2,所以这里不处理normalize
  if (attr->dtype == RKNN3_TENSOR_FLOAT16) {
    int      sub_c = RKNPU_B16_SUBC;
    float16* dst   = (float16*)dst_ptr;
    printf("NCHW_uint8_to_NC1HWC2_fp16\n");
    printf("batch=%d, h=%d, w=%d, channel=%d, sub_c=%d, align_stride=%d, "
           "align_hw=%d\n",
           batch, h, w, channel, sub_c, align_stride, align_hw);
    int hw      = w * h;
    int align_c = (channel + sub_c - 1) / sub_c * sub_c;
    for (int b = 0; b < batch; b++) {
      const uint8_t* src_b = src + b * channel * hw;
      float16*       dst_b = dst + b * align_c * align_hw;
      for (int c = 0; c < channel; ++c) {
        int      plane    = c / sub_c;
        float16* dstPlane = plane * align_hw * sub_c + dst_b;
        int      offset   = c % sub_c;
        for (int cur_h = 0; cur_h < h; ++cur_h)
          for (int cur_w = 0; cur_w < w; ++cur_w) {
            int cur_hw                        = cur_h * align_stride + cur_w;
            dstPlane[sub_c * cur_hw + offset] = fp32_to_fp16(((float)src_b[c * hw + cur_h * w + cur_w]));
          }
      }
    }
  } else if (attr->dtype == RKNN3_TENSOR_INT8) {
    int     sub_c      = RKNPU_B8_SUBC;
    int8_t* dst        = (int8_t*)dst_ptr;
    float   scale      = attr->qnt_info.scale;
    int32_t zero_point = attr->qnt_info.zero_point;
    printf("NCHW_uint8_to_NC1HWC2_fp16\n");
    printf("batch=%d, h=%d, w=%d, channel=%d, sub_c=%d, align_stride=%d, "
           "align_hw=%d, scale=%f, zp=%d\n",
           batch, h, w, channel, sub_c, align_stride, align_hw, scale, zero_point);
    int hw      = w * h;
    int align_c = (channel + sub_c - 1) / sub_c * sub_c;
    for (int b = 0; b < batch; b++) {
      const uint8_t* src_b = src + b * channel * hw;
      int8_t*        dst_b = dst + b * align_c * align_hw;
      for (int c = 0; c < channel; ++c) {
        int     plane    = c / sub_c;
        int8_t* dstPlane = plane * align_hw * sub_c + dst_b;
        int     offset   = c % sub_c;
        for (int cur_h = 0; cur_h < h; ++cur_h)
          for (int cur_w = 0; cur_w < w; ++cur_w) {
            int   cur_hw                      = cur_h * align_stride + cur_w;
            float dst_value                   = ((float)src_b[c * hw + cur_h * w + cur_w]) / scale + zero_point;
            dstPlane[sub_c * cur_hw + offset] = (int8_t)dst_value;
          }
      }
    }
  } else {
    printf("Unsupported dst_type: %d\n", attr->dtype);
    return -1;
  }
  return 0;
}

static int convert_fp32_to_int8(const float* src, int8_t* dst, int n_elems, rknn3_tensor_attr* attr) {
  float   scale      = attr->qnt_info.scale;
  int32_t zero_point = attr->qnt_info.zero_point;
  for (int i = 0; i < n_elems; i++) {
    dst[i] = ((float)src[i]) / scale + zero_point;
  }
  return 0;
}


static int convert_fp32_NCHW_to_NC1HWC2(const float* src, void* dst_ptr, int batch, int h, int w, int channel, int align_stride,
                                         int align_hw, rknn3_tensor_attr* attr)
{
  // 仅mean=0, std=1的输入会被gongga-toolkit标记成NC1HWC2,所以这里不处理normalize
  if (attr->dtype == RKNN3_TENSOR_FLOAT16) {
    int      sub_c = RKNPU_B16_SUBC;
    float16* dst   = (float16*)dst_ptr;
    printf("NCHW_fp32_to_NC1HWC2_fp16\n");
    printf("batch=%d, h=%d, w=%d, channel=%d, sub_c=%d, align_stride=%d, "
           "align_hw=%d\n",
           batch, h, w, channel, sub_c, align_stride, align_hw);
    int hw      = w * h;
    int align_c = (channel + sub_c - 1) / sub_c * sub_c;
    for (int b = 0; b < batch; b++) {
      const float* src_b = src + b * channel * hw;
      float16*       dst_b = dst + b * align_c * align_hw;
      for (int c = 0; c < channel; ++c) {
        int      plane    = c / sub_c;
        float16* dstPlane = plane * align_hw * sub_c + dst_b;
        int      offset   = c % sub_c;
        for (int cur_h = 0; cur_h < h; ++cur_h)
          for (int cur_w = 0; cur_w < w; ++cur_w) {
            int cur_hw                        = cur_h * align_stride + cur_w;
            dstPlane[sub_c * cur_hw + offset] = fp32_to_fp16(((float)src_b[c * hw + cur_h * w + cur_w]));
          }
      }
    }
  } else if (attr->dtype == RKNN3_TENSOR_INT8) {
    int     sub_c      = RKNPU_B8_SUBC;
    int8_t* dst        = (int8_t*)dst_ptr;
    float   scale      = attr->qnt_info.scale;
    int32_t zero_point = attr->qnt_info.zero_point;
    printf("NCHW_fp32_to_NC1HWC2_int8\n");
    printf("batch=%d, h=%d, w=%d, channel=%d, sub_c=%d, align_stride=%d, "
           "align_hw=%d, scale=%f, zp=%d\n",
           batch, h, w, channel, sub_c, align_stride, align_hw, scale, zero_point);
    int hw      = w * h;
    int align_c = (channel + sub_c - 1) / sub_c * sub_c;
    for (int b = 0; b < batch; b++) {
      const float* src_b = src + b * channel * hw;
      int8_t*        dst_b = dst + b * align_c * align_hw;
      for (int c = 0; c < channel; ++c) {
        int     plane    = c / sub_c;
        int8_t* dstPlane = plane * align_hw * sub_c + dst_b;
        int     offset   = c % sub_c;
        for (int cur_h = 0; cur_h < h; ++cur_h)
          for (int cur_w = 0; cur_w < w; ++cur_w) {
            int   cur_hw                      = cur_h * align_stride + cur_w;
            float dst_value                   = ((float)src_b[c * hw + cur_h * w + cur_w]) / scale + zero_point;
            dst_value = rint(dst_value);
            dstPlane[sub_c * cur_hw + offset] = (int8_t)dst_value;
          }
      }
    }
  } else {
    printf("Unsupported dst_type: %d\n", attr->dtype);
    return -1;
  }
  return 0;
}

static inline uint32_t shape_count(uint32_t* shape, int n_dims)
{
  uint32_t elems = 1;
  for (int i = 0; i < n_dims; i++) {
    elems *= shape[i];
  }
  return elems;
}

static int NC1HWC2_int8_to_NCHW_fp32(const int8_t* src, float* dst, int batch, int C1, int C2, int hw_src, int channel, int h, int w,
                                     rknn3_tensor_attr* attr)
{
  printf("NC1HWC2_int8_to_NCHW_fp32\n");
  printf("batch=%d, C1=%d, C2=%d, hw_src=%d, channel=%d, h=%d, w=%d\n", batch, C1, C2, hw_src, channel, h, w);

  // 获取量化参数
  float   scale      = 1.0f;
  int32_t zero_point = 0;

  if (attr->qnt_type != RKNN3_TENSOR_QNT_NONE) {
    scale      = attr->qnt_info.scale;
    zero_point = attr->qnt_info.zero_point;
    printf("Quantization params: scale=%f, zero_point=%d\n", scale, zero_point);
  } else {
    printf("No quantization parameters found, using default values\n");
  }

  int hw_dst = h * w;
  for (int i = 0; i < batch; i++) {
    const int8_t* src_b = src + i * C1 * hw_src * C2;
    float*        dst_b = dst + i * channel * hw_dst;
    for (int c = 0; c < channel; ++c) {
      int           plane  = c / C2;
      const int8_t* src_bc = plane * hw_src * C2 + src_b;
      int           offset = c % C2;
      for (int cur_h = 0; cur_h < h; ++cur_h)
        for (int cur_w = 0; cur_w < w; ++cur_w) {
          int    cur_hw          = cur_h * w + cur_w;
          int8_t quantized_value = src_bc[C2 * cur_hw + offset];
          // 反量化：float_value = (int8_value - zero_point) * scale
          dst_b[c * hw_dst + cur_hw] = (float)(quantized_value - zero_point) * scale;
        }
    }
  }

  return 0;
}

int main(int argc, char* argv[])
{
  rknn3_context         ctx;
  int                   ret;
  char*                 token      = NULL;
  uint32_t              core_mask  = 0x0;
  int                   loop_count = DEFAULT_LOOP_COUNT;
  struct timespec       start, end;
  bool use_random_input = false;
  bool skip_golden_comparison = false;
  rknn3_devices devs;

  if (argc < 3) {
    printf("Usage: %s <model_path> <weight_path> <input_npy_paths> <golden_output_npy_paths> <core_mask> [loop_count]\n", argv[0]);
    printf("  - If input_npy_paths is not provided, random input will be generated\n");
    printf("  - If golden_output_npy_paths is not provided, cosine similarity will be skipped\n");
    printf("  - If core_mask is not provided, it will be auto-generated based on core_number\n");
    printf("  - If you want to use both random input and set core_mask or loop_count, you can set both input_npy_paths and golden_output_npy_paths to empty strings.\n");
    return -1;
  }
  const char* model_path   = argv[1];
  const char* weight_path  = argv[2];

  // 检查是否提供了输入和golden输出路径
  const char* input_paths  = (argc > 3) ? argv[3] : NULL;
  const char* golden_paths = (argc > 4) ? argv[4] : NULL;
  if (!input_paths || !golden_paths || strlen(input_paths) == 0 || strlen(golden_paths) == 0) {
    use_random_input = true;
    skip_golden_comparison = true;
    printf("Input paths or golden output paths not provided, using random input and skipping golden comparison\n");
  }

  // 解析输入路径
  std::vector<std::string> input_files;
  if (input_paths) {
    char *input_path = _strdup(input_paths);
    token = strtok(input_path, "#");
    while (token != NULL)
    {
      input_files.push_back(token);
      token = strtok(NULL, "#");
    }
    free(input_path);
  }

  // 解析golden输出路径
  std::vector<std::string> golden_files;
  if (golden_paths) {
    char* golden_path = _strdup(golden_paths);
    token = strtok(golden_path, "#");
    while (token != NULL) {
      golden_files.push_back(token);
      token = strtok(NULL, "#");
    }
    free(golden_path);
  }

  // Query available devices
  memset(&devs, 0, sizeof(devs));
  ret = rknn3_find_devices(&devs);
  if (ret != RKNN3_SUCCESS) {
    printf("rknn3_find_devices failed! ret=%d\n", ret);
    return -1;
  }
  printf("Found %d RK182X devices\n", devs.n_devices);
  for (int i = 0; i < devs.n_devices; i++) {
    printf("  Device %d: transfer_type=%s, id=%s\n", i, devs.devices[i].type, devs.devices[i].id);
  }

  // Select the first device
  if (devs.n_devices == 0) {
    printf("No RK182X devices found\n");
    return -1;
  } else if (devs.n_devices == 1) {
    // If only one device found, the init_extend can be NULL
    printf("Info: Only one device found (id=%s), init_extend can be NULL\n", devs.devices[0].id);
  } else {
    printf("Multiple devices found, using the first one (id=%s)\n", devs.devices[0].id);
  }
  rknn3_init_extend init_extend = {0};
  init_extend.device_id = devs.devices[0].id;

  // Init RKNN3 context
  clock_gettime(CLOCK_MONOTONIC, &start);
  ret = rknn3_init(&ctx, &init_extend);
  if (ret != RKNN3_SUCCESS) {
    printf("rknn3_init failed! ret=%d\n", ret);
    return -1;
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("rknn3_init success, cost %.3f ms\n",
         ((end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000.0);

  // Query device memory info
  rknn3_dev_mem_info dev_mem_info;
  memset(&dev_mem_info, 0, sizeof(dev_mem_info));
  ret = rknn3_query(ctx, RKNN3_QUERY_DEVICE_MEM_INFO, &dev_mem_info, sizeof(dev_mem_info));
  if (ret != RKNN3_SUCCESS)
  {
    printf("rknn3_query device mem info failed! ret=%d\n", ret);
    rknn3_destroy(ctx);
    return -1;
  }
  printf("Device Memory Info: total=%zu MB, free=%zu MB\n",
         dev_mem_info.sys_total / (1024 * 1024), dev_mem_info.sys_free / (1024 * 1024));
  for (int i = 0; i < dev_mem_info.node_num; i++)
  {
    printf("  Node %d: total=%zu MB, free=%zu MB\n", i,
           dev_mem_info.node_mem_info[i].total / (1024 * 1024),
           dev_mem_info.node_mem_info[i].free / (1024 * 1024));
  }

  // Load model
  FILE* fp = fopen(model_path, "rb");
  if (fp == NULL) {
    printf("fopen %s fail!\n", model_path);
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  int   model_len = ftell(fp);
  void* model     = malloc(model_len);
  fseek(fp, 0, SEEK_SET);
  if (model_len != fread(model, 1, model_len, fp)) {
    printf("fread %s fail!\n", model_path);
    free(model);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  
  // Load weight
  fp = fopen(weight_path, "rb");
  if (fp == NULL) {
    printf("fopen %s fail!\n", weight_path);
    free(model);
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  size_t weight_len  = ftell(fp);
  void*  weight_data = malloc(weight_len);
  fseek(fp, 0, SEEK_SET);
  if (weight_len != fread(weight_data, 1, weight_len, fp)) {
    printf("fread %s fail!\n", weight_path);
    free(model);
    free(weight_data);
    fclose(fp);
    return -1;
  }
  fclose(fp);

  printf("model_len=%d, model=%p\n", model_len, model);
  printf("weight_len=%zu, weight_data=%p\n", weight_len, weight_data);
  clock_gettime(CLOCK_MONOTONIC, &start);
  ret = rknn3_load_model_from_data(ctx, model, model_len, weight_data, weight_len);
  if (ret != RKNN3_SUCCESS) {
    printf("rknn3_load_model_from_data failed! ret=%d\n", ret);
    rknn3_destroy(ctx);
    return -1;
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("rknn3_load_model_from_data success, cost %.3f ms\n",
         ((end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000.0);

  // check core_mask
  uint32_t core_num = 0;
  ret               = rknn3_query(ctx, RKNN3_QUERY_CORE_NUMBER, &core_num, sizeof(core_num));
  if (ret < 0) {
    printf("rknn3_query core number fail! ret=%d\n", ret);
    if (model) {
      free(model);
    }
    if (weight_data) {
      free(weight_data);
    }
    rknn3_destroy(ctx);
    return -1;
  }
  printf("Core number: %u\n", core_num);
  // 解析或生成core_mask
  if (argc > 5)
  {
    core_mask = strtoul(argv[5], NULL, 16);
    if (get_core_num(core_mask) != core_num)
    {
      printf("Error: core_mask 0x%x does not match core number %u\n", core_mask, core_num);
      if (model)
      {
        free(model);
      }
      if (weight_data)
      {
        free(weight_data);
      }
      rknn3_destroy(ctx);
      return -1;
    }
  }
  else
  {
    // 自动生成core_mask：从0位开始对应core_number个核心
    core_mask = 0;
    for (uint32_t i = 0; i < core_num; i++)
    {
      core_mask |= (1 << i);
    }
    printf("Auto-generated core_mask: 0x%x for %d cores\n", core_mask, core_num);
  }

  rknn3_config config;
  memset(&config, 0, sizeof(config));
  config.run_core_mask = core_mask;
  printf("model_config:%p\n", model_len, model, &config);

  // model init
  clock_gettime(CLOCK_MONOTONIC, &start);
  ret = rknn3_model_init(ctx, &config);
  if (ret != RKNN3_SUCCESS) {
    printf("rknn3_model_init failed! ret=%d\n", ret);
    if (model) {
      free(model);
    }
    if (weight_data) {
      free(weight_data);
    }
    rknn3_destroy(ctx);
    return -1;
  }
  if (model) {
    free(model);
  }
  if (weight_data) {
    free(weight_data);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("rknn3_model_init success, cost %.3f ms\n",
         ((end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000.0);

  // 查询输入输出信息
  rknn3_input_output_num io_num;
  ret = rknn3_query(ctx, RKNN3_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));

  printf("input tensors:\n");
  rknn3_tensor inputs[io_num.n_input];
  memset(inputs, 0, io_num.n_input * sizeof(rknn3_tensor));
  for (uint32_t i = 0; i < io_num.n_input; i++) {
    inputs[i].attr        = (rknn3_tensor_attr*)malloc(sizeof(rknn3_tensor_attr));
    inputs[i].attr->index = i;
    // query info
    ret = rknn3_query(ctx, RKNN3_QUERY_INPUT_ATTR, inputs[i].attr, sizeof(rknn3_tensor_attr));
    if (ret < 0) {
      printf("rknn3_query error! ret=%d\n", ret);
      return -1;
    }
    dump_tensor_attr(inputs[i].attr);

    // Create input tensor memory
    inputs[i].mem = rknn3_create_mem(ctx, inputs[i].attr->aligned_size, inputs[i].attr->core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
  }

  printf("output tensors:\n");
  rknn3_tensor outputs[io_num.n_output];
  memset(outputs, 0, io_num.n_output * sizeof(rknn3_tensor));
  for (uint32_t i = 0; i < io_num.n_output; i++) {
    outputs[i].attr        = (rknn3_tensor_attr*)malloc(sizeof(rknn3_tensor_attr));
    outputs[i].attr->index = i;
    // query info
    ret = rknn3_query(ctx, RKNN3_QUERY_OUTPUT_ATTR, outputs[i].attr, sizeof(rknn3_tensor_attr));
    if (ret != RKNN3_SUCCESS) {
      printf("rknn3_query fail! ret=%d\n", ret);
      return -1;
    }
    dump_tensor_attr(outputs[i].attr);

    outputs[i].mem = rknn3_create_mem(ctx, outputs[i].attr->aligned_size, outputs[i].attr->core_id, RKNN3_FLAG_MEMORY_CACHEABLE);
  }

  // 创建npy_array数组存储输入和输出数据
  std::vector<npy_array> input_arrays(io_num.n_input);
  std::vector<npy_array> golden_arrays(io_num.n_output);

  // Linux环境下读取文件到内存
  std::vector<void*> input_data_ptrs;
  std::vector<void*> golden_data_ptrs;

  if (!use_random_input) {
    for (uint32_t i = 0; i < io_num.n_input; i++)
    {
      FILE *fp = fopen(input_files[i].c_str(), "rb");
      if (!fp)
      {
        printf("Failed to open input numpy file: %s\n", input_files[i].c_str());
        return -1;
      }

      fseek(fp, 0, SEEK_END);
      size_t file_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      void *data = malloc(file_size);
      if (!data)
      {
        printf("Failed to allocate memory for input file %s\n", input_files[i].c_str());
        fclose(fp);
        return -1;
      }

      fread(data, 1, file_size, fp);
      fclose(fp);
      input_data_ptrs.push_back(data);

      if (parse_npy_header_from_mem(data, file_size, &input_arrays[i]) != 0)
      {
        printf("Failed to parse numpy header for input %d\n", i);
        return -1;
      }
    }

    for (uint32_t i = 0; i < io_num.n_output; i++)
    {
      FILE *fp = fopen(golden_files[i].c_str(), "rb");
      if (!fp)
      {
        printf("Failed to open golden numpy file: %s\n", golden_files[i].c_str());
        return -1;
      }

      fseek(fp, 0, SEEK_END);
      size_t file_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      void *data = malloc(file_size);
      if (!data)
      {
        printf("Failed to allocate memory for golden file %s\n", golden_files[i].c_str());
        fclose(fp);
        return -1;
      }

      fread(data, 1, file_size, fp);
      fclose(fp);
      golden_data_ptrs.push_back(data);

      if (parse_npy_header_from_mem(data, file_size, &golden_arrays[i]) != 0)
      {
        printf("Failed to parse numpy header for golden output %d\n", i);
        return -1;
      }
    }
  }

  // 处理输入
  for (uint32_t in_idx = 0; in_idx < io_num.n_input; in_idx++) {
    rknn3_tensor_attr* input_attr = inputs[in_idx].attr;
    rknn3_tensor_mem*  input_mem  = inputs[in_idx].mem;

    uint32_t dst_elems = shape_count(input_attr->shape, input_attr->n_dims);
    if (use_random_input) {
      // 生成随机输入数据
      printf("Generating random input data for input %d (name: %s)\n", in_idx, input_attr->name);

      // 检查特殊输入，如 LLM 的 Th / Tc 等
      bool is_special_tensor = false;
      float special_value = 0.0f;

      if (strcmp(input_attr->name, "Th") == 0)
      {
        is_special_tensor = true;
        special_value = 0.0f;
        printf("Special tensor 'Th' detected, setting all values to 0\n");
      }
      else if (strcmp(input_attr->name, "Tc") == 0)
      {
        is_special_tensor = true;
        special_value = 1.0f;
        printf("Special tensor 'Tc' detected, setting all values to 1\n");
      }
      else if (strcmp(input_attr->name, "Ts") == 0 || strcmp(input_attr->name, "Tsr") == 0)
      {
        is_special_tensor = true;
        special_value = 0.0f;
        printf("Special tensor '%s' detected, setting all values to 0\n", input_attr->name);
      }

      // 根据数据类型生成随机数据或设置特殊值
      switch (input_attr->dtype)
      {
      case RKNN3_TENSOR_FLOAT16:
      {
        float16 *data = (float16 *)input_mem->virt_addr;
        if (is_special_tensor)
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = fp32_to_fp16(special_value);
          }
        }
        else
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            float random_val = (float)rand() / RAND_MAX * 2.0f - 1.0f; // [-1, 1]
            data[i] = fp32_to_fp16(random_val);
          }
        }
        break;
      }
      case RKNN3_TENSOR_INT32:
      {
        int32_t *data = (int32_t *)input_mem->virt_addr;
        if (is_special_tensor)
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (int32_t)special_value;
          }
        }
        else
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (int32_t)(rand() % 2001 - 1000); // [-1000, 1000]
          }
        }
        break;
      }
      case RKNN3_TENSOR_INT8:
      {
        int8_t *data = (int8_t *)input_mem->virt_addr;
        if (is_special_tensor)
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (int8_t)special_value;
          }
        }
        else
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (int8_t)(rand() % 256 - 128); // [-128, 127]
          }
        }
        break;
      }
      case RKNN3_TENSOR_UINT8:
      {
        uint8_t *data = (uint8_t *)input_mem->virt_addr;
        if (is_special_tensor)
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (uint8_t)special_value;
          }
        }
        else
        {
          for (uint32_t i = 0; i < dst_elems; i++)
          {
            data[i] = (uint8_t)(rand() % 256); // [0, 255]
          }
        }
        break;
      }
      default:
        printf("Unsupported tensor type for random generation: %s\n", rknn3_get_type_string(input_attr->dtype));
        break;
      }
    } else {
      size_t src_elems = 1;
      for (size_t i = 0; i < input_arrays[in_idx].ndim; i++)
      {
        src_elems *= input_arrays[in_idx].shape[i];
      }

      // Verify input size
      if (src_elems > dst_elems)
      {
        printf("Input %d size mismatch! Expected %u number of elements, got %lu number of elements\n", in_idx, dst_elems, src_elems);
        return -1;
      }

      int batch = input_arrays[in_idx].ndim > 0 ? input_arrays[in_idx].shape[0] : 1;
      int channel = input_arrays[in_idx].ndim > 1 ? input_arrays[in_idx].shape[1] : 1;
      int h = input_arrays[in_idx].ndim > 2 ? input_arrays[in_idx].shape[2] : 1;
      int w = input_arrays[in_idx].ndim > 3 ? input_arrays[in_idx].shape[3] : 1;
      int w_stride = w; // TODO: 需要修改

      if (input_arrays[in_idx].dtype == CNPY_TYPE_FLOAT32)
      {
        float *input_data = (float *)((char *)input_arrays[in_idx].raw_data + input_arrays[in_idx].data_begin);
        if (input_attr->layout == RKNN3_TENSOR_NC1HWC2)
        {
          if (input_attr->dtype == RKNN3_TENSOR_FLOAT16)
          {
            int sub_c = input_attr->n_dims > 4 ? input_attr->shape[4] : 1;
            int align_stride = w;
            int align_hw = getAlignHW(input_attr->shape[2] * input_attr->shape[3]);
            NCHW_fp32_to_NC1HWC2_fp16(input_data, (float16 *)input_mem->virt_addr, batch, h, w, channel, sub_c, align_stride, align_hw);
          }
          else if (input_attr->dtype == RKNN3_TENSOR_INT8)
          {
            int align_stride = w;
            int align_hw = getAlignHW(input_attr->shape[2] * input_attr->shape[3]);
            convert_fp32_NCHW_to_NC1HWC2(input_data, input_mem->virt_addr, batch, h, w, channel, w, align_hw, input_attr);
          }
          else
          {
            printf("Unsupported type for NC1HWC2 format: %s\n", rknn3_get_type_string(input_attr->dtype));
          }
        }
        else if (input_attr->layout == RKNN3_TENSOR_NHWC)
        {
          if (input_attr->dtype == RKNN3_TENSOR_FLOAT16)
          {
            NCHW_fp32_to_NHWC_fp16(input_data, (float16 *)input_mem->virt_addr, batch, h, w, channel, w_stride);
          }
          else
          {
            printf("Unsupported type for NHWC format: %s\n", rknn3_get_type_string(input_attr->dtype));
          }
        }
        else if (input_attr->layout == RKNN3_TENSOR_NCHW || input_attr->layout == RKNN3_TENSOR_UNDEFINED)
        {
          if (input_attr->dtype == RKNN3_TENSOR_INT8)
          {
            convert_fp32_to_int8(input_data, (int8_t *)input_mem->virt_addr, dst_elems, input_attr);
          }
          else
          {
            convert_fp32_to_any_type(input_data, input_mem->virt_addr, dst_elems, input_attr->dtype);
          }
        }
        else
        {
          printf("Unsupported format: %s\n", rknn3_get_layout_string(input_attr->layout));
        }
      }
      else if (input_arrays[in_idx].dtype == CNPY_TYPE_UINT8)
      {
        uint8_t *input_data = (uint8_t *)((char *)input_arrays[in_idx].raw_data + input_arrays[in_idx].data_begin);

        if (input_attr->layout == RKNN3_TENSOR_NHWC)
        {
          // convert_uint8_NCHW_to_NHWC(input_data, input_mem->virt_addr, batch, h, w, channel, w_stride, input_attr->dtype);
          memcpy(input_mem->virt_addr, input_data, dst_elems * sizeof(uint8_t));
          printf("uint8, nhwc, no need convert.\n");
        }
        else if (input_attr->layout == RKNN3_TENSOR_NC1HWC2)
        {
          int align_stride = w;
          int align_hw = getAlignHW(input_attr->shape[2] * input_attr->shape[3]);
          convert_uint8_NCHW_to_NC1HWC2(input_data, input_mem->virt_addr, batch, h, w, channel, w, align_hw, input_attr);
        }
        else
        {
          printf("Error: uint8 input only supports NHWC layout conversion, got: %s\n", rknn3_get_layout_string(input_attr->layout));
          return -1;
        }
      }
      else if (input_arrays[in_idx].dtype == CNPY_TYPE_INT32 || input_arrays[in_idx].dtype == CNPY_TYPE_UINT32)
      {
        uint8_t *input_data = (uint8_t *)((char *)input_arrays[in_idx].raw_data + input_arrays[in_idx].data_begin);
        memcpy(input_mem->virt_addr, input_data, src_elems * sizeof(int32_t));
      }
      else if (input_arrays[in_idx].dtype == CNPY_TYPE_BOOLEAN)
      {
        uint8_t *input_data = (uint8_t *)((char *)input_arrays[in_idx].raw_data + input_arrays[in_idx].data_begin);
        if (input_attr->layout == RKNN3_TENSOR_UNDEFINED)
        {
          memcpy(input_mem->virt_addr, input_data, dst_elems * sizeof(uint8_t));
        }
        else
        {
          printf("Error: bool input only supports undefine layout conversion, got: %s\n", rknn3_get_layout_string(input_attr->layout));
          return -1;
        }
      }
      else
      {
        printf("Unsupported input numpy data type: %d\n", input_arrays[in_idx].dtype);
        return -1;
      }
    }

    printf("Input %d size check passed: %u elements\n", in_idx, dst_elems);

    // 打印输入数据的前10个值
    printf("Input[%d] first values (type: %d):\n", in_idx, input_attr->dtype);
    print_data_values((void*)input_mem->virt_addr, input_attr->dtype, input_mem->size, 10);
  }

  // 运行模型
  // 解析loop_count，默认为 DEFAULT_LOOP_COUNT
  loop_count = (argc > 6) ? atoi(argv[6]) : DEFAULT_LOOP_COUNT;
  printf("Running model %d times...\n", loop_count);
  for (int loop = 0; loop < loop_count; loop++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    // sync inputs
    for (int i = 0; i < io_num.n_input; i++)
    {
      printf("Syncing input[%d] to device...\n", i);
      ret = rknn3_mem_sync(ctx, inputs[i].mem, RKNN3_MEMORY_SYNC_TO_DEVICE);
      if (ret != RKNN3_SUCCESS)
      {
        printf("rknn3_mem_sync input[%d] failed! ret=%d\n", i, ret);
        break;
      }
    }
    // model inference
    ret = rknn3_run(ctx, inputs, io_num.n_input, outputs, io_num.n_output);
    if (ret != RKNN3_SUCCESS) {
      printf("rknn3_run failed at loop %d! ret=%d\n", loop + 1, ret);
      break;
    }
    // sync outputs
    for (int i = 0; i < io_num.n_output; i++)
    {
      printf("Syncing output[%d] from device...\n", i);
      rknn3_mem_sync(ctx, outputs[i].mem, RKNN3_MEMORY_SYNC_FROM_DEVICE);
      if (ret != RKNN3_SUCCESS)
      {
        printf("rknn3_mem_sync output[%d] failed! ret=%d\n", i, ret);
        break;
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("loop: %d/%d, model inference cost %.3f ms\n", loop + 1, loop_count,
           ((end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000.0);
  }

  if (ret == RKNN3_SUCCESS) {
    printf("All %d loops completed successfully\n", loop_count);
  } else {
    printf("Run failed. Please check the error code: %d\n", ret);
    rknn3_destroy(ctx);
    return -1;
  }

  // 验证输出
  if (!skip_golden_comparison) {
    for (uint32_t i = 0; i < io_num.n_output; i++)
    {
      rknn3_tensor_attr *output_attr = outputs[i].attr;
      rknn3_tensor_mem *output_mem = outputs[i].mem;

      // rt_hw_cpu_dcache_ops(RT_HW_CACHE_INVALIDATE, (void*)output_mem->virt_addr, output_mem->size);

      int batch = golden_arrays[i].ndim > 0 ? golden_arrays[i].shape[0] : 1;
      int channel = golden_arrays[i].ndim > 1 ? golden_arrays[i].shape[1] : 1;
      int h = golden_arrays[i].ndim > 2 ? golden_arrays[i].shape[2] : 1;
      int w = golden_arrays[i].ndim > 3 ? golden_arrays[i].shape[3] : 1;
      int src_elems = shape_count(output_attr->shape, output_attr->n_dims);
      int dst_elems = 1;
      for (uint32_t j = 0; j < golden_arrays[i].ndim; j++)
      {
        dst_elems *= golden_arrays[i].shape[j];
      }

      float *output_data = (float *)malloc(dst_elems * sizeof(float));
      if (output_data == NULL)
      {
        printf("Failed to allocate memory for output_data\n");
      }
      if (output_attr->layout == RKNN3_TENSOR_NC1HWC2)
      {
        if (output_attr->dtype == RKNN3_TENSOR_FLOAT16)
        {
          int C1 = output_attr->shape[1];
          int C2 = output_attr->shape[4];
          int hw = output_attr->shape[2] * output_attr->shape[3];
          NC1HWC2_fp16_to_NCHW_fp32((float16 *)output_mem->virt_addr, output_data, batch, C1, C2, hw, channel, h, w);
        }
        else if (output_attr->dtype == RKNN3_TENSOR_INT8)
        {
          int C1 = output_attr->shape[1];
          int C2 = output_attr->shape[4];
          int hw = output_attr->shape[2] * output_attr->shape[3];
          NC1HWC2_int8_to_NCHW_fp32((int8_t *)output_mem->virt_addr, output_data, batch, C1, C2, hw, channel, h, w, output_attr);
        }
        else
        {
          printf("Unsupported type for NC1HWC2 format: %s\n", rknn3_get_type_string(output_attr->dtype));
        }
      }
      else if (output_attr->layout == RKNN3_TENSOR_NCHW || output_attr->layout == RKNN3_TENSOR_UNDEFINED)
      {
        if (output_attr->dtype == RKNN3_TENSOR_INT8)
        {
          float scale = output_attr->qnt_info.scale;
          int32_t zero_point = output_attr->qnt_info.zero_point;
          convert_int8_to_fp32(output_mem->virt_addr, output_data, dst_elems, output_attr->dtype, scale, zero_point);
        }
        else
        {
          convert_any_type_to_fp32(output_mem->virt_addr, output_data, dst_elems, output_attr->dtype);
        }
      }
      else
      {
        printf("Unsupported output format: %s\n", rknn3_get_layout_string(output_attr->layout));
      }

      size_t *shape = (size_t *)malloc(output_attr->n_dims * sizeof(size_t));
      memset(shape, 0, output_attr->n_dims * sizeof(size_t));
      for (uint32_t j = 0; j < output_attr->n_dims; j++)
      {
        shape[j] = output_attr->shape[j];
      }
      char filename[256];
      snprintf(filename, sizeof(filename), "output_data_%d.npy", i);
      npy_save_float_buffer_to_file(filename, output_data, output_attr->n_elems, shape, output_attr->n_dims);
      if (shape)
      {
        free(shape);
      }

      // 打印output_data和golden数据的前10个值
      printf("\nComparing first 10 values of Output %d:\n", i);
      float *golden_data = (float *)((char *)golden_arrays[i].raw_data + golden_arrays[i].data_begin);
      for (int j = 0; j < 10; j++)
      {
        printf("Index[%d]: Output=%.5f, Golden=%.5f\n", j, output_data[j], golden_data[j]);
      }

      // 打印output_data和golden数据的后10个值
      printf("\nComparing last 10 values of Output %d:\n", i);
      for (int j = dst_elems - 10; j < dst_elems; j++)
      {
        printf("Index[%d]: Output=%.5f, Golden=%.5f\n", j, output_data[j], golden_data[j]);
      }

      printf("output native elems: %d, golden elems: %d\n", src_elems, dst_elems);

      // 计算余弦相似度
      float similarity = cosine_similarity(output_data, golden_data, dst_elems);
      printf("Output %d cosine similarity: %.5f\n", i, similarity);
      free(output_data);
    }
  }

  // 清理资源
  // 释放为numpy文件分配的内存
  for (void* ptr : input_data_ptrs) {
    free(ptr);
  }
  for (void* ptr : golden_data_ptrs) {
    free(ptr);
  }

  // 释放numpy数组的shape内存
  for (uint32_t i = 0; i < io_num.n_input; i++) {
    if (input_arrays[i].shape) {
      free(input_arrays[i].shape);
    }
  }
  for (uint32_t i = 0; i < io_num.n_output; i++) {
    if (golden_arrays[i].shape) {
      free(golden_arrays[i].shape);
    }
  }

  for (uint32_t i = 0; i < io_num.n_input; i++) {
    if (inputs[i].attr) {
      free(inputs[i].attr);
    }
    if (inputs[i].mem) {
      rknn3_destroy_mem(ctx, inputs[i].mem);
    }
  }
  for (uint32_t i = 0; i < io_num.n_output; i++) {
    if (outputs[i].attr) {
      free(outputs[i].attr);
    }
    if (outputs[i].mem) {
      rknn3_destroy_mem(ctx, outputs[i].mem);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &start);
  rknn3_destroy(ctx);
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("rknn3_destroy success, cost %.3f ms\n",
         ((end.tv_sec - start.tv_sec) * 1000 * 1000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000.0);

  return 0;
}
