#ifndef __RK_ALGO_AVS_TOOL_DEF_V2_H__
#define __RK_ALGO_AVS_TOOL_DEF_V2_H__

#include "rk_algo_avs_tool_comm.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

    /*
    * @enum    RKALGO_AVS_V2_STITCH_MODE_E
    * @brief   the stitch mode of AVS_V2
    * RKALGO_AVS_V2_STITCH_MODE_MESH_STATIC         : use static mesh
    * RKALGO_AVS_V2_STITCH_MODE_MESH_DYNAMIC        : update mesh dynamically
    * RKALGO_AVS_V2_STITCH_MODE_BUTT                : reserved fields
    */
    typedef enum {
        RKALGO_AVS_V2_STITCH_MODE_MESH_STATIC = 0,
        RKALGO_AVS_V2_STITCH_MODE_MESH_DYNAMIC = 1,
        RKALGO_AVS_V2_STITCH_MODE_BUTT
    }RKALGO_AVS_V2_STITCH_MODE_E;

    /*
    * @struct  RKALGO_AVS_V2_CTX_S
    * @brief   definition of AVS_V2 context struct
    */
    typedef struct AVS_V2_CTX_S {
        void *pCtxData;
    }RKALGO_AVS_V2_CTX_S;

    /*
    * @struct  RKALGO_AVS_V2_MIDDLE_LUT_S
    * @brief   definition of AVS_V2 middle lut params struct
    */
    typedef struct AVS_V2_MIDDLE_LUT_S {
        float                           stitchDistance;
        RKALGO_AVS_MIDDLE_LUT_TYPE_E    middleLutType;
        RKALGO_AVS_MASK_CONFIG_S        inputMaskConfig;
        RKALGO_AVS_FINE_TUNING_PARAMS_S fineTuningParams;
        double                          reservedField[64];                      /* reserved field */
    }RKALGO_AVS_V2_MIDDLE_LUT_S;

    /*
    * @struct  RKALGO_AVS_V2_FINAL_LUT_S
    * @brief   definition of AVS_V2 final lut params struct
    */
    typedef struct AVS_V2_FINAL_LUT_S {
        uint32_t                        dstWmax, dstHmax;
        RKALGO_AVS_FINAL_LUT_TYPE_E     finalLutType;
        uint32_t                        finalLutStepX, finalLutStepY;
        RKALGO_AVS_FUSE_WIDTH_E         finalLutFuseWidth;
        RKALGO_AVS_PROJECT_TYPE_E       projectType;
        uint32_t                        dstW, dstH;
        uint32_t                        fovX100, fovY100;
        int32_t                         centerX, centerY;
        RKALGO_AVS_ROTATION_S           oriRotation;
        RKALGO_AVS_ROTATION_S           rotation;
        double                          reservedField[64];                      /* reserved field */
    }RKALGO_AVS_V2_FINAL_LUT_S;

    /*
    * @struct  RKALGO_AVS_V2_GPU_STITCH_S
    * @brief   definition of AVS_V2 GPU stitching params struct
    */
    typedef struct AVS_V2_GPU_STITCH_S {
        RKALGO_AVS_IMG_FORMAT_E         gfxSrcImgFormat;
        RKALGO_AVS_IMG_FORMAT_E         gfxDstImgFormat;
        RKALGO_AVS_BOOL                 gfxSrcIsAfbc;
        RKALGO_AVS_BOOL                 gfxDstIsAfbc;
        RKALGO_AVS_BOOL                 gfxDoResize;                            /* if do resize on GPU stitch, set RKALGO_AVS_TRUE */
        uint32_t                        gfxDstWidResize;
        uint32_t                        gfxDstHgtResize;
        double                          reservedField[64];                      /* reserved field */
    }RKALGO_AVS_V2_GPU_STITCH_S;

    /*
    * @struct  RKALGO_AVS_V2_IMG_BUF_S
    * @brief   definition of AVS_V2 images buffer struct
    */
    typedef struct AVS_V2_IMG_BUF_S {
        uint32_t                        bufferIdx;                                      /* index of current buffer */
        RKALGO_AVS_BOOL                 isOccupied;                                     /* if current buffer is occupied, set RKALGO_AVS_TRUE */
        void                            *pGfxVirAddr[RKALGO_AVS_MAX_CAMERA_NUM + 1];    /* src and dst images bufffer address */
        int32_t                         gfxMemFd[RKALGO_AVS_MAX_CAMERA_NUM + 1];        /* src and dst images bufffer fd */
        void                            *userdata;                                      /* user defined data pointer for passing to callback function */
        double                          reservedField[64];                              /* reserved field */
    }RKALGO_AVS_V2_IMG_BUF_S;

    /* call back function */
    typedef int32_t(*CALL_BACK_FUNC_IMG_BUF) (const RKALGO_AVS_V2_IMG_BUF_S *pstImgBuffer);

    /*
    * @struct  RKALGO_AVS_V2_CALL_BACK_S
    * @brief   definition of AVS_V2 callback struct
    */
    typedef struct AVS_V2_CALL_BACK_S {
        CALL_BACK_FUNC_IMG_BUF          funcCallBackImgBuf;
    }RKALGO_AVS_V2_CALL_BACK_S;


    /*
    * @struct  RKALGO_AVS_V2_INIT_S
    * @brief   definition of AVS_V2 init struct
    */
    typedef struct AVS_V2_INIT_S {
        char                            srcCalibFilePath[512];
        char                            srcMaskYuvPathPrefix[512];
        char                            dstMiddleLutPath[512];
        char                            dstFinalLutPath[512];
        uint32_t                        cameraNum;
        uint32_t                        srcW, srcH;
        RKALGO_AVS_V2_STITCH_MODE_E     enStitchMode;                   /* update mesh dynamically or not */
        RKALGO_AVS_V2_MIDDLE_LUT_S      stMidLutParamsInput;            /* for middle LUT */
        RKALGO_AVS_V2_FINAL_LUT_S       stFinLutParamsInput;            /* for final LUT */
        RKALGO_AVS_V2_GPU_STITCH_S      stGpuStiParamsInput;            /* for GPU stitch */
        RKALGO_AVS_V2_CALL_BACK_S       stCallBack;                     /* for call back to get dst img */
        RKALGO_AVS_BOOL                 bUseMidLutBuf;                  /* if use data from stMidLutBuf, set RKALGO_AVS_TRUE */
        RKALGO_AVS_MIDDLE_LUT_BUFFER_S  stMidLutBuf;                    /* middle LUT buffer */
        RKALGO_AVS_BOOL                 bUseFinLutBuf;                  /* if use data from stFinLutBuf, set RKALGO_AVS_TRUE */
        RKALGO_AVS_FINAL_LUT_BUFFER_S   stFinLutBuf;                    /* final LUT buffer */
        double                          reservedField[64];              /* reserved field */
    }RKALGO_AVS_V2_INIT_S;

    /*
    * @struct  RKALGO_AVS_V2_PROCESS_S
    * @brief   definition of AVS_V2 stitch struct for every frame
    */
    typedef struct AVS_V2_PROCESS_S {
        /* for middle LUT update */
        RKALGO_AVS_BOOL                 bUpdateMiddleLut;               /* if stMidLutParamsInput changes, set RKALGO_AVS_TRUE */
        RKALGO_AVS_V2_MIDDLE_LUT_S      stMidLutParamsInput;
        /* for final LUT update */
        RKALGO_AVS_BOOL                 bUpdateFinalLut;                /* if stFinLutParamsInput changes, set RKALGO_AVS_TRUE */
        RKALGO_AVS_V2_FINAL_LUT_S       stFinLutParamsInput;
        /* for GPU stitching images */
        RKALGO_AVS_BOOL                 bUpdateGpuStitch;               /* if stGpuStiParamsInput changes, set RKALGO_AVS_TRUE */
        RKALGO_AVS_V2_GPU_STITCH_S      stGpuStiParamsInput;
        /* for src and dst images buffer addr and fd */
        RKALGO_AVS_V2_IMG_BUF_S         stImgBuffer;
        double                          reservedField[64];              /* reserved field */
    }RKALGO_AVS_V2_PROCESS_S;


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif // !__RK_ALGO_AVS_TOOL_DEF_V2_H__