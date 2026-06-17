/*
 * Copyright (c) 2025 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __RKADK_SCALER_H_
#define __RKADK_SCALER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rkadk_type.h"

#define RKADK_SCALER_MAX_YUV_PLANE_NUM  3   // max plane number is 3 for YUV Semi-Plane
#define RKADK_SCALER_MAX_CORE_NUM       16  // max core number for multi-thread

//---- RkScalerParams
typedef struct _RkadkScalerParams {
    RKADK_U32  nCallCnt;                                 // [i] process call counter, reserved
    RKADK_U32  nMethodLuma;                              // [i] interpolation method for luma (Y), see ScalerMethod_e
    RKADK_U32  nMethodChrm;                              // [i] interpolation method for chroma (UV), see ScalerMethod_e
    RKADK_U32  nCores;                                   // [i] multi-thread cores to use, set equal to the number of cpu cores

    // Src
    RKADK_FORMAT_E nSrcFmt;                              // [i] src format
    RKADK_U32  nSrcWid;                                  // [i] src width  [pixel]
    RKADK_U32  nSrcHgt;                                  // [i] src height [pixel]
    RKADK_U32  nSrcWStrides[RKADK_SCALER_MAX_YUV_PLANE_NUM];  // [i] src width  strides [byte]
    RKADK_U32  nSrcHStrides[RKADK_SCALER_MAX_YUV_PLANE_NUM];  // [i] src height strides [pixel]
    RKADK_U8  *pSrcBufs[RKADK_SCALER_MAX_YUV_PLANE_NUM];      // [i] src buffer pointers of each plane

    // Dst
    RKADK_FORMAT_E nDstFmt;                              // [i] dst format
    RKADK_U32  nDstWid;                                  // [i] dst width  [pixel]
    RKADK_U32  nDstHgt;                                  // [i] dst height [pixel]
    RKADK_U32  nDstWStrides[RKADK_SCALER_MAX_YUV_PLANE_NUM];  // [i] dst width  strides [byte]
    RKADK_U32  nDstHStrides[RKADK_SCALER_MAX_YUV_PLANE_NUM];  // [i] dst height strides [pixel]
    RKADK_U8  *pDstBufs[RKADK_SCALER_MAX_YUV_PLANE_NUM];      // [i] dst buffer pointers of each plane

    void* context;
} RKADK_SCALER_PARAMS;

RKADK_S32 RKADK_ScalerInit(void **pCtx, RKADK_S32 cores);
RKADK_S32 RKADK_ScalerProcessor(RKADK_SCALER_PARAMS *pScalerParams);
RKADK_S32 RKADK_ScalerDeinit(void *ctx);

#ifdef __cplusplus
}
#endif

#endif // __RKADK_SCALER_H_
