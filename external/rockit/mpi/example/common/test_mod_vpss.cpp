/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include "test_mod_vpss.h"
#include "test_comm_imgproc.h"
#include "test_comm_sys.h"
#include "test_comm_utils.h"
#include "test_comm_vpss.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "rk_debug.h"
#include "rk_mpi_vpss.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_rgn.h"

#define VPSS_GET_CHN_FRAME_TIMEOUT_MS      200

#ifndef UPALIGNTO
#define UPALIGNTO(value, align) (((value) + (align) - 1) / (align) * (align))
#endif

static RK_S32 TEST_VPSS_MosaicAttach(RK_S32 s32Grp, RK_S32 s32Chn, RK_S32 s32Handle, TEST_VPSS_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    RGN_ATTR_S stAttr;
    RGN_CHN_ATTR_S stChnAttr;
    MPP_CHN_S stMppChn;
    RK_U32 u32Width = UPALIGNTO(ctx->s32SrcWidth / 4, 16);
    RK_U32 u32Height = UPALIGNTO(ctx->s32SrcHeight / 4, 16);
    MOSAIC_BLK_SIZE_E mosaicBlkType = MOSAIC_BLK_SIZE_64;

    stMppChn.enModId = RK_ID_VPSS;
    stMppChn.s32DevId = s32Grp;
    stMppChn.s32ChnId = s32Chn;

    memset(&stAttr, 0, sizeof(RGN_ATTR_S));
    memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));

    stAttr.enType = MOSAIC_RGN;
    s32Ret = RK_MPI_RGN_Create(s32Handle, &stAttr);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_Create s32Handle[%d] failed with %#x!", s32Handle, s32Ret);
        return RK_FAILURE;
    }

    stChnAttr.bShow = RK_TRUE;
    stChnAttr.enType = MOSAIC_RGN;
    stChnAttr.unChnAttr.stMosaicChn.enMosaicType = AREA_RECT;
    stChnAttr.unChnAttr.stMosaicChn.stRect.s32X = 0;
    stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = 0;
    stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = u32Width;
    stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height = u32Height;
    stChnAttr.unChnAttr.stMosaicChn.enBlkSize = mosaicBlkType;
    stChnAttr.unChnAttr.stMosaicChn.u32Layer = s32Handle;
    s32Ret = RK_MPI_RGN_AttachToChn(s32Handle, &stMppChn, &stChnAttr);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_AttachToChn s32Handle[%d] failed with %#x!", s32Handle, s32Ret);
        return -1;
    }

    RK_LOGI("create an mosaic region and attach it to chn[%d] rect[%d, %d, %d, %d]",
            s32Chn, stChnAttr.unChnAttr.stMosaicChn.stRect.s32X,
            stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y,
            stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width,
            stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height);
    return 0;
}

static RK_S32 TEST_VPSS_CoverAttach(RK_S32 s32Grp, RK_S32 s32Chn, RK_S32 s32Handle, TEST_VPSS_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    RGN_ATTR_S stCoverAttr;
    RGN_CHN_ATTR_S stCoverChnAttr;
    RK_U32 u32Width = UPALIGNTO(ctx->s32SrcWidth/4, 16);
    RK_U32 u32Height = UPALIGNTO(ctx->s32SrcHeight/4, 16);

    MPP_CHN_S stMppChn;
    stMppChn.enModId = RK_ID_VPSS;
    stMppChn.s32DevId = s32Grp;
    stMppChn.s32ChnId = s32Chn;

    memset(&stCoverAttr, 0, sizeof(stCoverAttr));
    memset(&stCoverChnAttr, 0, sizeof(RGN_CHN_ATTR_S));

    stCoverAttr.enType = COVER_RGN;
    s32Ret = RK_MPI_RGN_Create(s32Handle, &stCoverAttr);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_Create s32Handle[%d] failed with %#x!", s32Handle, s32Ret);
        return RK_FAILURE;
    }

    stCoverChnAttr.bShow = RK_TRUE;
    stCoverChnAttr.enType = COVER_RGN;
    stCoverChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
    stCoverChnAttr.unChnAttr.stCoverChn.stRect.s32X = u32Width;
    stCoverChnAttr.unChnAttr.stCoverChn.stRect.s32Y = u32Height;
    stCoverChnAttr.unChnAttr.stCoverChn.stRect.u32Width = u32Width;
    stCoverChnAttr.unChnAttr.stCoverChn.stRect.u32Height = u32Height;

    //stCoverChnAttr.unChnAttr.stCoverChn.stQuadRangle.bSolid = RK_TRUE;
    //stCoverChnAttr.unChnAttr.stCoverChn.stQuadRangle.u32Thick = 1;

    stCoverChnAttr.unChnAttr.stCoverChn.u32Color = 0xaa00f800;
    stCoverChnAttr.unChnAttr.stCoverChn.u32Layer = s32Handle;
    s32Ret = RK_MPI_RGN_AttachToChn(s32Handle, &stMppChn, &stCoverChnAttr);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_AttachToChn s32Handle[%d] failed with %#x!", s32Handle, s32Ret);
        return RK_FAILURE;
    }

    RK_LOGI("create an cover region and attach it to chn[%d] rect[%d, %d, %d, %d]",
        s32Chn, stCoverChnAttr.unChnAttr.stCoverChn.stRect.s32X,
        stCoverChnAttr.unChnAttr.stCoverChn.stRect.s32Y,
        stCoverChnAttr.unChnAttr.stCoverChn.stRect.u32Width,
        stCoverChnAttr.unChnAttr.stCoverChn.stRect.u32Height);
    return s32Ret;
}

static RK_S32 TEST_VPSS_CmscDetach(RK_S32 s32Grp, RK_S32 s32Chn, RK_S32 s32Handle, TEST_VPSS_CTX_S *ctx) {
    MPP_CHN_S stMppChn;
    RK_S32 s32Ret = RK_SUCCESS;

    stMppChn.enModId = RK_ID_VPSS;
    stMppChn.s32DevId = s32Grp;
    stMppChn.s32ChnId = s32Chn;

    s32Ret = RK_MPI_RGN_DetachFromChn(s32Handle, &stMppChn);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_DetachFromChn failed with %#x!", s32Ret);
        return s32Ret;
    }

    s32Ret = RK_MPI_RGN_Destroy(s32Handle);
    if (RK_SUCCESS != s32Ret) {
        RK_LOGE("RK_MPI_RGN_Destroy failed with %#x!", s32Ret);
        return s32Ret;
    }

    RK_LOGI("detach and destroy cmsc form chn[%d]", s32Chn);
    return s32Ret;
}

static RK_S32 TEST_VPSS_RgnAttach(RK_S32 s32Grp, RK_S32 s32Chn, RK_S32 s32Handle, TEST_VPSS_CTX_S *ctx) {
    int ret;
    RK_U32 u32Width = UPALIGNTO(ctx->s32SrcWidth / 4, 16);
    RK_U32 u32Height = UPALIGNTO(ctx->s32SrcHeight / 4, 16);
    RK_U32 u32Size = u32Width * u32Height * 4;
    RGN_TYPE_E enType;

    if (ctx->s32VProcDevType == VIDEO_PROC_DEV_VPSS)
        enType = OVERLAY_EX_RGN;
    else
        enType = OVERLAY_RGN;

    MPP_CHN_S stMppChn, stMppChn1;
    stMppChn.enModId = RK_ID_VPSS;
    stMppChn.s32DevId = s32Grp;
    stMppChn.s32ChnId = VPSS_MAX_CHN_NUM;

    stMppChn1.enModId = RK_ID_VPSS;
    stMppChn1.s32DevId = s32Grp;
    stMppChn1.s32ChnId = s32Chn;

    void *pData = malloc(u32Size);
    if (!pData) {
        RK_LOGE("malloc osd buffer failed");
        return -1;
    }
    memset(pData, 0xaa, u32Size);

    RGN_ATTR_S stRgnAttr;
    memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));

    stRgnAttr.enType = enType;
    stRgnAttr.unAttr.stOverlay.enPixelFmt = RK_FMT_ARGB8888;
    stRgnAttr.unAttr.stOverlay.stSize.u32Width  = u32Width;
    stRgnAttr.unAttr.stOverlay.stSize.u32Height = u32Height;
    if (enType == OVERLAY_EX_RGN)
        stRgnAttr.unAttr.stOverlay.enVProcDev = VIDEO_PROC_DEV_RGA;

    ret = RK_MPI_RGN_Create(s32Handle, &stRgnAttr);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_Create failed with %#x!", ret);
        return ret;
    }

    ret = RK_MPI_RGN_Create(s32Handle + 1, &stRgnAttr);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_Create 1 failed with %#x!", ret);
        return ret;
    }

    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    stRgnChnAttr.bShow = RK_TRUE;
    stRgnChnAttr.enType = enType;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
    ret = RK_MPI_RGN_AttachToChn(s32Handle, &stMppChn, &stRgnChnAttr);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_AttachToChn s32Handle[%d] failed with %#x!", s32Handle, ret);
        return ret;
    }
    RK_LOGI("create osd and attach it to chn[%d] stPoint[%d, %d, %d, %d]",
            stMppChn.s32ChnId, stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y,
            u32Width, u32Height);

    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = u32Width;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = u32Height;
    ret = RK_MPI_RGN_AttachToChn(s32Handle + 1, &stMppChn1, &stRgnChnAttr);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_AttachToChn s32Handle[%d] failed with %#x!", s32Handle + 1, ret);
        return ret;
    }
    RK_LOGI("create osd and attach it to chn[%d] stPoint[%d, %d, %d, %d]",
            stMppChn1.s32ChnId, stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y,
            u32Width, u32Height);

    BITMAP_S stBitmap;
    memset(&stBitmap, 0, sizeof(BITMAP_S));
    stBitmap.enPixelFormat = RK_FMT_ARGB8888;
    stBitmap.u32Width = u32Width;
    stBitmap.u32Height = u32Height;
    stBitmap.pData = pData;
    ret = RK_MPI_RGN_SetBitMap(s32Handle, &stBitmap);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_SetBitMap failed with %#x!", ret);
        return ret;
    }

    ret = RK_MPI_RGN_SetBitMap(s32Handle + 1, &stBitmap);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_SetBitMap failed with %#x!", ret);
        return ret;
    }

    return 0;
}

static RK_S32 TEST_VPSS_RgnDetach(RK_S32 s32Grp, RK_S32 s32Chn, RK_S32 s32Handle, TEST_VPSS_CTX_S *ctx) {
    MPP_CHN_S stMppChn, stMppChn1;
    int ret;

    stMppChn.enModId = RK_ID_VPSS;
    stMppChn.s32DevId = s32Grp;
    stMppChn.s32ChnId = VPSS_MAX_CHN_NUM;

    stMppChn1.enModId = RK_ID_VPSS;
    stMppChn1.s32DevId = s32Grp;
    stMppChn1.s32ChnId = s32Chn;

    ret = RK_MPI_RGN_DetachFromChn(s32Handle, &stMppChn);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_DetachFromChn failed with %#x!", ret);
        return ret;
    }
    RK_LOGI("destory osd and detach it to chn[%d]", stMppChn.s32ChnId);

    ret = RK_MPI_RGN_DetachFromChn(s32Handle + 1, &stMppChn1);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_DetachFromChn failed with %#x!", ret);
        return ret;
    }
    RK_LOGI("destory osd and detach it to chn[%d]", stMppChn1.s32ChnId);

    ret = RK_MPI_RGN_Destroy(s32Handle);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_Destroy failed with %#x!", ret);
        return ret;
    }

    ret = RK_MPI_RGN_Destroy(s32Handle + 1);
    if (RK_SUCCESS != ret) {
        RK_LOGE("RK_MPI_RGN_Destroy failed with %#x!", ret);
        return ret;
    }
    return 0;
}

static void TEST_VPSS_InitAttr(
        TEST_VPSS_CTX_S *pstCtx, VPSS_GRP_ATTR_S *pstVpssGrpAttr, VPSS_CHN_ATTR_S *pstVpssChnAttrs) {
    memset(pstVpssGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
    memset(pstVpssChnAttrs, 0, sizeof(VPSS_CHN_ATTR_S) * VPSS_MAX_CHN_NUM);

    pstVpssGrpAttr->u32MaxW = 4096;
    pstVpssGrpAttr->u32MaxH = 4096;
    pstVpssGrpAttr->enPixelFormat = (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat;
    pstVpssGrpAttr->enCompressMode = (COMPRESS_MODE_E)pstCtx->s32SrcCompressMode;
    pstVpssGrpAttr->stFrameRate.s32SrcFrameRate = pstCtx->s32SrcGrpRate;
    pstVpssGrpAttr->stFrameRate.s32DstFrameRate = pstCtx->s32DstGrpRate;
    pstVpssGrpAttr->enVProcDev = (VIDEO_PROC_DEV_TYPE_E)pstCtx->s32VProcDevType;

    for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
        pstVpssChnAttrs[i].enChnMode = (VPSS_CHN_MODE_E)pstCtx->s32ChnMode;
        pstVpssChnAttrs[i].enCompressMode = (COMPRESS_MODE_E)pstCtx->s32DstCompressMode;
        pstVpssChnAttrs[i].enDynamicRange = DYNAMIC_RANGE_SDR8;
        pstVpssChnAttrs[i].enPixelFormat = (PIXEL_FORMAT_E)pstCtx->s32DstPixFormat;
        pstVpssChnAttrs[i].enVideoFormat = (VIDEO_FORMAT_E)(pstCtx->s32DstVideoFormat);
        pstVpssChnAttrs[i].stFrameRate.s32SrcFrameRate = pstCtx->s32SrcChnRate;
        pstVpssChnAttrs[i].stFrameRate.s32DstFrameRate = pstCtx->s32DstChnRate;
        pstVpssChnAttrs[i].u32Width = pstCtx->s32DstWidth;
        pstVpssChnAttrs[i].u32Height = pstCtx->s32DstHeight;
        pstVpssChnAttrs[i].u32Depth = pstCtx->u32ChnDepth;
        pstVpssChnAttrs[i].bFlip = (RK_BOOL)pstCtx->s32Flip;
        pstVpssChnAttrs[i].bMirror = (RK_BOOL)pstCtx->s32Mirror;
        pstVpssChnAttrs[i].u32FrameBufCnt = pstCtx->u32ChnDepth + 2;
        pstVpssChnAttrs[i].enRotation = (ROTATION_E)pstCtx->s32Rotation;
        pstVpssChnAttrs[i].enInterpType = (RK_INTERP_TYPE_E)pstCtx->u32InterpType;

        if (pstCtx->bShareBuffer) {
            //set share buffer param
            pstVpssChnAttrs[i].stPoint.s32X = 0;
            pstVpssChnAttrs[i].stPoint.s32Y = pstCtx->s32DstHeight * i;
            if (pstCtx->u32BufferWidth > 0 && pstCtx->u32BufferHeight > 0) {
                pstVpssChnAttrs[i].stMaxSize.u32Width = pstCtx->u32BufferWidth;
                pstVpssChnAttrs[i].stMaxSize.u32Height = pstCtx->u32BufferHeight;
            } else {
                pstVpssChnAttrs[i].stMaxSize.u32Width = pstCtx->s32DstWidth;
                pstVpssChnAttrs[i].stMaxSize.u32Height = pstCtx->s32DstHeight * pstCtx->s32ChnNum;
            }
        } else {
            pstVpssChnAttrs[i].stMaxSize.u32Width = pstCtx->s32DstWidth;
            pstVpssChnAttrs[i].stMaxSize.u32Height = pstCtx->s32DstHeight;
        }

        if (pstCtx->s32AspectRatio == 1)
            pstVpssChnAttrs[i].stAspectRatio.enMode = ASPECT_RATIO_AUTO;
    }
}

static MB_POOL TEST_VPSS_CreatePool(TEST_VPSS_CTX_S *pstCtx) {
    MB_POOL_CONFIG_S stMbPoolCfg;
    PIC_BUF_ATTR_S stPicBufAttr;
    MB_PIC_CAL_S stMbPicCalResult;
    RK_S32 s32Ret = RK_SUCCESS;

    stPicBufAttr.u32Width = pstCtx->s32SrcWidth;
    stPicBufAttr.u32Height = pstCtx->s32SrcHeight;
    stPicBufAttr.enCompMode = (COMPRESS_MODE_E)pstCtx->s32SrcCompressMode;
    stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat;
    s32Ret = RK_MPI_CAL_VGS_GetPicBufferSize(&stPicBufAttr, &stMbPicCalResult);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
        return MB_INVALID_POOLID;
    }

    memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    stMbPoolCfg.u64MBSize = stMbPicCalResult.u32MBSize;
    stMbPoolCfg.u32MBCnt  = 10;
    stMbPoolCfg.enRemapMode = MB_REMAP_MODE_CACHED;
    stMbPoolCfg.bPreAlloc = RK_TRUE;

    return RK_MPI_MB_CreatePool(&stMbPoolCfg);
}

static void *TEST_VPSS_GetRegionLuma(void *arg) {
    RK_S32 s32Ret = RK_SUCCESS;
    TEST_VPSS_LUME_CTX_S *pstCtx = reinterpret_cast<TEST_VPSS_LUME_CTX_S *>(arg);
    RK_U64 u64LumaData[2];
    RECT_S stRects[2] = {{0, 0, 256, 256}, {256, 256, 256, 256}};
    VIDEO_REGION_INFO_S stVideoRgn;

    memset(&stVideoRgn, 0, sizeof(VIDEO_REGION_INFO_S));
    stVideoRgn.pstRegion = stRects;
    stVideoRgn.u32RegionNum = 2;

    while(!pstCtx->u32Quit) {
        s32Ret = RK_MPI_VPSS_GetRegionLuma(pstCtx->u32GrpId, pstCtx->u32ChnId, &stVideoRgn, u64LumaData, 1000);
        if (s32Ret) {
            if (s32Ret != -1) {
                RK_LOGI("VPSS[%d, %d] RK_MPI_VPSS_GetRegionLuma failed[0x%x]", pstCtx->u32GrpId, pstCtx->u32ChnId, s32Ret);
                return NULL;
            }
        } else {
            RK_LOGI("VPSS[%d, %d] Rect[0] {0, 0, 256, 256} -> luma:%lld", pstCtx->u32GrpId, pstCtx->u32ChnId, u64LumaData[0]);
            RK_LOGI("VPSS[%d, %d] Rect[1] {256, 256, 256, 256} -> luma:%lld", pstCtx->u32GrpId, pstCtx->u32ChnId, u64LumaData[1]);
        }
    }

    RK_LOGI("Exit get vpss[%d, %d] luma thread", pstCtx->u32GrpId, pstCtx->u32ChnId);
    return NULL;
}

static void *TEST_VPSS_ModSingleTest(void *arg) {
    RK_S32           s32Ret = RK_SUCCESS;
    TEST_VPSS_CTX_S *pstCtx = reinterpret_cast<TEST_VPSS_CTX_S *>(arg);
    void *retArg = RK_NULL;
    VIDEO_FRAME_INFO_S stChnFrameInfos[VPSS_MAX_CHN_NUM];
    VIDEO_FRAME_INFO_S stGrpFrameInfo;
    char cWritePath[128] = {0};
    RK_S32 loopCount;

    pthread_t lumaTids[VPSS_MAX_CHN_NUM];
    TEST_VPSS_LUME_CTX_S lumaCtx[VPSS_MAX_CHN_NUM];

    memset(stChnFrameInfos, 0, sizeof(VIDEO_FRAME_INFO_S) * VPSS_MAX_CHN_NUM);
    memset(&stGrpFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = TEST_VPSS_ModInit(pstCtx);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

    if (pstCtx->bCmscTest) {
        TEST_VPSS_MosaicAttach(pstCtx->s32GrpIndex, VPSS_MAX_CHN_NUM, 0, pstCtx);
        TEST_VPSS_CoverAttach(pstCtx->s32GrpIndex, 0, 1, pstCtx);
    }

    if (pstCtx->bRgnTest) {
        TEST_VPSS_RgnAttach(pstCtx->s32GrpIndex, 0, 0, pstCtx);
    }

    if (pstCtx->bGetRegionLuma) {
        for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
            memset(&lumaCtx[i], 0, sizeof(TEST_VPSS_LUME_CTX_S));
            lumaCtx[i].u32GrpId = pstCtx->s32GrpIndex;
            lumaCtx[i].u32ChnId = i;
            lumaCtx[i].u32Quit = 0;
            pthread_create(&lumaTids[i], 0, TEST_VPSS_GetRegionLuma, reinterpret_cast<void *>(&lumaCtx[i]));
        }
    }

    for (loopCount = 0; loopCount < pstCtx->s32LoopCount; loopCount++) {
        s32Ret = TEST_VPSS_ModSendFrame(pstCtx);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }

        for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
            s32Ret = RK_MPI_VPSS_GetChnFrame(pstCtx->s32GrpIndex, i, &(stChnFrameInfos[i]), VPSS_GET_CHN_FRAME_TIMEOUT_MS);
            if (s32Ret == RK_SUCCESS) {
                RK_LOGI("vpss[%d, %d] GetChnFrame[%d] ok", pstCtx->s32GrpIndex, i, loopCount);
                if (pstCtx->dstFilePath) {
                    snprintf(cWritePath, sizeof(cWritePath), "%schn_out_%d_%dx%d_%d_%d.bin",
                             pstCtx->dstFilePath, stChnFrameInfos[i].stVFrame.enPixelFormat,
                             stChnFrameInfos[i].stVFrame.u32VirWidth,
                             stChnFrameInfos[i].stVFrame.u32VirHeight, pstCtx->s32GrpIndex, i);
                    s32Ret = TEST_COMM_FileWriteOneFrame(cWritePath, &(stChnFrameInfos[i]));
                    if (s32Ret != RK_SUCCESS) {
                        RK_LOGE("chn[%d] write frame failed. err 0x%x", i, s32Ret);
                    }
                }
                s32Ret = RK_MPI_VPSS_ReleaseChnFrame(pstCtx->s32GrpIndex, i, &(stChnFrameInfos[i]));
                if (s32Ret != RK_SUCCESS) {
                    goto __FAILED;
                }
            }
        }

        if (pstCtx->bCleanCache) {
            RK_MPI_VPSS_CleanCache(pstCtx->s32GrpIndex);
            RK_LOGI("vpss[%d] clean cache", pstCtx->s32GrpIndex);
        }
    }

    retArg = arg;
__FAILED:
    if (pstCtx->bGetRegionLuma) {
        for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
            lumaCtx[i].u32Quit = 1;
            s32Ret = pthread_join(lumaTids[i], NULL);
            if (s32Ret)
                RK_LOGE("Exit get chn[%d] luma thread failed!", i);

            lumaTids[i] = 0;
        }
    }

    if (pstCtx->bCmscTest) {
        TEST_VPSS_CmscDetach(pstCtx->s32GrpIndex, VPSS_MAX_CHN_NUM, 0, pstCtx);
        TEST_VPSS_CmscDetach(pstCtx->s32GrpIndex, 0, 1, pstCtx);
    }

    if (pstCtx->bRgnTest) {
        TEST_VPSS_RgnDetach(pstCtx->s32GrpIndex, 0, 0, pstCtx);
    }

    TEST_VPSS_ModDeInit(pstCtx);
    if (retArg == RK_NULL) {
        RK_LOGE("single vpss test %d running failed.", pstCtx->s32GrpIndex);
    }
    return retArg;
}

RK_S32 TEST_VPSS_ModTest(TEST_VPSS_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    pthread_t tids[VPSS_MAX_GRP_NUM];
    TEST_VPSS_CTX_S tmpCtx[VPSS_MAX_GRP_NUM];

    for (RK_S32 grpIndex = 0; grpIndex < ctx->s32GrpNum; grpIndex++) {
        memcpy(&(tmpCtx[grpIndex]), ctx, sizeof(TEST_VPSS_CTX_S));
        pthread_create(&tids[grpIndex], 0, TEST_VPSS_ModSingleTest, reinterpret_cast<void *>(&tmpCtx[grpIndex]));
        ctx->s32GrpIndex++;
    }

    for (RK_S32 grpIndex = 0; grpIndex < ctx->s32GrpNum; grpIndex++) {
        void *retval = RK_NULL;
        pthread_join(tids[grpIndex], &retval);
        if (retval == RK_NULL) {
            RK_LOGE("Has error test.");
            s32Ret = RK_FAILURE;
        }
    }

    return s32Ret;
}

RK_S32 TEST_VPSS_UnitTest(TEST_VPSS_CTX_S *pstCtx, VIDEO_FRAME_INFO_S *pstChnFrames) {
    RK_S32           s32Ret = RK_SUCCESS;

    s32Ret = TEST_VPSS_ModInit(pstCtx);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }
    s32Ret = TEST_VPSS_ModSendFrame(pstCtx);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }
    s32Ret = TEST_VPSS_ModGetChnFrame(pstCtx, pstChnFrames, 0);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

__FAILED:
    TEST_VPSS_ModDeInit(pstCtx);

    return s32Ret;
}

RK_S32 TEST_VPSS_ModInit(TEST_VPSS_CTX_S *pstCtx) {
    RK_S32           s32Ret = RK_SUCCESS;
    MB_BLK           srcBlk = MB_INVALID_HANDLE;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_CHN_NUM];

    TEST_VPSS_InitAttr(pstCtx, &stVpssGrpAttr, stVpssChnAttr);

    if (pstCtx->bAttachPool) {
        VPSS_MOD_PARAM_S stModParam;
        stModParam.enVpssMBSource = MB_SOURCE_USER;
        s32Ret = RK_MPI_VPSS_SetModParam(&stModParam);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }
    }

    s32Ret = TEST_VPSS_Start(pstCtx->s32GrpIndex, pstCtx->s32ChnNum,
                             &stVpssGrpAttr, stVpssChnAttr);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

    s32Ret = RK_MPI_VPSS_SetVProcDev(pstCtx->s32GrpIndex, (VIDEO_PROC_DEV_TYPE_E)pstCtx->s32VProcDevType);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VPSS_SetVProcDev(grp:%d) failed with %#x!", pstCtx->s32GrpIndex, s32Ret);
        return s32Ret;
    }

    if (pstCtx->s32VProcDevType != VIDEO_PROC_DEV_ISP) {
        s32Ret = TEST_VPSS_GrpSetZoom(pstCtx->s32GrpIndex, pstCtx->s32GrpCropRatio, pstCtx->bGrpCropEn);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }

        s32Ret = RK_MPI_VPSS_SetGrpRotation(pstCtx->s32GrpIndex, (ROTATION_E)pstCtx->s32GrpRotation);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }

        s32Ret = RK_MPI_VPSS_SetGrpMirror(pstCtx->s32GrpIndex, (RK_BOOL)pstCtx->s32GrpMirror, (RK_BOOL)pstCtx->s32GrpFlip);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VPSS_SetGrpMirror(grp:%d) failed with %#x! mirror %d, flip %d.",
                    pstCtx->s32GrpIndex, s32Ret, pstCtx->s32GrpMirror, pstCtx->s32GrpFlip);
        }
    }

    for (RK_S32 chnIndex = 0; chnIndex < pstCtx->s32ChnNum; chnIndex++) {
        if (pstCtx->s32VProcDevType != VIDEO_PROC_DEV_ISP) {
            s32Ret = TEST_VPSS_ChnSetZoom(pstCtx->s32GrpIndex, chnIndex, pstCtx->s32ChnCropRatio, pstCtx->bChnCropEn);
            if (s32Ret != RK_SUCCESS) {
                goto __FAILED;
            }
            s32Ret = TEST_VPSS_SetChnRotation(pstCtx->s32GrpIndex, chnIndex, (ROTATION_E)pstCtx->s32Rotation);
            if (s32Ret != RK_SUCCESS) {
                goto __FAILED;
            }
            s32Ret = TEST_VPSS_SetChnRotationEx(pstCtx->s32GrpIndex, chnIndex, pstCtx->s32RotationEx);
            if (s32Ret != RK_SUCCESS) {
                goto __FAILED;
            }
        }

        if (pstCtx->bAttachPool) {
            pstCtx->attachPool = TEST_VPSS_CreatePool(pstCtx);
            if (pstCtx->attachPool == MB_INVALID_POOLID) {
                s32Ret = RK_FAILURE;
                goto __FAILED;
            }
            s32Ret = RK_MPI_VPSS_AttachMbPool(pstCtx->s32GrpIndex, chnIndex, pstCtx->attachPool);
            if (s32Ret != RK_SUCCESS) {
                goto __FAILED;
            }
        }
    }

    return RK_SUCCESS;
__FAILED:
    for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
        if (pstCtx->bAttachPool) {
            s32Ret = RK_MPI_VPSS_DetachMbPool(pstCtx->s32GrpIndex, i);
            if (s32Ret != RK_SUCCESS) {
                continue;
            }
        }
    }
    if (pstCtx->bAttachPool) {
        RK_MPI_MB_DestroyPool(pstCtx->attachPool);
    }
    return s32Ret;
}

RK_S32 TEST_VPSS_ModDeInit(TEST_VPSS_CTX_S *pstCtx) {
    RK_S32 s32Ret = RK_SUCCESS;

    for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
        if (pstCtx->bAttachPool) {
            s32Ret = RK_MPI_VPSS_DetachMbPool(pstCtx->s32GrpIndex, i);
            if (s32Ret != RK_SUCCESS) {
                continue;
            }
        }
    }

    TEST_VPSS_Stop(pstCtx->s32GrpIndex, pstCtx->s32ChnNum);
    if (pstCtx->bAttachPool) {
        RK_MPI_MB_DestroyPool(pstCtx->attachPool);
    }

    return s32Ret;
}


RK_S32 TEST_VPSS_ModSendFrame(TEST_VPSS_CTX_S *pstCtx) {
    RK_S32 s32Ret = RK_SUCCESS;
    PIC_BUF_ATTR_S stPicBufAttr;
    VIDEO_FRAME_INFO_S stVideoFrame;

    memset(&stVideoFrame, 0x0, sizeof(VIDEO_FRAME_INFO_S));

    stPicBufAttr.u32Width = pstCtx->s32SrcWidth;
    stPicBufAttr.u32Height = pstCtx->s32SrcHeight;
    stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat;
    stPicBufAttr.enCompMode = (COMPRESS_MODE_E)pstCtx->s32SrcCompressMode;
    s32Ret = TEST_SYS_CreateVideoFrame(&stPicBufAttr, &stVideoFrame);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }
    if (pstCtx->srcFileName != RK_NULL) {
        s32Ret = TEST_COMM_FileReadOneFrame(pstCtx->srcFileName, &stVideoFrame, 0);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }
    } else {
        s32Ret = TEST_COMM_FillImage(
                            (RK_U8 *)RK_MPI_MB_Handle2VirAddr(stVideoFrame.stVFrame.pMbBlk),
                            pstCtx->s32SrcWidth, pstCtx->s32SrcHeight,
                            RK_MPI_CAL_COMM_GetHorStride(pstCtx->s32SrcVirWidth,
                                            (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat),
                            pstCtx->s32SrcVirHeight,
                            (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat, 1);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }
    }
    RK_MPI_SYS_MmzFlushCache(stVideoFrame.stVFrame.pMbBlk, RK_FALSE);

    stVideoFrame.stVFrame.enVideoFormat = (VIDEO_FORMAT_E)pstCtx->s32SrcVideoFormat;
    stVideoFrame.stVFrame.enColorGamut = (COLOR_GAMUT_E)pstCtx->s32SrcColorGamut;
    stVideoFrame.stVFrame.enQuantRange = (QUANT_RANGE_E)pstCtx->s32SrcQuantRange;
    s32Ret = RK_MPI_VPSS_SendFrame(pstCtx->s32GrpIndex, 0, &stVideoFrame, -1);
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

__FAILED:
    RK_MPI_MB_ReleaseMB(stVideoFrame.stVFrame.pMbBlk);

    return s32Ret;
}

RK_S32 TEST_VPSS_ModGetGrpFrame(TEST_VPSS_CTX_S *pstCtx, VIDEO_FRAME_INFO_S *pstVideoFrame) {
    RK_S32 s32Ret = RK_SUCCESS;

    s32Ret = RK_MPI_VPSS_GetGrpFrame(pstCtx->s32GrpIndex, 0, pstVideoFrame);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    return s32Ret;
}

RK_S32 TEST_VPSS_ModGetChnFrame(TEST_VPSS_CTX_S *pstCtx, VIDEO_FRAME_INFO_S *pstVideoFrame, RK_S32 loopCount) {
    RK_S32 s32Ret = RK_SUCCESS;

    for (RK_S32 i = 0; i < pstCtx->s32ChnNum; i++) {
        s32Ret = RK_MPI_VPSS_GetChnFrame(pstCtx->s32GrpIndex, i, &(pstVideoFrame[i]), VPSS_GET_CHN_FRAME_TIMEOUT_MS);
        if (s32Ret != RK_SUCCESS) {
            return RK_SUCCESS;
        } else {
            RK_LOGI("vpss[%d, %d] GetChnFrame[%d] ok", pstCtx->s32GrpIndex, i, loopCount);
        }
    }

    return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

