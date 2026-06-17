#ifndef __RK_ALGO_AVS_TOOL_COMM_H__
#define __RK_ALGO_AVS_TOOL_COMM_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define RKALGO_AVS_CALIB_FILE_LENGTH  10240
#define RKALGO_AVS_MAX_CAMERA_NUM 8
#define RKALGO_AVS_SRC_BLK_NUM_W 15
#define RKALGO_AVS_SRC_BLK_NUM_H 15

    /*
    * @enum    RKALGO_AVS_STATUS_E
    * @brief   the return status of function
    * RKALGO_AVS_STATUS_EOF              : error of function inside
    * RKALGO_AVS_STATUS_OK               : run successfully
    * RKALGO_AVS_STATUS_FILE_READ_ERROR  : error: fail to read file
    * RKALGO_AVS_STATUS_FILE_WRITE_ERROR : error: fail to write file
    * RKALGO_AVS_STATUS_INVALID_PARAM    : error: invalid parameter
    * RKALGO_AVS_STATUS_ALLOC_FAILED     : error: fail to alloc buffer
    * RKALGO_AVS_STATUS_BUTT             : reserved fields
    */
    typedef enum
    {
        RKALGO_AVS_STATUS_EOF = -1,
        RKALGO_AVS_STATUS_OK = 0,
        RKALGO_AVS_STATUS_FILE_READ_ERROR,
        RKALGO_AVS_STATUS_FILE_WRITE_ERROR,
        RKALGO_AVS_STATUS_INVALID_PARAM,
        RKALGO_AVS_STATUS_ALLOC_FAILED,
        RKALGO_AVS_STATUS_NOT_READY,
        RKALGO_AVS_STATUS_BUTT
    }RKALGO_AVS_STATUS_E;

    /*
    * @enum    RKALGO_AVS_BOOL
    * @brief   rk bool enum define
    */
    typedef enum
    {
        RKALGO_AVS_FALSE = 0,
        RKALGO_AVS_TRUE = 1,
    }RKALGO_AVS_BOOL;

    /*
    * @enum    RKALGO_AVS_FUSE_WIDTH_E
    * @brief   Specify the low frequency layer fuse width
    * RKALGO_AVS_FUSE_WIDTH_128 : fuse_width = 128
    * RKALGO_AVS_FUSE_WIDTH_256 : fuse_width = 256
    * RKALGO_AVS_FUSE_WIDTH_512 : fuse_width = 512
    */
    typedef enum
    {
        RKALGO_AVS_FUSE_WIDTH_128 = 128,
        RKALGO_AVS_FUSE_WIDTH_256 = 256,
        RKALGO_AVS_FUSE_WIDTH_512 = 512,
    }RKALGO_AVS_FUSE_WIDTH_E;

    /*
    * @enum    RKALGO_AVS_IMG_FORMAT_E
    * @brief   Specify the images format
    */
    typedef enum
    {
        RKALGO_AVS_IMG_FORMAT_GRAY = 0,
        RKALGO_AVS_IMG_FORMAT_RGB = 1,
        RKALGO_AVS_IMG_FORMAT_NV12 = 2,
        RKALGO_AVS_IMG_FORMAT_BUTT
    }RKALGO_AVS_IMG_FORMAT_E;

    /*
    * @enum    RKALGO_AVS_FINAL_LUT_TYPE_E
    * @brief   Specify the type of final LUT
    * RKALGO_AVS_FINAL_LUT_GPU       : final LUT for gpu(3588)
    * RKALGO_AVS_FINAL_LUT_CMODEL    : final LUT for cmodel
    * RKALGO_AVS_FINAL_LUT_DEBUG     : final LUT for debug
    * RKALGO_AVS_FINAL_LUT_FEC       : final LUT for FEC
    * RKALGO_AVS_FINAL_LUT_AFT       : final LUT for AFT
    * RKALGO_AVS_FINAL_LUT_BUTT      : reserved fields
    */
    typedef enum
    {
        RKALGO_AVS_FINAL_LUT_GPU = 0,
        RKALGO_AVS_FINAL_LUT_CMODEL = 1,
        RKALGO_AVS_FINAL_LUT_DEBUG = 2,
        RKALGO_AVS_FINAL_LUT_FEC = 3,
        RKALGO_AVS_FINAL_LUT_AFT = 4,
        RKALGO_AVS_FINAL_LUT_GPU_ALPHA_U16 = 5,
        RKALGO_AVS_FINAL_LUT_GPU_ALPHA_U8 = 6,
        RKALGO_AVS_FINAL_LUT_GPU_ALPHA_DYNAMIC_U8 = 7,
        RKALGO_AVS_FINAL_LUT_BUTT
    }RKALGO_AVS_FINAL_LUT_TYPE_E;

    /*
    * @enum    RKALGO_AVS_MASK_SHAPE_E
    * @brief   Specify the type of mask
    * RKALGO_AVS_MASK_SHAPE_RECT     : build a rectangle shape mask
    * RKALGO_AVS_MASK_SHAPE_ELLIPSE  : build a ecllipse shape mask, including circl shape
    * RKALGO_AVS_MASK_SHAPE_BUTT     : reserved
    */
    typedef enum
    {
        RKALGO_AVS_MASK_SHAPE_RECT = 0,
        RKALGO_AVS_MASK_SHAPE_ELLIPSE = 1,
        RKALGO_AVS_MASK_SHAPE_BUTT
    }RKALGO_AVS_MASK_SHAPE_E;

    /*
    * @enum    RKALGO_AVS_MIDDLE_LUT_TYPE_E
    * @brief   Specify the type of middle LUT
    * RKALGO_AVS_MIDDLE_LUT_TYPE_A       : RK middle LUT type
    * RKALGO_AVS_MIDDLE_LUT_TYPE_BUTT    : reserved
    */
    typedef enum
    {
        RKALGO_AVS_MIDDLE_LUT_TYPE_A = 0,
        RKALGO_AVS_MIDDLE_LUT_TYPE_BUTT
    }RKALGO_AVS_MIDDLE_LUT_TYPE_E;

    /*
    * @enum    RKALGO_AVS_CAMERA_TYPE_E
    * @brief   Specify the camera type
    * RKALGO_AVS_CAMERA_TYPE_PINHOLE : pinhole
    * RKALGO_AVS_CAMERA_TYPE_OMNI    : omnidirectional
    * RKALGO_AVS_CAMERA_TYPE_FISH    : fisheye
    */
    typedef enum
    {
        RKALGO_AVS_CAMERA_TYPE_PINHOLE,
        RKALGO_AVS_CAMERA_TYPE_OMNI,
        RKALGO_AVS_CAMERA_TYPE_FISH,
    }RKALGO_AVS_CAMERA_TYPE_E;

    /*
    * @enum    RKALGO_AVS_PROJECT_TYPE_E
    * @brief   Specify the project type
    * RKALGO_AVS_PROJECT_EQUIRECTANGULAR         : equirectangular
    * RKALGO_AVS_PROJECT_RECTILINEAR             : rectilinear
    * RKALGO_AVS_PROJECT_CYLINDRICAL             : cylindrical
    * RKALGO_AVS_PROJECT_CUBE_MAP                : cube map
    * RKALGO_AVS_PROJECT_EQUIRECTANGULAR_TRANS   : transverse equirectangular
    * RKALGO_AVS_PROJECT_CYLINDRICAL_TRANS       : transverse cylindrical
    * RKALGO_AVS_PROJECT_PANINI                  : panini
    * RKALGO_AVS_PROJECT_PANINI_TRANS            : transverse panini
    * RKALGO_AVS_PROJECT_BUTT                    : reserved
    */
    typedef enum
    {
        RKALGO_AVS_PROJECT_EQUIRECTANGULAR = 0,
        RKALGO_AVS_PROJECT_RECTILINEAR = 1,
        RKALGO_AVS_PROJECT_CYLINDRICAL = 2,
        RKALGO_AVS_PROJECT_CUBE_MAP = 3,
        RKALGO_AVS_PROJECT_EQUIRECTANGULAR_TRANS = 4,
        RKALGO_AVS_PROJECT_CYLINDRICAL_TRANS = 5,
        RKALGO_AVS_PROJECT_PANINI = 6,
        RKALGO_AVS_PROJECT_PANINI_TRANS = 7,
        RKALGO_AVS_PROJECT_BUTT
    }RKALGO_AVS_PROJECT_TYPE_E;

    /*
    * @enum    RKALGO_AVS_QUERY_MODE_E
    * @brief   Specify the coordinate query mode
    * RKALGO_AVS_DST_QUERY_SRC   : query mode, dst img coordinates ---> src img coordinates
    * RKALGO_AVS_SRC_QUERY_DST   : query mode, src img coordinates ---> dst img coordinates
    * RKALGO_AVS_QUERY_MODE_BUTT : reserved
    */
    typedef enum
    {
        RKALGO_AVS_DST_QUERY_SRC = 0,
        RKALGO_AVS_SRC_QUERY_DST = 1,
        RKALGO_AVS_QUERY_MODE_BUTT
    }RKALGO_AVS_QUERY_MODE_E;

    /*
    * @enum    RKALGO_AVS_ROTATE_E
    * @brief   the clockwise rotation from orginal image to stitch image
    * RKALGO_AVS_ROTATE_0    : clockwise rotate 0 degrees
    * RKALGO_AVS_ROTATE_90   : clockwise rotate 90 degrees
    * RKALGO_AVS_ROTATE_180  : clockwise rotate 180 degrees
    * RKALGO_AVS_ROTATE_270  : clockwise rotate 270 degrees
    */
    typedef enum
    {
        RKALGO_AVS_ROTATE_0 = 0,
        RKALGO_AVS_ROTATE_90 = 1,
        RKALGO_AVS_ROTATE_180 = 2,
        RKALGO_AVS_ROTATE_270 = 3,
    }RKALGO_AVS_ROTATE_E;

    /*
    * @struct  RKALGO_AVS_POINT_S
    * @brief   struct for point
    */
    typedef struct rkAlgoAVS_POINT_S
    {
        int32_t X;
        int32_t Y;
    }RKALGO_AVS_POINT_S;

    /*
    * @struct  RKALGO_AVS_MIDDLE_LUT_BUFFER_S
    * @brief   the structure of output buffer for generating middle LUT from calib file
    * [I]middleLutType              : the type of middle LUT
    * [I]middleLutSize              : the size of middle LUT buffer
    * [O]pMiddleLutBuf              : the middle LUT buffer
    */
    typedef struct rkAlgoAVS_MIDDLE_LUT_BUFFER_S
    {
        RKALGO_AVS_MIDDLE_LUT_TYPE_E middleLutType;
        uint32_t middleLutSize;
        float *pMiddleLutBuf[RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_MIDDLE_LUT_BUFFER_S;

    /*
    * @struct  RKALGO_AVS_FINAL_LUT_PARAMS_S
    * @brief   parameters for final LUT
    */
    typedef struct rkAlgoAVS_FINAL_LUT_PARAMS_S
    {
        uint32_t nonoverlap_num;
        uint32_t overlap_num;
        uint32_t nonoverlap_dst_startx[2];
        uint32_t nonoverlap_dst_width[2];
        uint32_t nonoverlap_mesh_width[2];
        uint32_t nonoverlap_mesh_height[2];

        uint32_t overlap_band_num;
        uint32_t overlap_dst_startx[2];
        uint32_t overlap_dst_width[2];
        uint32_t overlap_mesh_width[2];
        uint32_t overlap_mesh_height[2];
        uint32_t overlap_alpha_width[2];
        uint32_t overlap_alpha_height[2];
    }RKALGO_AVS_FINAL_LUT_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_FINAL_LUT_BUFFER_S
    * @brief   input/output buffer for final LUT
    */
    typedef struct rkAlgoAVS_FINAL_LUT_BUFFER_S
    {
        uint32_t dstW_max;
        uint32_t dstH_max;
        uint32_t src_width;
        uint32_t src_height;
        uint32_t dst_width;
        uint32_t dst_height;
        uint32_t mesh_step_x;
        uint32_t mesh_step_y;
        uint32_t camera_num;
        uint32_t mesh_size;
        uint32_t alpha_size;
        float* pMeshBuf;
        float* pAlphaBuf;
        RKALGO_AVS_FINAL_LUT_PARAMS_S mesh_params[RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_FINAL_LUT_BUFFER_S;

    /*
    * @struct  RKALGO_AVS_MASK_DEFINE_S
    * @brief   definition of mask and corresponding parameters
    * mask_shape        : The type of mask shape
    * offset_x          : The offset of center in x direction
    * offset_y          : The offset of center in y direction
    * half_major_axis   : The half of long axis in X direction
    * half_minor_axis   : The half of short axis in Y direction
    */
    typedef struct rkAlgoAVS_MASK_DEFINE_S
    {
        RKALGO_AVS_MASK_SHAPE_E mask_shape;
        int32_t offset_x;
        int32_t offset_y;
        uint32_t half_major_axis;
        uint32_t half_minor_axis;
    }RKALGO_AVS_MASK_DEFINE_S;

    /*
    * @struct  RKALGO_AVS_MASK_CONFIG_S
    * @brief   Specify of each mask input.
    * bSameMask     : If RKALGO_AVS_TRUE, all the mask using mask[0] parameters, otherwise using eack mask parameters.
    * bInputYuvMask : If RKALGO_AVS_TRUE, using the yuv400 mask directly; else creat a new yuv400 by using maskDefine parameters.
    * mask_width    : width of mask
    * mask_height   : height of mask
    * maskDefine    : Mask define for each camera, Supports up to 8 cameras.
    * maskAddr      : memory address for saving each mask, the memory size for each one should be sizeof(short)*Width*Height, which Width and Height are the input resolution.
    */
    typedef struct rkAlgoAVS_MASK_CONFIG_S
    {
        RKALGO_AVS_BOOL bSameMask;
        RKALGO_AVS_BOOL bInputYuvMask;
        uint32_t mask_width;
        uint32_t mask_height;
        RKALGO_AVS_MASK_DEFINE_S maskDefine[RKALGO_AVS_MAX_CAMERA_NUM];
        int16_t *maskAddr[RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_MASK_CONFIG_S;

    /*
    * @struct  RKALGO_AVS_ROTATION_S
    * @brief   rotation angle
    * yaw100    : Euler angle of yaw, unit: 0.01 degree
    * pitch100  : Euler angle of pitch, unit: 0.01 degree
    * roll100   : Euler angle of roll, unit: 0.01 degree
    */
    typedef struct rkAlgoAVS_ROTATION_S
    {
        int32_t yaw100;
        int32_t pitch100;
        int32_t roll100;
    }RKALGO_AVS_ROTATION_S;

    /*
    * @struct  RKALGO_AVS_FT_PARAMS_SINGLE_S
    * @brief   fine tuning params for single camera
    */
    typedef struct rkAlgoAVS_FT_PARAMS_SINGLE_S
    {
        RKALGO_AVS_BOOL fine_tuning_en;
        int32_t offset_w;
        int32_t offset_h;
        RKALGO_AVS_ROTATION_S rotation;
    }RKALGO_AVS_FT_PARAMS_SINGLE_S;

    /*
    * @struct  RKALGO_AVS_FINE_TUNING_PARAMS_S
    * @brief   fine tuning params for all camera
    */
    typedef struct rkAlgoAVS_FINE_TUNING_PARAMS_S
    {
        uint32_t camera_num;
        RKALGO_AVS_BOOL fine_tuning_en;
        RKALGO_AVS_FT_PARAMS_SINGLE_S fine_tuning_params[RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_FINE_TUNING_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_CAMERA_STATUS_S
    * @brief   definitation for the status of camera
    * camOrder  : camera order from left to right in stitch image
    * rotate    : the clockwise rotation from orginal image to stitch image
    */
    typedef struct rkAlgoAVS_CAMERA_STATUS_S
    {
        uint32_t camOrder;
        RKALGO_AVS_ROTATE_E rotate;

    }RKALGO_AVS_CAMERA_STATUS_S;

    /*
    * @enum    RKALGO_AVS_GROUND_MODEL_OPTIMIZE_OPTION
    * @brief   camera optimization parameter options
    * RKALGO_AVS_GROUNDMODEL_OPT_CAMERA_MODEL           : optimize camera extrinsic parameters and intrinsic parameters
    * RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_POSE            : fixed camera extrinsic parameters
    * RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_MATRIX          : fixed camera intrinsic parameters
    * RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_DISTORTION      : fixed camera distortion
    */
    typedef enum
    {
        RKALGO_AVS_GROUNDMODEL_OPT_CAMERA_MODEL = 1,
        RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_POSE = 2,
        RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_MATRIX = 4,
        RKALGO_AVS_GROUNDMODEL_FIX_CAMERA_DISTORTION = 8,
        RKALGO_AVS_GROUNDMODEL_FIX_BUTT
    }RKALGO_AVS_GROUND_MODEL_OPTIMIZE_OPTION_E;

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif // !__RK_ALGO_AVS_TOOL_COMM_H__