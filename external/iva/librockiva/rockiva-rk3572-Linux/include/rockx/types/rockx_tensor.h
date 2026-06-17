#ifndef _ROCKX_TENSOR_H_
#define _ROCKX_TENSOR_H_

#include <stddef.h>
#include <stdint.h>

#include "rockx_data_type.h"
#include "rockx_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tensor Format
 *
 */
typedef enum {
    ROCKX_TENSOR_FORMAT_NCHW = 0,   ///< NCHW
    ROCKX_TENSOR_FORMAT_NHWC,       ///< NHWC
    ROCKX_TENSOR_FORMAT_NC1HWC2,    ///< NC1HWC2
    ROCKX_TENSOR_FORMAT_UNDEFINED,  ///< Undefined

    ROCKX_TENSOR_FORMAT_MAX,  ///< Maximum value
} RockXTensorFormat;

/**
 * @brief Tensor quantization type
 *
 */
typedef enum {
    ROCKX_TENSOR_QNT_NONE = 0,           //< none.
    ROCKX_TENSOR_QNT_DFP,                //< dynamic fixed point.
    ROCKX_TENSOR_QNT_AFFINE_ASYMMETRIC,  //< asymmetric affine.
    ROCKX_TENSOR_QNT_MAX,                //< max.
} RockXTensorQntType;

/**
 * @brief Tensor quantization info
 *
 */
typedef struct
{
    RockXTensorQntType qntType;  ///< The quantitative type of tensor. */
    int8_t fl;                   ///< Fractional length for RKNN_TENSOR_QNT_DFP. */
    int32_t zp;                  ///< Zero point for RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC. */
    float scale;                 ///< Scale for RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC. */
} RockXTensorQntInfo;

/**
 * @brief Tensor shape
 */
typedef struct
{
    int dims[16];              ///< Tensor dimensions
    int ndims;                 ///< Tensor dimensions number
    RockXTensorFormat format;  ///< Tensor data arrangement format
} RockXTensorShape;

/**
 * @brief Tensor
 *
 */
typedef struct RockXTensor
{
    RockXTensorShape shape;      ///< Tensor shape
    size_t nelems;               ///< Elements number
    RockXDataType dtype;         ///< Data type
    RockXMemoryBuffer buffer;    ///< Buffer
    RockXTensorQntInfo qntInfo;  ///< Quantization info
} RockXTensor;

/**
 * @brief Tensor format enum to string
 *
 * @param format Tensor format enum
 * @return const char* Tensor format string
 */
static inline const char* RockXTensorFormatToString(RockXTensorFormat format)
{
    switch (format) {
        case ROCKX_TENSOR_FORMAT_NCHW: return "NCHW";
        case ROCKX_TENSOR_FORMAT_NHWC: return "NHWC";
        case ROCKX_TENSOR_FORMAT_NC1HWC2: return "NC1HWC2";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Tensor quantization type enum to string
 *
 * @param type Tensor quantization type enum
 * @return const char* Tensor quantization type string
 */
static inline const char* RockXTensorQntTypeToString(RockXTensorQntType type)
{
    switch (type) {
        case ROCKX_TENSOR_QNT_NONE: return "NONE";
        case ROCKX_TENSOR_QNT_DFP: return "DFP";
        case ROCKX_TENSOR_QNT_AFFINE_ASYMMETRIC: return "AFFINE_ASYMMETRIC";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Read tensor from file
 *
 * @param path File path
 * @param dtype Data type
 * @param out_tensor Output tensor
 * @return int 0: success, -1: failed
 */
int RockXTensorReadFromFile(const char* path, RockXDataType dtype, RockXTensor* out_tensor);

/**
 * @brief Write tensor to file
 *
 * @param path File path
 * @param tensor Tensor
 */
void RockXTensorWrite(const char* path, RockXTensor* tensor);

/**
 * @brief Write tensor to txt file
 *
 * @param path File path
 * @param tensor Tensor
 */
int RockXTensorWriteTxt(const char* path, RockXTensor* tensor);

/**
 * @brief Get tensor elements number
 *
 * @param shape Tensor shape
 * @return size_t Elements number
 */
size_t RockXTensorShapeElemsNum(RockXTensorShape* shape);

/**
 * @brief Get tensor memory size
 *
 * @param tensor Tensor
 * @return size_t Memory size
 */
size_t RockXTensorSize(RockXTensor* tensor);

/**
 * @brief Print tensor information
 *
 * @param tensor Tensor
 */
void RockXTensorDump(RockXTensor* tensor);

/**
 * @brief Get tensor data
 *
 * @param tensor Tensor
 * @param n Batch
 * @param c Channel
 * @param h Height
 * @param w Width
 * @param data Data
 * @return int 0: success, -1: failed
 */
int RockXTensorGetData(RockXTensor* tensor, int n, int c, int h, int w, float* data);

/**
 * @brief Get tensor data
 *
 * @param tensor Tensor
 * @param index Index
 * @param data Data
 * @return int 0: success, -1: failed
 */
int RockXTensorGetData1(RockXTensor* tensor, int index, float* data);

/**
 * @brief Get tensor data
 *
 * @param tensor Tensor
 * @param n Batch
 * @param c Channel
 * @param h Height
 * @param w Width
 * @param data Data
 * @return int 0: success, -1: failed
 */
int RockXTensorGetDataInt8(RockXTensor* tensor, int n, int c, int h, int w, int8_t* data);

#ifdef __cplusplus
}
#endif

#endif