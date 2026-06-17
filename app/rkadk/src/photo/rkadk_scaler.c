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

#include <string.h>
#include "rkadk_scaler.h"
#include "rkadk_log.h"

#ifdef JPEG_SLICE
#include "libRkScalerApi.h"
#endif

#ifdef JPEG_SLICE
enum ScalerFormat_e RKADK_ScalerGetFormat(RKADK_FORMAT_E format) {
    enum ScalerFormat_e enFormat = SCALER_FMT_YUV420SP;

    switch (format) {
        case RKADK_FMT_YUV420SP:
            enFormat = SCALER_FMT_YUV420SP;
            break;
        case RKADK_FMT_YUV422SP:
            enFormat = SCALER_FMT_YUV422SP;
            break;
        default:
            RKADK_LOGE("Unsupported format[%d], default to SCALER_FMT_YUV420SP", format);
            break;
    }

    return enFormat;
}

RKADK_S32 RKADK_ScalerInit(void **pCtx, RKADK_S32 cores) {
    if (*pCtx) {
        RKADK_LOGE("ctx has been created");
        return RKADK_FAILURE;
    }

    return RkScalerInit(pCtx, cores);
}

RKADK_S32 RKADK_ScalerProcessor(RKADK_SCALER_PARAMS *pScalerParams) {
    RkScalerContext context = NULL;
    RkScalerParams scalerParam;

    context = (RkScalerContext)pScalerParams->context;
    if (!context) {
        RKADK_LOGE("context is NULL");
        return RKADK_FAILURE;
    }

    if (!pScalerParams) {
        RKADK_LOGE("pScalerParams is NULL");
        return RKADK_FAILURE;
    }

    if (RKADK_SCALER_MAX_YUV_PLANE_NUM != RKSCALER_MAX_YUV_PLANE_NUM) {
        RKADK_LOGE("RKADK_SCALER_MAX_YUV_PLANE_NUM[%d] != RKSCALER_MAX_YUV_PLANE_NUM[%d]", RKADK_SCALER_MAX_YUV_PLANE_NUM, RKSCALER_MAX_YUV_PLANE_NUM);
        return RKADK_FAILURE;
    }

    if (pScalerParams->nCores > RKADK_SCALER_MAX_CORE_NUM) {
        RKADK_LOGE("nCores[%d] > RKADK_SCALER_MAX_CORE_NUM[%d]", pScalerParams->nCores, RKADK_SCALER_MAX_CORE_NUM);
        return RKADK_FAILURE;
    }

    memset(&scalerParam, 0, sizeof(RkScalerParams));
    scalerParam.nMethodLuma = (RK_U32)pScalerParams->nMethodLuma;
    scalerParam.nMethodChrm = (RK_U32)pScalerParams->nMethodChrm;
    scalerParam.nCores = (RK_U32)pScalerParams->nCores;
    scalerParam.nCallCnt = (RK_U32)pScalerParams->nCallCnt;
    scalerParam.nSrcFmt = RKADK_ScalerGetFormat(pScalerParams->nSrcFmt);
    scalerParam.nSrcWid = (RK_U32)pScalerParams->nSrcWid;
    scalerParam.nSrcHgt = (RK_U32)pScalerParams->nSrcHgt;
    scalerParam.nDstFmt = RKADK_ScalerGetFormat(pScalerParams->nDstFmt);;
    scalerParam.nDstWid = (RK_U32)pScalerParams->nDstWid;
    scalerParam.nDstHgt = (RK_U32)pScalerParams->nDstHgt;

    for (int i = 0; i < RKSCALER_MAX_YUV_PLANE_NUM; i++) {
        scalerParam.nSrcWStrides[i] = (RK_U32)pScalerParams->nSrcWStrides[i];
        scalerParam.nSrcHStrides[i] = (RK_U32)pScalerParams->nSrcHStrides[i];
        scalerParam.pSrcBufs[i] = (RK_U8*)pScalerParams->pSrcBufs[i];

        scalerParam.nDstWStrides[i] = (RK_U32)pScalerParams->nDstWStrides[i];
        scalerParam.nDstHStrides[i] = (RK_U32)pScalerParams->nDstHStrides[i];
        scalerParam.pDstBufs[i] = (RK_U8*)pScalerParams->pDstBufs[i];
    }

    return RkScalerProcessor(context, &scalerParam);
}

RKADK_S32 RKADK_ScalerDeinit(void *ctx) {
    if (!ctx)
         return RKADK_FAILURE;

    return RkScalerDeinit((RkScalerContext)ctx);
}
#endif
