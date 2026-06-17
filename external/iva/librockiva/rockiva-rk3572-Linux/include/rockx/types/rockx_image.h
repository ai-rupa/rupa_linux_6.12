#ifndef _ROCKX_IMAGE_H_
#define _ROCKX_IMAGE_H_

#include "rockx_data_type.h"
#include "rockx_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Image pixel format type
 *
 */
typedef enum {
    ROCKX_PIXEL_FORMAT_UNKNOW = 0,     ///< Unknown binary data
    ROCKX_PIXEL_FORMAT_GRAY8,          ///< Gray8
    ROCKX_PIXEL_FORMAT_GRAY16,         ///< Gray16
    ROCKX_PIXEL_FORMAT_RGB888,         ///< RGB888
    ROCKX_PIXEL_FORMAT_BGR888,         ///< BGR888
    ROCKX_PIXEL_FORMAT_RGBA8888,       ///< RGBA8888
    ROCKX_PIXEL_FORMAT_BGRA8888,       ///< BGRA8888
    ROCKX_PIXEL_FORMAT_YUV420P_YU12,   ///< YUV420P YU12: YYYYYYYYUUVV
    ROCKX_PIXEL_FORMAT_YUV420P_YV12,   ///< YUV420P YV12: YYYYYYYYVVUU
    ROCKX_PIXEL_FORMAT_YUV420SP_NV12,  ///< YUV420SP NV12: YYYYYYYYUVUV
    ROCKX_PIXEL_FORMAT_YUV420SP_NV21,  ///< YUV420SP NV21: YYYYYYYYVUVU
    ROCKX_PIXEL_FORMAT_YUV422P_YU16,   ///< YUV422P YU16: YYYYYYYYUUUUVVVV
    ROCKX_PIXEL_FORMAT_YUV422P_YV16,   ///< YUV422P YV16: YYYYYYYYVVVVUUUU
    ROCKX_PIXEL_FORMAT_YUV422SP_NV16,  ///< YUV422SP NV16: YYYYYYYYUVUVUVUV
    ROCKX_PIXEL_FORMAT_YUV422SP_NV61,  ///< YUV422SP NV61: YYYYYYYYVUVUVUVU
    ROCKX_PIXEL_FORMAT_YUV422_YUYV,    ///< YUV422 YUYV: YUYVYUYV
    ROCKX_PIXEL_FORMAT_YUV422_YVYU,    ///< YUV422 YVYU: YVYUYVYU
    ROCKX_PIXEL_FORMAT_JPEG,           ///< JPEG
    ROCKX_PIXEL_FORMAT_MAX,
} RockXPixelFormat;

/**
 * @brief Image transform type
 *
 */
typedef enum {
    ROCKX_IMAGE_TRANSFORM_NONE = 0x00,        //< Normal
    ROCKX_IMAGE_TRANSFORM_FLIP_H = 0x01,      //< Horizontal flip
    ROCKX_IMAGE_TRANSFORM_FLIP_V = 0x02,      //< Vertical flip
    ROCKX_IMAGE_TRANSFORM_ROTATE_90 = 0x04,   //< Clockwise 90 degrees
    ROCKX_IMAGE_TRANSFORM_ROTATE_180 = 0x03,  //< Clockwise 180 degrees
    ROCKX_IMAGE_TRANSFORM_ROTATE_270 = 0x07,  //< Clockwise 270 degrees
    ROCKX_IMAGE_TRANSFORM_TRANSPOSE = 0x05,   //< Transpose
} RockXImageTransform;

/**
 * @brief Image
 *
 */
typedef struct RockXImage
{
    int width;                 ///< Image width
    int height;                ///< Image height
    RockXPixelFormat format;   ///< Image pixel format
    RockXMemoryBuffer buffer;  ///< Image buffer
} RockXImage;

/**
 * @brief Pixel format enum to string
 *
 * @param format Pixel format enum
 * @return const char* Pixel format string
 */
static inline const char* RockXPixelFormatToString(RockXPixelFormat format)
{
    switch (format) {
        case ROCKX_PIXEL_FORMAT_UNKNOW: return "UNKNOW";
        case ROCKX_PIXEL_FORMAT_GRAY8: return "GRAY8";
        case ROCKX_PIXEL_FORMAT_GRAY16: return "GRAY16";
        case ROCKX_PIXEL_FORMAT_RGB888: return "RGB888";
        case ROCKX_PIXEL_FORMAT_BGR888: return "BGR888";
        case ROCKX_PIXEL_FORMAT_RGBA8888: return "RGBA8888";
        case ROCKX_PIXEL_FORMAT_BGRA8888: return "BGRA8888";
        case ROCKX_PIXEL_FORMAT_YUV420P_YU12: return "YUV420P_YU12";
        case ROCKX_PIXEL_FORMAT_YUV420P_YV12: return "YUV420P_YV12";
        case ROCKX_PIXEL_FORMAT_YUV420SP_NV12: return "YUV420SP_NV12";
        case ROCKX_PIXEL_FORMAT_YUV420SP_NV21: return "YUV420SP_NV21";
        case ROCKX_PIXEL_FORMAT_YUV422P_YU16: return "YUV422P_YU16";
        case ROCKX_PIXEL_FORMAT_YUV422P_YV16: return "YUV422P_YV16";
        case ROCKX_PIXEL_FORMAT_YUV422SP_NV16: return "YUV422SP_NV16";
        case ROCKX_PIXEL_FORMAT_YUV422SP_NV61: return "YUV422SP_NV61";
        case ROCKX_PIXEL_FORMAT_YUV422_YUYV: return "YUV422_YUYV";
        case ROCKX_PIXEL_FORMAT_YUV422_YVYU: return "YUV422_YVYU";
        case ROCKX_PIXEL_FORMAT_JPEG: return "JPEG";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Image transform enum to string
 *
 * @param transform Image transform enum
 * @return const char* Image transform string
 */
static inline const char* RockXImageTransformToString(RockXImageTransform transform)
{
    switch (transform) {
        case ROCKX_IMAGE_TRANSFORM_NONE: return "NONE";
        case ROCKX_IMAGE_TRANSFORM_FLIP_H: return "FLIP_H";
        case ROCKX_IMAGE_TRANSFORM_FLIP_V: return "FLIP_V";
        case ROCKX_IMAGE_TRANSFORM_ROTATE_90: return "ROTATE_90";
        case ROCKX_IMAGE_TRANSFORM_ROTATE_180: return "ROTATE_180";
        case ROCKX_IMAGE_TRANSFORM_ROTATE_270: return "ROTATE_270";
        case ROCKX_IMAGE_TRANSFORM_TRANSPOSE: return "TRANSPOSE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get image buffer memory size based on image width, height and pixel format information
 *
 * @param image Image
 * @return size_t Memory size
 */
size_t RockXImageSize(RockXImage* image);

/**
 * @brief Allocate image buffer memory based on image width, height and pixel format information
 *
 * @param image Image
 * @param type Memory type
 * @return int 0: success, -1: failed
 */
int RockXImageAllocBuffer(RockXImage* image, RockXMemType type);

/**
 * @brief Free image buffer memory
 *
 * @param image Image
 * @return int 0: success, -1: failed
 */
int RockXImageFreeBuffer(RockXImage* image);

/**
 * @brief Synchronize image buffer memory, sync cache to memory
 *
 * @param image Image
 * @return int 0: success, -1: failed
 */
int RockXImageSyncBuffer(RockXImage* image);

/**
 * @brief Write image raw data to file
 *
 * @param image Image
 * @param path Path
 * @return int 0: success, -1: failed
 */
int RockXImageWriteRaw(RockXImage* image, const char* path);

#ifdef __cplusplus
}
#endif

#endif