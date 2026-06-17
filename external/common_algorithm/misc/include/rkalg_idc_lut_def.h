/**
 * Copyright     : Copyright(c) 2024 Rockchip. All right reserved.
 * FileName      : rkalg_idc_lut_def.h
 * Creacted By   : cjk
 * Creacted Date : 2024/06/11
 * Updated Date  : 2025/10/30
 * Description   : header file for LUT API structure definition
 */

#ifndef _RKALG_IDC_LUT_DEF_H_
#define _RKALG_IDC_LUT_DEF_H_

#include "rkalg_idc_comm.h"

typedef struct _rkAlgLutCtx_S {
    void *p;
} RKALG_LUT_CTX_S;

typedef enum _rkAlgLutMode_E {
    RKALG_IDC_LUT_DEFAULT_MODE = 0, /* Use default Kernel and NDRange */
    RKALG_IDC_LUT_MANUAL_MODE  = 1, /* Use user defined Kernel and NDRange */
    RKALG_IDC_LUT_AUTO_MODE    = 2  /* Auto Detect best Kernel and NDRange. This takes some extra time at startup. */
} RKALG_LUT_MODE_E;

typedef enum _rkAlgLutOp_E {
    RKALG_IDC_LUT_OP_0 = 0, /* default op*/
    RKALG_IDC_LUT_OP_1 = 1,
    RKALG_IDC_LUT_OP_2 = 2,
    RKALG_IDC_LUT_OP_3 = 3
} RKALG_LUT_OP_E;

typedef struct _rkAlgLutInitParams_S {
    uint32_t u32SrcWidth;     /* 2 pixel aligned */
    uint32_t u32SrcHeight;
    uint32_t u32SrcStride;    /* 64 pixel aligned */
    uint32_t u32SrcHgtStride; /* 2 pixel aligned */
    uint32_t u32DstWidth;
    uint32_t u32DstHeight;    /* 4 pixel aligned */
    uint32_t u32DstStride;    /* 16 pixel aligned */
    uint32_t u32DstHgtStride; /* 8 pixel aligned */
    uint32_t u32Affinity;     /* Reserved */
    char     cOpenclLibraryPath[256]; /* (optional) OpenCL/mali Library Path. If missing, the default system library path will be used. */
    RKALG_LUT_MODE_E eMode;
} RKALG_LUT_INIT_PARAMS_S;

typedef struct _rkAlgLutOpAttr_S {
    RKALG_LUT_OP_E eOp;       /* Only work in manual mode. */
    uint32_t u32LocalSize[2]; /* Only work in manual mode. */
} RKALG_LUT_OP_ATTR_S;

typedef struct _rkAlgLutTask_S {
    RKALG_IDC_IMAGE_S   *pSrcImage; /* src attr and data */
    RKALG_IDC_IMAGE_S   *pDstImage; /* dst attr and data */
    RKALG_IDC_MESH_S    *pMesh;     /* mesh attr and data */
    RKALG_LUT_OP_ATTR_S *pOpAttr;   /* op attr, only work in manual mode.*/
} RKALG_LUT_TASK_S;

#endif //_RKALG_IDC_LUT_DEF_H_