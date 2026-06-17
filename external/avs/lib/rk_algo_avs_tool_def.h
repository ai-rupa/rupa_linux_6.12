#ifndef __RK_ALGO_AVS_TOOL_DEF_H__
#define __RK_ALGO_AVS_TOOL_DEF_H__

#include "rk_algo_avs_tool_comm.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


    /*
    * @struct  RKALGO_AVS_CALIB_PARAMS_S
    * @brief   the structure of params for reading calib file
    * [I]calibFilePath              : the path of calib file
    * [I]cameraNum                  : the number of camera
    */
    typedef struct rkAlgoAVS_CALIB_PARAMS_S
    {
        /* input params */
        const char *calibFilePath;
        uint32_t cameraNum;
    }RKALGO_AVS_CALIB_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_CALIB_BUFFER_S
    * @brief   the structure of output buffer for reading calib file
    * [O]pCalibParamsBuf            : the buffer of calib file params
    */
    typedef struct rkAlgoAVS_CALIB_BUFFER_S
    {
        /* output buffer */
        uint8_t *pCalibParamsBuf;
    }RKALGO_AVS_CALIB_BUFFER_S;

    /*
    * @struct  RKALGO_AVS_MIDDLE_LUT_PARAMS_S
    * @brief   the structure of params for generating middle LUT from calib file
    * [I]calibBuf                   : the buffer of calib file params
    * [I]cameraNum                  : the number of camera
    * [I]stitchDistance             : the best stitch distance
    * [I]inputMaskConfig            : the config of input mask
    * [I]fineTuningParams           : the params of fine tuning
    */
    typedef struct rkAlgoAVS_MIDDLE_LUT_PARAMS_S
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        uint32_t cameraNum;
        float stitchDistance;
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        RKALGO_AVS_FINE_TUNING_PARAMS_S fineTuningParams;
    }RKALGO_AVS_MIDDLE_LUT_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_COMPUTE_FINAL_LUT_SIZE_PARAMS_S
    * @brief   the structure of params for computing the max size of final LUT buffer
    * [I]cameraNum                 : the number of camera
    * [I]dstWmax                   : the largest possible value of output image width
    * [I]dstHmax                   : the largest possible value of output image height
    * [I]finalLutType              : the type of final LUT
    * [I]finalLutStepX             : the step of final LUT in x direction
    * [I]finalLutStepY             : the step of final LUT in y direction
    * [I]finalLutFuseWidth         : the fuse width of final LUT, its value must be 128, 256 or 512
    */
    typedef struct rkAlgoAVS_COMPUTE_FINAL_LUT_SIZE_PARAMS_S
    {
        /* input params */
        uint32_t cameraNum;
        uint32_t dstWmax;
        uint32_t dstHmax;
        RKALGO_AVS_FINAL_LUT_TYPE_E finalLutType;
        uint32_t finalLutStepX;
        uint32_t finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
    }RKALGO_AVS_COMPUTE_FINAL_LUT_SIZE_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_FINAL_LUT_PARAMS_USE_MIDDLE_LUT_S
    * @brief   the structure of params for generating final LUT using middle LUT
    * [I]midLutBuf                  : the params and buffer for middle LUT
    * [I]cameraNum                  : the number of camera
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]projectType                : projection type
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]finalLutType               : the type of final LUT to be generated
    * [I]finalLutStepX              : the step of final LUT in x direction
    * [I]finalLutStepY              : the step of final LUT in y direction
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    */
    typedef struct rkAlgoAVS_FINAL_LUT_PARAMS_USE_MIDDLE_LUT_S
    {
        /* input params and buffer for middle LUT */
        RKALGO_AVS_MIDDLE_LUT_BUFFER_S midLutBuf;
        /* input params for stitched image */
        uint32_t cameraNum;
        uint32_t srcW;
        uint32_t srcH;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        /* input params for final LUT */
        RKALGO_AVS_FINAL_LUT_TYPE_E finalLutType;
        uint32_t finalLutStepX;
        uint32_t finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
    }RKALGO_AVS_FINAL_LUT_PARAMS_USE_MIDDLE_LUT_S;

    /*
    * @struct  RKALGO_AVS_AUTO_FT_PARAMS_S
    * @brief   the structure of params for auto fine tuning
    * [I]calibBuf                   : the buffer of calib file params
    * [I]stitchDistance             : the best stitch distance
    * [I]inputMaskConfig            : the config of input mask
    * [I]fineTuningParams           : the params of fine tuning
    * [I]camStatus                  : the status of camera
    * [I]maxOffset                  : the max x/y offset to fine tuning in pixels
    * [I]scaleRatio                 : the down scal ratio of stitch image, range[0.2, 1]
    * [I]srcImgFormat               : source image format
    * [I]pSrcImageBuf               : the buffer of input images
    * [I]pSrcImgFilePath            : source image path
    * [I]pAftSavePath               : aft middle result save path
    * [I]cameraNum                  : the number of camera
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]projectType                : projection type
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]finalLutStepX              : the step of final LUT in x direction
    * [I]finalLutStepY              : the step of final LUT in y direction
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    */
    typedef struct rkAlgoAVS_AUTO_FT_PARAMS_S
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        float stitchDistance;
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        RKALGO_AVS_FINE_TUNING_PARAMS_S fineTuningParams;
        RKALGO_AVS_CAMERA_STATUS_S camStatus[RKALGO_AVS_MAX_CAMERA_NUM];
        uint32_t maxOffset;
        float scaleRatio;
        RKALGO_AVS_IMG_FORMAT_E srcImgFormat;
        uint8_t *pSrcImageBuf[RKALGO_AVS_MAX_CAMERA_NUM];
        const char *pAftSavePath;
        uint32_t cameraNum;
        uint32_t srcW;
        uint32_t srcH;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        uint32_t finalLutStepX;
        uint32_t finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
    }RKALGO_AVS_AUTO_FT_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_AUTO_FT_OUT_S
    * @brief   the structure of output for auto fine tuning
    * [O]autoFT                     : auto fine tuning rotation result
    * [O]midLutBuf                  : the middle LUT buffer
    */
    typedef struct rkAlgoAVS_AUTO_FT_OUT_S
    {
        /* output */
        RKALGO_AVS_ROTATION_S autoFT[RKALGO_AVS_MAX_CAMERA_NUM];
        RKALGO_AVS_MIDDLE_LUT_BUFFER_S midLutBuf;
    }RKALGO_AVS_AUTO_FT_OUT_S;

    /*
    * @struct  RKALGO_AVS_POS_MESH_PARAMS_S
    * @brief   the structure of params for pos mesh generation
    * [I]midLutBuf                  : the params and buffer for middle LUT
    * [I]cameraNum                  : the number of camera
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]projectType                : projection type
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]queryMode                  : coordinate query mode
    * [I]posMeshStepX               : the step of pos mesh in x direction, Range:[2,256]
    * [I]posMeshStepY               : the step of pos mesh in y direction, Range:[2,256]
    */
    typedef struct rkAlgoAVS_POS_MESH_PARAMS_S
    {
        /* input params and buffer for middle LUT */
        RKALGO_AVS_MIDDLE_LUT_BUFFER_S midLutBuf;
        /* input params for stitched image */
        uint32_t cameraNum;
        uint32_t srcW;
        uint32_t srcH;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        /* input params for generating posMesh */
        RKALGO_AVS_QUERY_MODE_E queryMode;
        uint32_t posMeshStepX;
        uint32_t posMeshStepY;
    }RKALGO_AVS_POS_MESH_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_POS_MESH_BUFFER_S
    * @brief   the structure of output buffer for pos mesh generation
    * [O]pPosMeshBuf                : the buffer address for posMesh
    */
    typedef struct rkAlgoAVS_POS_MESH_BUFFER_S
    {
        /* output buffer */
        float *pPosMeshBuf[RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_POS_MESH_BUFFER_S;

    /*
    * @struct  RKALGO_AVS_POS_QUERY_PARAMS_S
    * @brief   the structure of params for pos query
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]posMeshStepX               : the step of pos mesh in x direction, Range:[2,256]
    * [I]posMeshStepY               : the step of pos mesh in y direction, Range:[2,256]
    */
    typedef struct rkAlgoAVS_POS_QUERY_PARAMS_S
    {
        /* input params */
        uint32_t srcW;
        uint32_t srcH;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t posMeshStepX;
        uint32_t posMeshStepY;
    }RKALGO_AVS_POS_QUERY_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_STITCH_IMG_PARAMS_S
    * @brief   the structure of params for stitching images
    * [I]cameraNum                  : the number of camera
    * [I]imageFormat                : the format of input images
    * [I]pSrcImageBuf               : the buffer of input images
    * [I]finalLutType               : the type of final LUT
    * [I]finalLutPath               : the path of final LUT file
    * [I]bUseGain                   : use gain or not(0-not use; 1-use)
    * [I]gainStepX                  : the step in x direction of gain
    * [I]gainStepY                  : the step in y direction of gain
    */
    typedef struct rkAlgoAVS_STITCH_IMG_PARAMS_S
    {
        /* input params */
        uint32_t cameraNum;
        RKALGO_AVS_IMG_FORMAT_E imageFormat;
        uint8_t *pSrcImageBuf[RKALGO_AVS_MAX_CAMERA_NUM];
        RKALGO_AVS_FINAL_LUT_TYPE_E finalLutType;
        const char *finalLutPath;
        RKALGO_AVS_BOOL bUseGain;
        uint32_t gainStepX;
        uint32_t gainStepY;
    }RKALGO_AVS_STITCH_IMG_PARAMS_S;

    /*
    * @struct  RKALGO_AVS_STITCH_IMG_OUT_S
    * @brief   the structure of output buffer for stitching images
    * [O]pStitchImageBuf            : the buffer of output stitched image
    */
    typedef struct rkAlgoAVS_STITCH_IMG_OUT_S
    {
        /* output buffer */
        uint8_t *pStitchImageBuf;
    }RKALGO_AVS_STITCH_IMG_OUT_S;

    /*
    * @struct  RKALGO_AVS_SRC_OVERLAP_MASK_INPUT_S
    * @brief   the structure of input params for getting src image overlap mask
    * [I]calibBuf                   : the buffer of calib file params
    * [I]stitchDistance             : the best stitch distance
    * [I]inputMaskConfig            : the config of input mask
    * [I]fineTuningParams           : the params of fine tuning
    * [I]cameraNum                  : the number of camera
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]projectType                : projection type
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]finalLutType               : the type of final LUT
    * [I]finalLutStepX              : the step of final LUT in x direction
    * [I]finalLutStepY              : the step of final LUT in y direction
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    */
    typedef struct rkAlgoAVS_SRC_OVERLAP_MASK_INPUT_S
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        float stitchDistance;
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        RKALGO_AVS_FINE_TUNING_PARAMS_S fineTuningParams;
        uint32_t cameraNum;
        uint32_t srcW;
        uint32_t srcH;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        RKALGO_AVS_FINAL_LUT_TYPE_E finalLutType;
        uint32_t finalLutStepX;
        uint32_t finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
    }RKALGO_AVS_SRC_OVERLAP_MASK_INPUT_S;

    /*
    * @struct  RKALGO_AVS_SRC_OVERLAP_MASK_OUTPUT_S
    * @brief   the structure of output for getting src images' overlap mask
    * [O]versionInfo                : the info of AVS version
    * [O]srcRotation                : the clockwise rotation from src image to stitched image
    * [O]overlapMask                : the overlap mask of src images
    */
#pragma pack(push)
#pragma pack(16)
    typedef struct rkAlgoAVS_SRC_OVERLAP_MASK_OUTPUT_S
    {
        /* output */
        char versionInfo[64];
        RKALGO_AVS_ROTATE_E srcRotation[RKALGO_AVS_MAX_CAMERA_NUM];
        uint8_t overlapMask[RKALGO_AVS_SRC_BLK_NUM_W * RKALGO_AVS_SRC_BLK_NUM_H * RKALGO_AVS_MAX_CAMERA_NUM];
    }RKALGO_AVS_SRC_OVERLAP_MASK_OUTPUT_S;
#pragma pack(pop)

    /*
    * @struct  RKALGO_AVS_GET_MASK_USE_DEFINE_INPUT_S
    * @brief   the structure of input params for generating mask yuv400.
    * maskWidth     : width of mask
    * maskHeight    : height of mask
    * maskDefine    : define params for valid region of mask.
    */
    typedef struct rkAlgoAVS_GET_MASK_USE_DEFINE_INPUT_S
    {
        uint32_t maskWidth;
        uint32_t maskHeight;
        RKALGO_AVS_MASK_DEFINE_S maskDefine;
    }RKALGO_AVS_GET_MASK_USE_DEFINE_INPUT_S;

    /*
    * @struct  RKALGO_AVS_ROI_PARAMS_TRANSFORM_IN_S
    * @brief   the structure of input params for ROI params transform
    * [I]enAvsImgProjType       : AVS output image projection type
    * [I]u32AvsImgW             : AVS output image width
    * [I]u32AvsImgH             : AVS output image height
    * [I]u32AvsImgFovX100       : AVS output image fov in x direction
    * [I]u32AvsImgFovY100       : AVS output image fov in y direction
    * [I]s32AvsImgCenterX       : AVS output image project center coordinate in x direction
    * [I]s32AvsImgCenterY       : AVS output image project center coordinate in y direction
    * [I]s32AvsRoiStartX        : ROI start x base on AVS output image
    * [I]s32AvsRoiStartY        : ROI start y base on AVS output image
    * [I]u32AvsRoiW             : ROI width base on AVS output image
    * [I]u32AvsRoiH             : ROI height base on AVS output image
    */
    typedef struct rkAlgoAVS_ROI_PARAMS_TRANSFORM_IN_S
    {
        RKALGO_AVS_PROJECT_TYPE_E enAvsImgProjType;
        uint32_t u32AvsImgW;
        uint32_t u32AvsImgH;
        uint32_t u32AvsImgFovX100;
        uint32_t u32AvsImgFovY100;
        int32_t  s32AvsImgCenterX;
        int32_t  s32AvsImgCenterY;
        int32_t  s32AvsRoiStartX;
        int32_t  s32AvsRoiStartY;
        uint32_t u32AvsRoiW;
        uint32_t u32AvsRoiH;
    }RKALGO_AVS_ROI_PARAMS_TRANSFORM_IN_S;

    /*
    * @struct  RKALGO_AVS_ROI_PARAMS_TRANSFORM_OUT_S
    * @brief   the structure of output params for ROI params transform
    * [I/O]u32AvsRoiDstW        : ROI image dst width in new AVS output
    * [I/O]u32AvsRoiDstH        : ROI image dst height in new AVS output
    * [O]u32AvsRoiFovX100       : AVS ROI image fov in x direction
    * [O]u32AvsRoiFovY100       : AVS ROI image fov in y direction
    * [O]s32AvsRoiCenterX       : AVS ROI image project center coordinate in x direction
    * [O]s32AvsRoiCenterY       : AVS ROI image project center coordinate in y direction
    */
    typedef struct rkAlgoAVS_ROI_PARAMS_TRANSFORM_OUT_S
    {
        uint32_t u32AvsRoiDstW;
        uint32_t u32AvsRoiDstH;
        uint32_t u32AvsRoiFovX100;
        uint32_t u32AvsRoiFovY100;
        int32_t  s32AvsRoiCenterX;
        int32_t  s32AvsRoiCenterY;
    }RKALGO_AVS_ROI_PARAMS_TRANSFORM_OUT_S;
    /*
    * @struct rkAlgoAVS_OVERLAP_PARAMS_S
    * @brief the structure of params for overlap
    * [O]u32OverlapX        : the starting x-coordinate of the overlapping area
    * [O]u32OverlapY        : the starting y-coordinate of the overlapping area
    * [O]u32OverlapW        : width of overlapping areas
    * [O]u32OverlapH        : height of overlapping areas
    */
    typedef struct rkAlgoAVS_OVERLAP_PARAMS_S
    {

        uint32_t u32OverlapX;
        uint32_t u32OverlapY;
        uint32_t u32OverlapW;
        uint32_t u32OverlapH;
    }RKALGO_AVS_OVERLAP_PARAMS_S;
    /* 
    * @struct rkAlgoAVS_GROUND_MODEL_OPTIMIZE_PARAMS_S
    * @brief the structure of optimize ground model
    * [I]cameraNum                  : the number of camera
    * [I]stitchDistance             : the best stitch distance
    * [I]cameraHeight               : camera ground height
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]scaleRatio                 : stitch image scaling factor
    * [I]inputMaskConfig            : the config of input mask
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    * [I]pcOutputPath               : calibration file output path
    * [I]flag                       : ground model optimization options
    */
    typedef struct rkAlgoAVS_GROUND_MODEL_OPTIMIZE_PARAMS_S
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        uint32_t cameraNum;
        float stitchDistance;
        float groundDistance;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        RKALGO_AVS_IMG_FORMAT_E srcImgFormat;
        uint32_t srcW;
        uint32_t srcH;
        uint32_t dstW;
        uint32_t dstH;
        uint8_t* pSrcImageBuf[RKALGO_AVS_MAX_CAMERA_NUM];
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        float scaleRatio;                                                      
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
        const char* pcOutputPath;
        RKALGO_AVS_GROUND_MODEL_OPTIMIZE_OPTION_E flag;
    }RKALGO_AVS_GROUND_MODEL_OPTIMIZE_PARAMS_S;

    /*
    * @struct rkAlgoAVS_GROUND_MODEL_PARAMS_S
    * @brief the structure of input pramas for ground model
    * [I]calibBuf                   : the buffer of calib file params
    * [I]cameraNum                  : the number of camera
    * [I]stitchDistance             : the best stitch distance
    * [I]projectType                : projection type
    * [I]groundDistance             : the distance between camera and ground plane, unit: meter
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]inputMaskConfig            : the config of input mask
    * [I]finalLutType               : the type of final LUT
    * [I]finalLutStepX              : the step of final LUT in x direction
    * [I]finalLutStepY              : the step of final LUT in y direction
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    * [I]highFrequencyFuseWidth     : the high-frequency fuse width, Range:[8,finalLutFuseWidth]
    */

    typedef struct rkAlgoAVS_GROUND_MODEL_PARAMS_S
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        uint32_t cameraNum;
        float stitchDistance;
        float groundDistance;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t srcW;
        uint32_t srcH;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        /* input params for final LUT */
        RKALGO_AVS_FINAL_LUT_TYPE_E finalLutType;
        uint32_t finalLutStepX;
        uint32_t finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
        int32_t highFrequencyFuseWidth;

    }RKALGO_AVS_GROUND_MODEL_PARAMS_S;

    /*
    * @struct rkAlgoAVS_AUTO_STITCH_DISTANCE_PARAMS
    * @brief the structure of input pramas for automatic calculation of stitch distance
    * [I]calibBuf                   : the buffer of calib file params
    * [I]cameraNum                  : the number of camera
    * [I]inputStitchDistance        : The initial input stitch distance
    * [I]projectType                : projection type
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]fovX100                    : output image fov in x direction
    * [I]fovY100                    : output image fov in y direction
    * [I]centerX                    : output image project center coordinate in x direction
    * [I]centerY                    : output image project center coordinate in y direction
    * [I]oriRotation                : output image original rotation
    * [I]rotation                   : output image rotation
    * [I]scaleRatio                 : stitch image scaling factor
    * [I]inputMaskConfig            : the config of input mask
    * [I]finalLutFuseWidth          : the fuse width of final LUT, its value must be 128, 256 or 512
    * [I]pSrcImageBuf               : the buffer of input images
    */
    typedef struct rkAlgoAVS_AUTO_STITCH_DISTANCE_PARAMS
    {
        /* input params */
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        uint32_t cameraNum;
        float inputStitchDistance;
        float groundDistance;
        RKALGO_AVS_PROJECT_TYPE_E projectType;
        uint32_t srcW;
        uint32_t srcH;
        uint32_t dstW;
        uint32_t dstH;
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
        RKALGO_AVS_ROTATION_S oriRotation;
        RKALGO_AVS_ROTATION_S rotation;
        float scaleRatio;
        RKALGO_AVS_MASK_CONFIG_S inputMaskConfig;
        RKALGO_AVS_FUSE_WIDTH_E finalLutFuseWidth;
        uint8_t* pSrcImageBuf[RKALGO_AVS_MAX_CAMERA_NUM];
        RKALGO_AVS_IMG_FORMAT_E srcImgFormat;
    }RKALGO_AVS_AUTO_STITCH_DISTANCE_PARAM_S;
     

    /*
    * @struct rkAlgAVS_GOOD_PARAMS_CONFIG_S
    * @brief the structure of input pramas for good parameters
    * [I]calibBuf                   : the buffer of calib file params
    * [I]cameraNum                  : the number of camera
    * [I]stitchDistance             : the best stitch distance
    * [I]projectType                : projection type
    * [I]srcW                       : input image width
    * [I]srcH                       : input image height
    * [I]dstW                       : output image width
    * [I]dstH                       : output image height
    * [I]oriRotation                : output image original rotation
    */
    typedef struct rkAlgAVS_GOOD_PARAMS_CONFIG_S
    {
        RKALGO_AVS_CALIB_BUFFER_S calibBuf;
        uint32_t cameraNum;
        float stitchDistance;
        uint32_t srcW;
        uint32_t srcH;
        uint32_t dstW;
        uint32_t dstH;
        RKALGO_AVS_ROTATION_S oriRotation;
    }RKALGO_AVS_GOOD_PARAMS_CONFIG_S;


    /*
    * @struct rkAlgoAVS_PROJECT_CONFIG_S
    * @brief the structure of out pramas for project parameters
    * [O]fovX100                    : output image fov in x direction
    * [O]fovY100                    : output image fov in y direction
    * [O]centerX                    : output image project center coordinate in x direction
    * [O]centerY                    : output image project center coordinate in y direction
    */
    typedef struct rkAlgoAVS_PROJECT_CONFIG_S
    {
        uint32_t fovX100;
        uint32_t fovY100;
        int32_t centerX;
        int32_t centerY;
    }RKALGO_AVS_PROJECT_CONFIG_S;

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif // !__RK_ALGO_AVS_TOOL_DEF_H__