#ifndef __RK_ALGO_AVS_TOOL_API_H__
#define __RK_ALGO_AVS_TOOL_API_H__

#include "rk_algo_avs_tool_def.h"
#include "rk_algo_avs_tool_comm.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#if defined _WIN32 || defined __CYGWIN__
    #ifdef BUILDING_DLL
        #ifdef __GNUC__
            #define DLL_PUBLIC __attribute__ ((dllexport))
        #else
            // Note: actually gcc seems to also supports this syntax.
            #define DLL_PUBLIC __declspec(dllexport)
        #endif
    #else
        #ifdef __GNUC__
            #define DLL_PUBLIC __attribute__ ((dllimport))
        #else
            // Note: actually gcc seems to also supports this syntax.
            #define DLL_PUBLIC
        #endif
        #define DLL_LOCAL
    #endif
#else
    #if __GNUC__ >= 4
        #define DLL_PUBLIC __attribute__ ((visibility ("default")))
        #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
    #else
        #define DLL_PUBLIC
        #define DLL_LOCAL
    #endif
#endif

    /* get RKALGO AVS tool version */
    DLL_PUBLIC int32_t RKALGO_AVS_GetVersion(char avsToolVersion[128]);

    /* get params from calibration file */
    DLL_PUBLIC int32_t RKALGO_AVS_GetCalibParamsFromCalibFile(const RKALGO_AVS_CALIB_PARAMS_S *pstCalibParams, RKALGO_AVS_CALIB_BUFFER_S *pstCalibBuf);

    /* get the size of buffer for middle LUT */
    DLL_PUBLIC int32_t RKALGO_AVS_GetMiddleLutBufSize(RKALGO_AVS_MIDDLE_LUT_TYPE_E middleLutType, uint32_t *u32MiddleLutBufSize);

    /* get middle LUT from a calibration parameters */
    DLL_PUBLIC int32_t RKALGO_AVS_GetMiddleLutFromCalibParams(const RKALGO_AVS_MIDDLE_LUT_PARAMS_S *pstMidLutParams, RKALGO_AVS_MIDDLE_LUT_BUFFER_S *pstMidLutBuf);

    /* get the max size of buffer for final LUT */
    DLL_PUBLIC int32_t RKALGO_AVS_GetFinalLutBufMaxSize(const RKALGO_AVS_COMPUTE_FINAL_LUT_SIZE_PARAMS_S *pstComputefinalLutSizeParams, RKALGO_AVS_FINAL_LUT_BUFFER_S *pstFinalLutBuf);

    /* get final LUT from middle LUT */
    DLL_PUBLIC int32_t RKALGO_AVS_GetFinalLutFromMiddleLut(const RKALGO_AVS_FINAL_LUT_PARAMS_USE_MIDDLE_LUT_S *pstFinalLutParams, RKALGO_AVS_FINAL_LUT_BUFFER_S *pstFinalLutBuf);

    /* get yuv400 format mask file from RGB format mask image */
    DLL_PUBLIC int32_t RKALGO_AVS_GetMaskYuv400FromRgb(const char *pSrcMaskImgPath, const char *pDstMaskYuvPath);

    /* automatically compute fine tuning parameters from calibration parameters */
    DLL_PUBLIC int32_t RKALGO_AVS_AutoFineTuningFromCalibParams(const RKALGO_AVS_AUTO_FT_PARAMS_S *pstAftParams, RKALGO_AVS_AUTO_FT_OUT_S *pstAftOut);

    /* get posMesh(position coordinate query mesh) from middle LUT */
    DLL_PUBLIC int32_t RKALGO_AVS_GetPosMeshFromMiddleLut(const RKALGO_AVS_POS_MESH_PARAMS_S *pstPosMeshParams, RKALGO_AVS_POS_MESH_BUFFER_S *pstPosMeshBuf);

    /* position coordinate query: dst coordinate to src coordinate */
    DLL_PUBLIC int32_t RKALGO_AVS_PosQueryDst2Src(const RKALGO_AVS_POS_QUERY_PARAMS_S *pstPosQueryParams, const float *pOneCameraPosMeshBuf, const RKALGO_AVS_POINT_S *pstDstPointIn, RKALGO_AVS_POINT_S *pstSrcPointOut);

    /* position coordinate query: src coordinate to dst coordinate */
    DLL_PUBLIC int32_t RKALGO_AVS_PosQuerySrc2Dst(const RKALGO_AVS_POS_QUERY_PARAMS_S *pstPosQueryParams, const float *pOneCameraPosMeshBuf, const RKALGO_AVS_POINT_S *pstSrcPointIn, RKALGO_AVS_POINT_S *pstDstPointOut);

    /* get stitched image from final LUT */
    DLL_PUBLIC int32_t RKALGO_AVS_GetStitchImageFromFinalLut(const RKALGO_AVS_STITCH_IMG_PARAMS_S *pstStitchImgParams, RKALGO_AVS_STITCH_IMG_OUT_S *pstStitchImgOut);

    /* get src image overlap mask data from calibration parameters */
    DLL_PUBLIC int32_t RKALGO_AVS_GetSrcOverlapMaskFromCalibParams(const RKALGO_AVS_SRC_OVERLAP_MASK_INPUT_S *pstInput, RKALGO_AVS_SRC_OVERLAP_MASK_OUTPUT_S *pstOutput);

    /* get yuv400 format mask file from mask define params */
    DLL_PUBLIC int32_t RKALGO_AVS_GetMaskYuv400FromDefineParams(RKALGO_AVS_GET_MASK_USE_DEFINE_INPUT_S *pstGetMaskUseDefine, const char *pDstMaskYuvPath);

    /* combine two groups of Euler angles into one group */
    DLL_PUBLIC int32_t RKALGO_AVS_CombineTwoGrpEulerAnglesIntoOneGrp(RKALGO_AVS_ROTATION_S *pOriRotation, RKALGO_AVS_ROTATION_S *pRotation, RKALGO_AVS_ROTATION_S *pOriRotationOut);

    /* get AVS fov and center params from ROI params */
    DLL_PUBLIC int32_t RKALGO_AVS_GetAvsParamsFromRoiParams(RKALGO_AVS_ROI_PARAMS_TRANSFORM_IN_S *pstRoiParamsIn, RKALGO_AVS_ROI_PARAMS_TRANSFORM_OUT_S *pstRoiParamsOut);

    /* get final LUT from calibration file */
    DLL_PUBLIC int32_t RKALGO_AVS_GroundModel_GetFinalLutFromCalibParams(const RKALGO_AVS_GROUND_MODEL_PARAMS_S* pstGroundModelParams, RKALGO_AVS_FINAL_LUT_BUFFER_S* pstFinalLutBuf);
  
    /* conversion of ground Euler angle to original Euler angle */
    DLL_PUBLIC int32_t RKALGO_AVS_GroundModel_EulerAngleToOriRotation (RKALGO_AVS_ROTATION_S* pGroundRotation, RKALGO_AVS_ROTATION_S* pOriRotation);

    DLL_PUBLIC int32_t RKALGO_AVS_GroundModel_AutoGetStitchDistance(const RKALGO_AVS_AUTO_STITCH_DISTANCE_PARAM_S* pstAutoStitchDistanceParams, const float groundDistance, float* autoStitchDistance);

    DLL_PUBLIC int32_t RKALGO_AVS_AutoGetStitchDistance(const RKALGO_AVS_AUTO_STITCH_DISTANCE_PARAM_S* pstAutoStitchDistanceParams, float* autoStitchDistance);

    DLL_PUBLIC int32_t RKALGO_AVS_getGoodParams(const RKALGO_AVS_GOOD_PARAMS_CONFIG_S* pstGoodParamsConfig, RKALGO_AVS_PROJECT_CONFIG_S* pstProjectConfig);

    DLL_PUBLIC int32_t RKALGO_AVS_GroundModel_getGoodParams(const RKALGO_AVS_GOOD_PARAMS_CONFIG_S* pstGoodParamsConfig, const float groundDistance, RKALGO_AVS_PROJECT_CONFIG_S* pstProjectConfig);

    DLL_PUBLIC int32_t RKALGO_AVS_GroundModel_OptimizeParamsFromCalibParams(const RKALGO_AVS_GROUND_MODEL_OPTIMIZE_PARAMS_S* pstGroundParams, float* pGroundDistance);

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif // !__RK_ALGO_AVS_TOOL_API_H__