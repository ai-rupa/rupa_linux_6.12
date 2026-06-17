/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
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

#include "rkadk_photo.h"
#include "rkadk_log.h"
#include "rkadk_media_comm.h"
#include "rkadk_param.h"
#include "rkadk_thumb_comm.h"
#include "rkadk_signal.h"
#include "rk_mpi_tde.h"
#include <byteswap.h>
#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utime.h>

#include "rkadk_scaler.h"

#ifndef RKADK_PHOTO_SLICE_TIME_LOG
#define RKADK_PHOTO_SLICE_TIME_LOG 0
#endif

#if RKADK_PHOTO_SLICE_TIME_LOG
static inline RKADK_U64 RKADK_PHOTO_GetTimeUs(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (RKADK_U64)tv.tv_sec * 1000000ULL + (RKADK_U64)tv.tv_usec;
}
#endif

#define JPG_THUMB_TAG_LEN 4

#define GET_DATA_VDEC_CHN (VDEC_MAX_CHN_NUM - 2)
#define GET_DATA_VPSS_GRP (VPSS_MAX_GRP_NUM - 2)
#define GET_DATA_VPSS_CHN 0

#define JPG_MMAP_FILE_PATH "/tmp/.mmap"

typedef enum {
  RKADK_JPG_LITTLE_ENDIAN, // II
  RKADK_JPG_BIG_ENDIAN,    // MM
  RKADK_JPG_BYTE_ORDER_BUTT
} RKADK_JPG_BYTE_ORDER_E;

typedef struct {
  RKADK_U16 u16Type;
  RKADK_U16 u16TypeByte;
} RKADK_JPG_DE_TYPE_S;

typedef struct {
  RKADK_S32 s32Witdh;
  RKADK_S32 s32Height; //uneven slice
  RKADK_S32 s32EvenHeight; //even slice
  RKADK_S32 s32LastWidth;
  RKADK_S32 s32LastHeight;
} RKADK_JPG_SLICE_RES;

typedef struct {
  bool bJpegSlice;
  RKADK_U32 u32SliceCount;
  pthread_t sliceTid;
  RKADK_JPG_SLICE_RES stViSlice;
  RKADK_JPG_SLICE_RES stVencSlice;

  //mcu count contained in each slice, the unit of mcu is 16 x 16
  RK_U32 u32MCUPerECS;
} RKADK_JPG_SLICE_PARAM;

typedef struct {
  RKADK_U32 u32CamId;
  RKADK_U32 u32ViChn;
  bool bReseting;
  bool bUseVpss;
  bool bFmtChange;
  RKADK_PHOTO_DATA_RECV_FN_PTR pDataRecvFn;
  pthread_t tid;
  bool bGetJpeg;
  RKADK_U32 u32PhotoCnt;
  RKADK_JPG_SLICE_PARAM stSliceParam;
  void *userdata;
  RKADK_PHOTO_FMT_CHANGE_S stFmtChange;
} RKADK_PHOTO_HANDLE_S;

static RKADK_U8 *RKADK_PHOTO_Mmap(RKADK_CHAR *FileName, RKADK_U32 u32PhotoLen) {
  char data = 0xff;
  FILE *fd = NULL;
  RKADK_U8 *pu8Photo = NULL;

  fd = fopen(FileName, "w+");
  if (!fd) {
    RKADK_LOGE("open %s failed", FileName);
    return NULL;
  }

  if (fseek(fd, u32PhotoLen, SEEK_SET)) {
    fclose(fd);
    RKADK_LOGE("seek %s failed", FileName);
    return NULL;
  }
  fwrite(&data, 1, 1, fd);

  pu8Photo = (RKADK_U8 *)mmap(NULL, u32PhotoLen, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fd), 0);
  if (pu8Photo == MAP_FAILED) {
    fclose(fd);
    RKADK_LOGE("Mmap %s, errno: %d",FileName, errno);
    return NULL;
  }
  fclose(fd);

  return pu8Photo;
}

static int RKADK_PHOTO_SetViSliceParam(RKADK_PHOTO_HANDLE_S *pHandle,
                                RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg, RKADK_U32 u32Width,
                                RKADK_U32 u32Height) {
  int count;
  RKADK_U32 multiplier = 0;
  RKADK_S32 s32Height = 0, s32SaveHeight = 0;

  pHandle->stSliceParam.stViSlice.s32Witdh = u32Width;
  pHandle->stSliceParam.stViSlice.s32LastWidth = u32Width;

  multiplier = u32Height * pHandle->stSliceParam.stVencSlice.s32Height;
  s32Height = multiplier / pstPhotoCfg->image_height;
  if (multiplier % pstPhotoCfg->image_height) {
    s32SaveHeight = s32Height;
    s32Height += 1;
  }

RETRY:
  if (s32Height % 2) {
    pHandle->stSliceParam.stViSlice.s32Height = s32Height - 1;
    pHandle->stSliceParam.stViSlice.s32EvenHeight = s32Height + 1;
  } else {
    pHandle->stSliceParam.stViSlice.s32Height = s32Height;
    pHandle->stSliceParam.stViSlice.s32EvenHeight = s32Height;
  }

  count = (pHandle->stSliceParam.u32SliceCount - 1) / 2;
  pHandle->stSliceParam.stViSlice.s32LastHeight = u32Height - (pHandle->stSliceParam.stViSlice.s32Height
    + pHandle->stSliceParam.stViSlice.s32EvenHeight) * count;
  if ((pHandle->stSliceParam.u32SliceCount - 1) % 2)
    pHandle->stSliceParam.stViSlice.s32LastHeight -= pHandle->stSliceParam.stViSlice.s32Height;

  if (pHandle->stSliceParam.stViSlice.s32LastHeight <= 0) {
    RKADK_LOGD("Invalid s32LastHeight[%d], recalculation", pHandle->stSliceParam.stViSlice.s32LastHeight);
    s32Height = s32SaveHeight;
    goto RETRY;
  }

  RKADK_LOGD("Vi slice w*h[%d, %d, %d], last slice w*h[%d, %d]",
              pHandle->stSliceParam.stViSlice.s32Witdh,
              pHandle->stSliceParam.stViSlice.s32Height,
              pHandle->stSliceParam.stViSlice.s32EvenHeight,
              pHandle->stSliceParam.stViSlice.s32LastWidth,
              pHandle->stSliceParam.stViSlice.s32LastHeight);

  return 0;
}

static int RKADK_PHOTO_SetSliceParam(RKADK_PHOTO_HANDLE_S *pHandle,
                              RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg,
                              RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg) {
  if (pstPhotoCfg->slice_height <= 0 || pstPhotoCfg->slice_height > pstPhotoCfg->image_height) {
    RKADK_LOGE("Invalid slice_height[%d], image_height[%d]", pstPhotoCfg->slice_height, pstPhotoCfg->image_height);
    return -1;
  }

  if (pstPhotoCfg->slice_height % 16) {
    RKADK_LOGE("Invalid slice_height[%d], must be 16 aligned", pstPhotoCfg->slice_height);
    return -1;
  }

  if (pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat != RK_FMT_YUV420SP
      && pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat != RK_FMT_YUV422SP) {
    RKADK_LOGE("Jpeg slice unsupport format: %d", pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat);
    return -1;
  }

  pHandle->stSliceParam.stVencSlice.s32Witdh = pstPhotoCfg->image_width;
  pHandle->stSliceParam.stVencSlice.s32Height = pstPhotoCfg->slice_height;

  pHandle->stSliceParam.u32SliceCount = pstPhotoCfg->image_height / pstPhotoCfg->slice_height;
  if (pstPhotoCfg->image_height % pstPhotoCfg->slice_height)
    pHandle->stSliceParam.u32SliceCount += 1;

  pHandle->stSliceParam.u32MCUPerECS = UPALIGNTO(pstPhotoCfg->image_width, 16) * pstPhotoCfg->slice_height / 256;

  pHandle->stSliceParam.stVencSlice.s32LastWidth = pHandle->stSliceParam.stVencSlice.s32Witdh;
  pHandle->stSliceParam.stVencSlice.s32LastHeight
    = pstPhotoCfg->image_height - pstPhotoCfg->slice_height * (pHandle->stSliceParam.u32SliceCount - 1);

  if (pHandle->stSliceParam.stVencSlice.s32Witdh > JPEG_SLICE_WIDTH_MAX) {
    RKADK_LOGE("Slice output width[%d] > %d", pHandle->stSliceParam.stVencSlice.s32Witdh, JPEG_SLICE_WIDTH_MAX);
    return -1;
  }

  if (pHandle->stSliceParam.stVencSlice.s32Height > JPEG_SLICE_HEIGHT_MAX) {
    RKADK_LOGE("Slice output height[%d] > %d", pHandle->stSliceParam.stVencSlice.s32Witdh, JPEG_SLICE_HEIGHT_MAX);
    return -1;
  }

  RKADK_LOGD("Jpeg slice w*h[%d, %d], last slice w*h[%d, %d], u32MCUPerECS: %d, slice count: %d",
              pHandle->stSliceParam.stVencSlice.s32Witdh,
              pHandle->stSliceParam.stVencSlice.s32Height,
              pHandle->stSliceParam.stVencSlice.s32LastWidth,
              pHandle->stSliceParam.stVencSlice.s32LastHeight,
              pHandle->stSliceParam.u32MCUPerECS,
              pHandle->stSliceParam.u32SliceCount);

  return 0;
}

//#define JPEG_SLICE_WRITE
//#define FULL_IMAGE_TEST

typedef struct {
  RKADK_PHOTO_HANDLE_S *pHandle;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg;
  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg;

  RKADK_U32 u32DstBufSize;
  RKADK_U32 u32PhotoLen;
  RKADK_U8 *pu8Photo; /* mmap buffer for exif+thumb merge */

  MB_POOL vencMbPool;

  VENC_STREAM_S stStream;
  VENC_PACK_S stPack;

  VENC_STREAM_S stThumbFrame;
  VENC_PACK_S stThumbPack;

  bool bGetThumb;
} RKADK_PHOTO_SLICE_CTX_S;

static RKADK_S32 RKADK_PHOTO_DropFirstFrame(RKADK_S32 vencChn, VENC_STREAM_S *pstStream) {
  int ret;

  ret = RK_MPI_VENC_GetStream(vencChn, pstStream, 1000);
  if (ret == RK_SUCCESS) {
    RK_MPI_VENC_ReleaseStream(vencChn, pstStream);
  } else {
    RKADK_LOGE("RK_MPI_VENC_GetStream[%d] timeout[%x]", vencChn, ret);
  }

  RK_MPI_VENC_StopRecvFrame(vencChn);
  RK_MPI_VENC_ResetChn(vencChn);
  return 0;
}

static RKADK_S32 RKADK_PHOTO_SliceInit(RKADK_PHOTO_SLICE_CTX_S *pstCtx, void *params,
                                            RKADK_FORMAT_E *enPixelFormat) {
  MB_POOL_CONFIG_S stMbPoolCfg;
  RKADK_CHAR mapPath[RKADK_MAX_FILE_PATH_LEN];

  memset(pstCtx, 0, sizeof(*pstCtx));
  pstCtx->vencMbPool = MB_INVALID_POOLID;
  pstCtx->pu8Photo = NULL;
  pstCtx->bGetThumb = false;

  pstCtx->pHandle = (RKADK_PHOTO_HANDLE_S *)params;
  if (!pstCtx->pHandle) {
    RKADK_LOGE("Get jpeg thread invalid param");
    return -1;
  }

  if (!pstCtx->pHandle->pDataRecvFn) {
    RKADK_LOGE("u32CamId[%d] don't register callback", pstCtx->pHandle->u32CamId);
    return -1;
  }

  pstCtx->pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pstCtx->pHandle->u32CamId);
  if (!pstCtx->pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  pstCtx->ptsThumbCfg = RKADK_PARAM_GetThumbCfg(pstCtx->pHandle->u32CamId);
  if (!pstCtx->ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  if (pstCtx->pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat == RK_FMT_YUV420SP) {
    pstCtx->u32DstBufSize = pstCtx->pstPhotoCfg->max_slice_width * pstCtx->pstPhotoCfg->max_slice_height * 3 / 2;
    if (enPixelFormat)
      *enPixelFormat = RKADK_FMT_YUV420SP;
  } else if (pstCtx->pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat == RK_FMT_YUV422SP) {
    pstCtx->u32DstBufSize = pstCtx->pstPhotoCfg->max_slice_width * pstCtx->pstPhotoCfg->max_slice_height * 2;
    if (enPixelFormat)
      *enPixelFormat = RKADK_FMT_YUV422SP;
  } else {
    RKADK_LOGE("Unsupported format[%d]", pstCtx->pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat);
    return -1;
  }

  pstCtx->u32PhotoLen = pstCtx->pHandle->stSliceParam.stVencSlice.s32Witdh * pstCtx->pHandle->stSliceParam.stVencSlice.s32Height
                        + pstCtx->ptsThumbCfg->thumb_width * pstCtx->ptsThumbCfg->thumb_height;

  snprintf(mapPath, RKADK_MAX_FILE_PATH_LEN, "%s_%d.jpeg", JPG_MMAP_FILE_PATH, pstCtx->pstPhotoCfg->venc_chn);
  pstCtx->pu8Photo = RKADK_PHOTO_Mmap(mapPath, pstCtx->u32PhotoLen);
  if (!pstCtx->pu8Photo)
    return -1;

  memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
  stMbPoolCfg.u64MBSize = pstCtx->u32DstBufSize;
  stMbPoolCfg.u32MBCnt = 1;
  stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
  stMbPoolCfg.bPreAlloc = RK_TRUE;
  pstCtx->vencMbPool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
  if (pstCtx->vencMbPool == MB_INVALID_POOLID) {
    RKADK_LOGE("Create venc MB pool failed!");
    goto Exit;
  }

  memset(&pstCtx->stStream, 0, sizeof(VENC_STREAM_S));
  memset(&pstCtx->stPack, 0, sizeof(VENC_PACK_S));
  memset(&pstCtx->stThumbFrame, 0, sizeof(VENC_STREAM_S));
  memset(&pstCtx->stThumbPack, 0, sizeof(VENC_PACK_S));
  pstCtx->stStream.pstPack = &pstCtx->stPack;
  pstCtx->stThumbFrame.pstPack = &pstCtx->stThumbPack;

  RKADK_PHOTO_DropFirstFrame(pstCtx->pstPhotoCfg->venc_chn, &pstCtx->stStream);
  RKADK_PHOTO_DropFirstFrame(pstCtx->ptsThumbCfg->photo_venc_chn, &pstCtx->stThumbFrame);

  return 0;

Exit:
  if (pstCtx->vencMbPool != MB_INVALID_POOLID) {
    RK_MPI_MB_DestroyPool(pstCtx->vencMbPool);
    pstCtx->vencMbPool = MB_INVALID_POOLID;
  }
  if (pstCtx->pu8Photo) {
    munmap(pstCtx->pu8Photo, pstCtx->u32PhotoLen);
    pstCtx->pu8Photo = NULL;
  }
  return -1;
}

static void RKADK_PHOTO_SliceDeinit(RKADK_PHOTO_SLICE_CTX_S *pstCtx) {
  if (!pstCtx)
    return;

  if (pstCtx->vencMbPool != MB_INVALID_POOLID) {
    RK_MPI_MB_DestroyPool(pstCtx->vencMbPool);
    pstCtx->vencMbPool = MB_INVALID_POOLID;
  }

  if (pstCtx->pu8Photo) {
    munmap(pstCtx->pu8Photo, pstCtx->u32PhotoLen);
    pstCtx->pu8Photo = NULL;
  }
}

static RKADK_S32 RKADK_PHOTO_SliceJpeg(RKADK_PHOTO_SLICE_CTX_S *pstCtx) {
  int ret;
  RKADK_PHOTO_RECV_DATA_S stData;
  RKADK_U8 *pu8JpgData;

  pu8JpgData = (RKADK_U8 *)RK_MPI_MB_Handle2VirAddr(pstCtx->stStream.pstPack->pMbBlk);
  memset(&stData, 0, sizeof(RKADK_PHOTO_RECV_DATA_S));
  stData.userdata = pstCtx->pHandle->userdata;

  if (!pstCtx->bGetThumb) {
    ret = RK_MPI_VENC_GetStream(pstCtx->ptsThumbCfg->photo_venc_chn, &pstCtx->stThumbFrame, 1000);
    if (ret == RK_SUCCESS) {
      if (pstCtx->stThumbFrame.pstPack->u32Len + 1024 < 65535) { //exif max len: 0xFFFF
        stData.u32DataLen = ThumbnailPhotoData(pu8JpgData, pstCtx->stStream.pstPack->u32Len, pstCtx->stThumbFrame, pstCtx->pu8Photo);
        stData.pu8DataBuf = pstCtx->pu8Photo;
      } else {
        stData.u32DataLen = pstCtx->stStream.pstPack->u32Len;
        stData.pu8DataBuf = pu8JpgData;
      }
      stData.u32CamId = pstCtx->pHandle->u32CamId;
      stData.bStreamEnd = pstCtx->stStream.pstPack->bStreamEnd;
      pstCtx->pHandle->pDataRecvFn(&stData);

      ret = RK_MPI_VENC_ReleaseStream(pstCtx->ptsThumbCfg->photo_venc_chn, &pstCtx->stThumbFrame);
      if (ret != RK_SUCCESS)
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);

      RK_MPI_VENC_ResetChn(pstCtx->ptsThumbCfg->photo_venc_chn);
    } else {
      RKADK_LOGW("Get thumb venc frame failed[%x]", ret);
      RK_MPI_VENC_StopRecvFrame(pstCtx->ptsThumbCfg->photo_venc_chn);
      stData.pu8DataBuf = pu8JpgData;
      stData.u32DataLen = pstCtx->stStream.pstPack->u32Len;
      stData.u32CamId = pstCtx->pHandle->u32CamId;
      stData.bStreamEnd = pstCtx->stStream.pstPack->bStreamEnd;
      pstCtx->pHandle->pDataRecvFn(&stData);
    }
    pstCtx->bGetThumb = true;
  } else {
    stData.pu8DataBuf = pu8JpgData;
    stData.u32DataLen = pstCtx->stStream.pstPack->u32Len;
    stData.u32CamId = pstCtx->pHandle->u32CamId;
    stData.bStreamEnd = pstCtx->stStream.pstPack->bStreamEnd;
    pstCtx->pHandle->pDataRecvFn(&stData);
  }

  if (stData.bStreamEnd) {
    pstCtx->bGetThumb = false;
    RKADK_LOGD("Photo success, seq = %d, len = %d", pstCtx->stStream.u32Seq, pstCtx->stStream.pstPack->u32Len);
    RKADK_BUFINFO("take photo[%d]", pstCtx->pstPhotoCfg->venc_chn);
  }

  return 0;
}
static void *RKADK_PHOTO_SliceProc(void *params) {
#ifdef JPEG_SLICE
  int ret, i = 0;
  int srcOffsetY = 0, srcOffsetUV = 0;
  void *pSrcData = NULL, *pDstBuf = NULL;
  RKADK_PHOTO_SLICE_CTX_S stCtx;
  VIDEO_FRAME_INFO_S stViFrame, stFrame;
  MB_BLK pMbBlk = NULL;
  RKADK_SCALER_PARAMS stScalerParam;
  RKADK_FORMAT_E format;

#ifdef JPEG_SLICE_WRITE
  FILE *fileSrc = NULL;
  char sliceSrcPath[128];
  FILE *file = NULL;
  char slicePath[128];
  static int frameCnt = 0;
#endif

#ifdef FULL_IMAGE_TEST
  char *imageBuf = NULL;
  int imageSize;
  int imageOffsetY = 0, imageOffsetUV = 0;
#endif

  ret = RKADK_PHOTO_SliceInit(&stCtx, params, &format);
  if (ret)
    return NULL;

#ifdef FULL_IMAGE_TEST
  imageSize = stCtx.pstPhotoCfg->image_width * stCtx.pstPhotoCfg->image_height * 1.5;
  imageBuf = (char *)malloc(imageSize);
  imageOffsetUV = stCtx.pstPhotoCfg->image_width * stCtx.pstPhotoCfg->image_height;
#endif

  memset(&stScalerParam, 0, sizeof(RKADK_SCALER_PARAMS));
  stScalerParam.nMethodLuma = 1; //SCALER_METHOD_BILINEAR;
  stScalerParam.nMethodChrm = 0; //SCALER_METHOD_NEAREST;
  stScalerParam.nCores = 1;
  stScalerParam.nSrcFmt = format;
  stScalerParam.pSrcBufs[2] = NULL;
  stScalerParam.nDstFmt = format;
  stScalerParam.pDstBufs[2] = NULL;

  ret = RKADK_ScalerInit(&(stScalerParam.context), stScalerParam.nCores);
  if (ret) {
    RKADK_LOGE("Init scaler context failed[%d]", ret);
    return NULL;
  }

  memset(&stViFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

  while (stCtx.pHandle->bGetJpeg) {
    ret = RK_MPI_VI_GetChnFrame(stCtx.pHandle->u32CamId, stCtx.pstPhotoCfg->vi_attr.u32ViChn, &stViFrame, 1000);
    if (ret == RK_SUCCESS) {
      if (stCtx.pHandle->u32PhotoCnt > 0) {
#if RKADK_PHOTO_SLICE_TIME_LOG
        RKADK_U64 frameStartUs = RKADK_PHOTO_GetTimeUs();
#endif
        if (stCtx.pHandle->stSliceParam.stViSlice.s32Witdh != stViFrame.stVFrame.u32VirWidth)
          RKADK_PHOTO_SetViSliceParam(stCtx.pHandle, stCtx.pstPhotoCfg, stViFrame.stVFrame.u32VirWidth,
                                      stViFrame.stVFrame.u32VirHeight);

        pSrcData = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
        srcOffsetY = 0;
        srcOffsetUV = stViFrame.stVFrame.u32VirWidth * stViFrame.stVFrame.u32VirHeight;

#ifdef JPEG_SLICE_WRITE
        if (!file) {
          memset(slicePath, 0, 128);
          sprintf(slicePath, "/data/slice_%d.yuv", frameCnt);
          file = fopen(slicePath, "w");
          if (!file)
            RKADK_LOGD("Craete %s failed", slicePath);
          else
            RKADK_LOGD("Create %s success", slicePath);
        }

#ifdef FULL_IMAGE_TEST
        imageOffsetY = 0;
        imageOffsetUV = stCtx.pstPhotoCfg->image_width * stCtx.pstPhotoCfg->image_height;
#endif
#endif

        for (i = 1; i <= (int)stCtx.pHandle->stSliceParam.u32SliceCount; i++) {
#ifdef JPEG_SLICE_WRITE
          if (!fileSrc) {
            memset(sliceSrcPath, 0, 128);
            sprintf(sliceSrcPath, "/data/slice_src_%d.yuv", i);
            fileSrc = fopen(sliceSrcPath, "w");
            if (!fileSrc)
              RKADK_LOGD("Craete src file %s failed", sliceSrcPath);
            else
              RKADK_LOGD("Create src file %s success", sliceSrcPath);
          }
#endif

          stScalerParam.nCallCnt = i;
          if (stScalerParam.nCallCnt == (RKADK_S32)stCtx.pHandle->stSliceParam.u32SliceCount) {
            stScalerParam.nSrcWid = stCtx.pHandle->stSliceParam.stViSlice.s32LastWidth;
            stScalerParam.nSrcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32LastHeight;
            stScalerParam.nDstWid = stCtx.pHandle->stSliceParam.stVencSlice.s32LastWidth;
            stScalerParam.nDstHgt = stCtx.pHandle->stSliceParam.stVencSlice.s32LastHeight;
          } else {
            stScalerParam.nSrcWid = stCtx.pHandle->stSliceParam.stViSlice.s32Witdh;

            if (i % 2)
              stScalerParam.nSrcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32Height;
            else
              stScalerParam.nSrcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32EvenHeight;

            stScalerParam.nDstWid = stCtx.pHandle->stSliceParam.stVencSlice.s32Witdh;
            stScalerParam.nDstHgt = stCtx.pHandle->stSliceParam.stVencSlice.s32Height;
          }
          //Y
          stScalerParam.nSrcWStrides[0] = stScalerParam.nSrcWid;
          stScalerParam.nSrcHStrides[0] = stScalerParam.nSrcHgt;
          stScalerParam.nDstWStrides[0] = stScalerParam.nDstWid;
          stScalerParam.nDstHStrides[0] = stScalerParam.nDstHgt;

          //UV
          stScalerParam.nSrcWStrides[1] = stScalerParam.nSrcWid;
          stScalerParam.nDstWStrides[1] = stScalerParam.nDstWid;
          if (format == RKADK_FMT_YUV420SP) {
            stScalerParam.nSrcHStrides[1] = stScalerParam.nSrcHgt / 2;
            stScalerParam.nDstHStrides[1] = stScalerParam.nDstHgt / 2;
          } else {
            stScalerParam.nSrcHStrides[1] = stScalerParam.nSrcHgt;
            stScalerParam.nDstHStrides[1] = stScalerParam.nDstHgt;
          }

          //Y+C mode
          stScalerParam.pSrcBufs[0] = (RKADK_U8*)pSrcData + srcOffsetY;
          stScalerParam.pSrcBufs[1] = (RKADK_U8*)pSrcData + srcOffsetUV;

          pMbBlk = RK_MPI_MB_GetMB(stCtx.vencMbPool, stCtx.u32DstBufSize, RK_TRUE);
          if (RK_NULL == pMbBlk) {
            RKADK_LOGE("RK_MPI_MB_GetMB failed");
            goto Exit;
          }

          pDstBuf = RK_MPI_MB_Handle2VirAddr(pMbBlk);
          stScalerParam.pDstBufs[0] = (RKADK_U8*)pDstBuf;
          stScalerParam.pDstBufs[1] = (RKADK_U8*)pDstBuf + stScalerParam.nDstWStrides[0] * stScalerParam.nDstHStrides[0];

#ifdef JPEG_SLICE_WRITE
          if (fileSrc) {
            fwrite(stScalerParam.pSrcBufs[0], 1, stScalerParam.nSrcWStrides[0] * stScalerParam.nSrcHStrides[0], fileSrc);
            fwrite(stScalerParam.pSrcBufs[1], 1, stScalerParam.nSrcWStrides[1] * stScalerParam.nSrcHStrides[1], fileSrc);
            fsync(fileSrc);
            fclose(fileSrc);
            fileSrc = NULL;
            RKADK_LOGD("Close src file: %s", sliceSrcPath);
          }
#endif

#if RKADK_PHOTO_SLICE_TIME_LOG
          RKADK_U64 startUs = RKADK_PHOTO_GetTimeUs();
#endif
          /* call scaler processor */
          ret = RKADK_ScalerProcessor(&(stScalerParam));
          if (ret != 0) {
            RKADK_LOGE("RKADK_RkScalerProcessor failed[%d]", ret);
            RK_MPI_MB_ReleaseMB(pMbBlk);
            goto Exit;
          }

#if RKADK_PHOTO_SLICE_TIME_LOG
          RKADK_U64 endUs = RKADK_PHOTO_GetTimeUs();
          RKADK_U64 costUs = (endUs >= startUs) ? (endUs - startUs) : 0;
          RKADK_LOGD("YuvScaler slice cost: %llu us (%.3f ms), slice[%d/%d], src[%d,%d] -> dst[%d,%d]",
                     (unsigned long long)costUs, (double)costUs / 1000.0,
                     i, stCtx.pHandle->stSliceParam.u32SliceCount,
                     stScalerParam.nSrcWid, stScalerParam.nSrcHgt, stScalerParam.nDstWid, stScalerParam.nDstHgt);
#endif

          srcOffsetY += stScalerParam.nSrcWStrides[0] * stScalerParam.nSrcHStrides[0];
          srcOffsetUV += stScalerParam.nSrcWStrides[1] * stScalerParam.nSrcHStrides[1];
          RK_MPI_SYS_MmzFlushCache(pMbBlk, RK_FALSE);

#ifdef JPEG_SLICE_WRITE
#ifndef FULL_IMAGE_TEST
          if (file) {
            fwrite(pDstBuf, 1, stCtx.u32DstBufSize, file);
            fsync(file);
          }
#else
          memcpy(imageBuf + imageOffsetY, (RKADK_U8*)pDstBuf, stScalerParam.nDstWStrides[0] * stScalerParam.nDstHStrides[0]);
          memcpy(imageBuf + imageOffsetUV, (RKADK_U8*)pDstBuf + stScalerParam.nDstWStrides[0] * stScalerParam.nDstHStrides[0],
                  stScalerParam.nDstWStrides[1] * stScalerParam.nDstHStrides[1]);

          imageOffsetY += stScalerParam.nDstWStrides[0] * stScalerParam.nDstHStrides[0];
          imageOffsetUV += stScalerParam.nDstWStrides[1] * stScalerParam.nDstHStrides[1];
#endif
#endif

          //Send venc frame
          stFrame.stVFrame.pMbBlk = pMbBlk;
          stFrame.stVFrame.u32Width = stScalerParam.nDstWid;
          stFrame.stVFrame.u32Height = stScalerParam.nDstHgt;
          stFrame.stVFrame.u32VirWidth = stScalerParam.nDstWid;
          stFrame.stVFrame.u32VirHeight = stScalerParam.nDstHgt;
          stFrame.stVFrame.enPixelFormat = stCtx.pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
          stFrame.stVFrame.u64PTS = stViFrame.stVFrame.u64PTS + i - 1;
          if (stScalerParam.nCallCnt == (RKADK_S32)stCtx.pHandle->stSliceParam.u32SliceCount)
            stFrame.stVFrame.u32FrameFlag = FRAME_FLAG_SNAP_END;
          else
            stFrame.stVFrame.u32FrameFlag = 0;

          ret = RK_MPI_VENC_SendFrame(stCtx.pstPhotoCfg->venc_chn, &stFrame, -1);
          if (ret != RK_SUCCESS) {
            RKADK_LOGE("RK_MPI_VENC_SendFrame failed[%x]", ret);
            RK_MPI_MB_ReleaseMB(pMbBlk);
            goto Exit;
          }

          ret = RK_MPI_MB_ReleaseMB(pMbBlk);
          if (ret != RK_SUCCESS)
            RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);

          //Get jpeg
          ret = RK_MPI_VENC_GetStream(stCtx.pstPhotoCfg->venc_chn, &stCtx.stStream, -1);
          if (ret == RK_SUCCESS) {
            RKADK_PHOTO_SliceJpeg(&stCtx);

            ret = RK_MPI_VENC_ReleaseStream(stCtx.pstPhotoCfg->venc_chn, &stCtx.stStream);
            if (ret != RK_SUCCESS)
              RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);
          } else {
            RKADK_LOGE("RK_MPI_VENC_GetStream failed[%x]", ret);
            goto Exit;
          }
        }

#if RKADK_PHOTO_SLICE_TIME_LOG
        RKADK_U64 frameEndUs = RKADK_PHOTO_GetTimeUs();
        RKADK_U64 frameCostUs = (frameEndUs >= frameStartUs) ? (frameEndUs - frameStartUs) : 0;
        RKADK_LOGD("SliceProc vi frame cost: %llu us (%.3f ms), slice_cnt=%u, vi[%u,%u], venc_dst[%u,%u]",
                   (unsigned long long)frameCostUs, (double)frameCostUs / 1000.0,
                   stCtx.pHandle->stSliceParam.u32SliceCount,
                   stViFrame.stVFrame.u32VirWidth, stViFrame.stVFrame.u32VirHeight,
                   stCtx.pstPhotoCfg->image_width, stCtx.pstPhotoCfg->image_height);
#endif

#ifdef JPEG_SLICE_WRITE
        if (file) {
#ifdef FULL_IMAGE_TEST
          fwrite(imageBuf, 1, imageSize, file);
#endif
          fclose(file);
          file = NULL;
          RKADK_LOGD("Close slice path: %s", slicePath);

          frameCnt++;
        }
#endif

        stCtx.pHandle->u32PhotoCnt -= 1;
      }

      ret = RK_MPI_VI_ReleaseChnFrame(stCtx.pHandle->u32CamId, stCtx.pstPhotoCfg->vi_attr.u32ViChn, &stViFrame);
      if (ret)
        RKADK_LOGE("RK_MPI_VI_ReleaseChnFrame failed[%x]", ret);
    } else {
      RKADK_LOGE("RK_MPI_VI_GetChnFrame failed[%x]", ret);
    }
  }

Exit:
  //release the context
  ret = RKADK_ScalerDeinit(stScalerParam.context);
  if (ret)
    RKADK_LOGE("Deinit scaler context failed[%d]", ret);

#ifdef FULL_IMAGE_TEST
  if (imageBuf)
    free(imageBuf);
#endif
  RKADK_PHOTO_SliceDeinit(&stCtx);

  RKADK_LOGD("Exit jpeg slice thread");
  return NULL;
#else
  RKADK_LOGE("No define JPEG_SLICE macro");
  return NULL;
#endif
}

static void *RKADK_PHOTO_TdeSliceProc(void *params) {
#ifdef JPEG_SLICE
  int ret, i = 0;
  RKADK_S32 srcYPos = 0;
  RKADK_PHOTO_SLICE_CTX_S stCtx;
  VIDEO_FRAME_INFO_S stViFrame, stFrame;
  MB_BLK pMbBlk = NULL;
  RKADK_BOOL bTdeOpened = RK_FALSE;

  TDE_HANDLE hHandle = RK_ERR_TDE_INVALID_HANDLE;
  TDE_SURFACE_S stSrcSurface;
  TDE_RECT_S stSrcRect;
  TDE_SURFACE_S stDstSurface;
  TDE_RECT_S stDstRect;

  ret = RKADK_PHOTO_SliceInit(&stCtx, params, NULL);
  if (ret)
    return NULL;

  ret = RK_TDE_Open();
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_TDE_Open failed[%x]", ret);
    goto Exit;
  }
  bTdeOpened = RK_TRUE;

  memset(&stViFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

  while (stCtx.pHandle->bGetJpeg) {
    ret = RK_MPI_VI_GetChnFrame(stCtx.pHandle->u32CamId, stCtx.pstPhotoCfg->vi_attr.u32ViChn, &stViFrame, 1000);
    if (ret == RK_SUCCESS) {
      if (stCtx.pHandle->u32PhotoCnt > 0) {
#if RKADK_PHOTO_SLICE_TIME_LOG
        RKADK_U64 frameStartUs = RKADK_PHOTO_GetTimeUs();
#endif
        if (stCtx.pHandle->stSliceParam.stViSlice.s32Witdh != stViFrame.stVFrame.u32VirWidth)
          RKADK_PHOTO_SetViSliceParam(stCtx.pHandle, stCtx.pstPhotoCfg, stViFrame.stVFrame.u32VirWidth,
                                      stViFrame.stVFrame.u32VirHeight);

        srcYPos = 0;
        for (i = 1; i <= (int)stCtx.pHandle->stSliceParam.u32SliceCount; i++) {
          RKADK_S32 srcWid, srcHgt, dstWid, dstHgt;
          if ((RKADK_U32)i == stCtx.pHandle->stSliceParam.u32SliceCount) {
            srcWid = stCtx.pHandle->stSliceParam.stViSlice.s32LastWidth;
            srcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32LastHeight;
            dstWid = stCtx.pHandle->stSliceParam.stVencSlice.s32LastWidth;
            dstHgt = stCtx.pHandle->stSliceParam.stVencSlice.s32LastHeight;
          } else {
            srcWid = stCtx.pHandle->stSliceParam.stViSlice.s32Witdh;
            if (i % 2)
              srcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32Height;
            else
              srcHgt = stCtx.pHandle->stSliceParam.stViSlice.s32EvenHeight;
            dstWid = stCtx.pHandle->stSliceParam.stVencSlice.s32Witdh;
            dstHgt = stCtx.pHandle->stSliceParam.stVencSlice.s32Height;
          }

          pMbBlk = RK_MPI_MB_GetMB(stCtx.vencMbPool, stCtx.u32DstBufSize, RK_TRUE);
          if (RK_NULL == pMbBlk) {
            RKADK_LOGE("RK_MPI_MB_GetMB failed");
            goto Exit;
          }

          memset(&stSrcSurface, 0, sizeof(stSrcSurface));
          memset(&stDstSurface, 0, sizeof(stDstSurface));
          memset(&stSrcRect, 0, sizeof(stSrcRect));
          memset(&stDstRect, 0, sizeof(stDstRect));

          stSrcSurface.pMbBlk = stViFrame.stVFrame.pMbBlk;
          stSrcSurface.enColorFmt = stCtx.pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
          stSrcSurface.u32Width = stViFrame.stVFrame.u32VirWidth;
          stSrcSurface.u32Height = stViFrame.stVFrame.u32VirHeight;
          stSrcSurface.enComprocessMode = COMPRESS_MODE_NONE;

          stSrcRect.s32Xpos = 0;
          stSrcRect.s32Ypos = srcYPos;
          stSrcRect.u32Width = srcWid;
          stSrcRect.u32Height = srcHgt;

          stDstSurface.pMbBlk = pMbBlk;
          stDstSurface.enColorFmt = stCtx.pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
          stDstSurface.u32Width = dstWid;
          stDstSurface.u32Height = dstHgt;
          stDstSurface.enComprocessMode = COMPRESS_MODE_NONE;

          stDstRect.s32Xpos = 0;
          stDstRect.s32Ypos = 0;
          stDstRect.u32Width = dstWid;
          stDstRect.u32Height = dstHgt;

#if RKADK_PHOTO_SLICE_TIME_LOG
          RKADK_U64 tdeStartUs = RKADK_PHOTO_GetTimeUs();
#endif

          hHandle = RK_TDE_BeginJob();
          if (hHandle == RK_ERR_TDE_INVALID_HANDLE) {
            RKADK_LOGE("RK_TDE_BeginJob failed");
            RK_MPI_MB_ReleaseMB(pMbBlk);
            pMbBlk = NULL;
            goto Exit;
          }

          ret = RK_TDE_QuickResize(hHandle, &stSrcSurface, &stSrcRect, &stDstSurface, &stDstRect);
          if (ret != RK_SUCCESS) {
            RKADK_LOGE("RK_TDE_QuickResize failed[%x]", ret);
            RK_TDE_CancelJob(hHandle);
            hHandle = RK_ERR_TDE_INVALID_HANDLE;
            RK_MPI_MB_ReleaseMB(pMbBlk);
            pMbBlk = NULL;
            goto Exit;
          }

          ret = RK_TDE_EndJob(hHandle, RK_FALSE, RK_TRUE, 10);
          if (ret != RK_SUCCESS) {
            RKADK_LOGE("RK_TDE_EndJob failed[%x]", ret);
            RK_TDE_CancelJob(hHandle);
            hHandle = RK_ERR_TDE_INVALID_HANDLE;
            RK_MPI_MB_ReleaseMB(pMbBlk);
            pMbBlk = NULL;
            goto Exit;
          }

          ret = RK_TDE_WaitForDone(hHandle);
          if (ret != RK_SUCCESS)
            RKADK_LOGW("RK_TDE_WaitForDone failed[%x]", ret);
          hHandle = RK_ERR_TDE_INVALID_HANDLE;

#if RKADK_PHOTO_SLICE_TIME_LOG
          RKADK_U64 tdeEndUs = RKADK_PHOTO_GetTimeUs();
          RKADK_U64 tdeCostUs = (tdeEndUs >= tdeStartUs) ? (tdeEndUs - tdeStartUs) : 0;
          RKADK_LOGD("TDE slice cost: %llu us (%.3f ms), slice[%d/%d], src[%d,%d]@y=%d -> dst[%d,%d]",
                     (unsigned long long)tdeCostUs, (double)tdeCostUs / 1000.0,
                     i, stCtx.pHandle->stSliceParam.u32SliceCount,
                     srcWid, srcHgt, srcYPos, dstWid, dstHgt);
#endif

          srcYPos += srcHgt;
          RK_MPI_SYS_MmzFlushCache(pMbBlk, RK_FALSE);

          //Send venc frame
          stFrame.stVFrame.pMbBlk = pMbBlk;
          stFrame.stVFrame.u32Width = dstWid;
          stFrame.stVFrame.u32Height = dstHgt;
          stFrame.stVFrame.u32VirWidth = dstWid;
          stFrame.stVFrame.u32VirHeight = dstHgt;
          stFrame.stVFrame.enPixelFormat = stCtx.pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
          stFrame.stVFrame.u64PTS = stViFrame.stVFrame.u64PTS + i - 1;
          if ((RKADK_U32)i == stCtx.pHandle->stSliceParam.u32SliceCount)
            stFrame.stVFrame.u32FrameFlag = FRAME_FLAG_SNAP_END;
          else
            stFrame.stVFrame.u32FrameFlag = 0;

          ret = RK_MPI_VENC_SendFrame(stCtx.pstPhotoCfg->venc_chn, &stFrame, -1);
          if (ret != RK_SUCCESS) {
            RKADK_LOGE("RK_MPI_VENC_SendFrame failed[%x]", ret);
            RK_MPI_MB_ReleaseMB(pMbBlk);
            pMbBlk = NULL;
            goto Exit;
          }

          ret = RK_MPI_MB_ReleaseMB(pMbBlk);
          pMbBlk = NULL;
          if (ret != RK_SUCCESS)
            RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);

          //Get jpeg
          ret = RK_MPI_VENC_GetStream(stCtx.pstPhotoCfg->venc_chn, &stCtx.stStream, -1);
          if (ret == RK_SUCCESS) {
            RKADK_PHOTO_SliceJpeg(&stCtx);

            ret = RK_MPI_VENC_ReleaseStream(stCtx.pstPhotoCfg->venc_chn, &stCtx.stStream);
            if (ret != RK_SUCCESS)
              RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);
          } else {
            RKADK_LOGE("RK_MPI_VENC_GetStream failed[%x]", ret);
            goto Exit;
          }
        }

#if RKADK_PHOTO_SLICE_TIME_LOG
        RKADK_U64 frameEndUs = RKADK_PHOTO_GetTimeUs();
        RKADK_U64 frameCostUs = (frameEndUs >= frameStartUs) ? (frameEndUs - frameStartUs) : 0;
        RKADK_LOGD("TdeSliceProc vi frame cost: %llu us (%.3f ms), slice_cnt=%u, vi[%u,%u], venc_dst[%u,%u]",
                   (unsigned long long)frameCostUs, (double)frameCostUs / 1000.0,
                   stCtx.pHandle->stSliceParam.u32SliceCount,
                   stViFrame.stVFrame.u32VirWidth, stViFrame.stVFrame.u32VirHeight,
                   stCtx.pstPhotoCfg->image_width, stCtx.pstPhotoCfg->image_height);
#endif

        stCtx.pHandle->u32PhotoCnt -= 1;
      }

      ret = RK_MPI_VI_ReleaseChnFrame(stCtx.pHandle->u32CamId, stCtx.pstPhotoCfg->vi_attr.u32ViChn, &stViFrame);
      if (ret)
        RKADK_LOGE("RK_MPI_VI_ReleaseChnFrame failed[%x]", ret);
    } else {
      RKADK_LOGE("RK_MPI_VI_GetChnFrame failed[%x]", ret);
    }
  }

Exit:
  if (hHandle != RK_ERR_TDE_INVALID_HANDLE)
    RK_TDE_CancelJob(hHandle);

  if (bTdeOpened)
    RK_TDE_Close();
  RKADK_PHOTO_SliceDeinit(&stCtx);

  RKADK_LOGD("Exit jpeg tde slice thread");
  return NULL;
#else
  RKADK_LOGE("No define JPEG_SLICE macro");
  return NULL;
#endif
}

static void *RKADK_PHOTO_GetJpeg(void *params) {
  int ret;
  VENC_STREAM_S stFrame, stThumbFrame;
  VENC_PACK_S stPack, stThumbPack;
  RKADK_PHOTO_RECV_DATA_S stData;
  RKADK_U8 *pu8JpgData;
  RKADK_U32 u32PhotoLen;
  RKADK_U8 *pu8Photo = NULL;
  RKADK_CHAR mapPath[RKADK_MAX_FILE_PATH_LEN];

  RKADK_PHOTO_HANDLE_S *pHandle = (RKADK_PHOTO_HANDLE_S *)params;
  if (!pHandle) {
    RKADK_LOGE("Get jpeg thread invalid param");
    return NULL;
  }

  if (!pHandle->pDataRecvFn) {
    RKADK_LOGE("u32CamId[%d] don't register callback", pHandle->u32CamId);
    return NULL;
  }

  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = RKADK_PARAM_GetThumbCfg(pHandle->u32CamId);
  if (!ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return NULL;
  }

  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(pHandle->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return NULL;
  }

  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return NULL;
  }

  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;

  memset(&stThumbFrame, 0, sizeof(stThumbFrame));
  memset(&stThumbPack, 0, sizeof(stThumbPack));
  stThumbFrame.pstPack = &stThumbPack;

  u32PhotoLen = pstPhotoCfg->max_width * pstPhotoCfg->max_height
                + ptsThumbCfg->thumb_width * ptsThumbCfg->thumb_height;

  snprintf(mapPath, RKADK_MAX_FILE_PATH_LEN, "%s_%d.jpeg", JPG_MMAP_FILE_PATH, pstPhotoCfg->venc_chn);
  pu8Photo = RKADK_PHOTO_Mmap(mapPath, u32PhotoLen);
  if (!pu8Photo)
    return NULL;

  while (pHandle->bGetJpeg) {
    ret = RK_MPI_VENC_GetStream(pstPhotoCfg->venc_chn, &stFrame, 1000);
    if (ret == RK_SUCCESS) {
      pu8JpgData = (RKADK_U8 *)RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
      memset(&stData, 0, sizeof(RKADK_PHOTO_RECV_DATA_S));
      stData.userdata = pHandle->userdata;

      ret = RK_MPI_VENC_GetStream(ptsThumbCfg->photo_venc_chn, &stThumbFrame, 1000);
      if (ret == RK_SUCCESS) {
        if (stThumbFrame.pstPack->u32Len + 1024 < 65535) { //exif max len: 0xFFFF
          stData.u32DataLen = ThumbnailPhotoData(pu8JpgData, stFrame.pstPack->u32Len, stThumbFrame, pu8Photo);
          stData.pu8DataBuf = pu8Photo;
        } else {
          stData.u32DataLen = stFrame.pstPack->u32Len;
          stData.pu8DataBuf = pu8JpgData;
        }
        stData.u32CamId = pHandle->u32CamId;
        stData.bStreamEnd = true;
        pHandle->pDataRecvFn(&stData);

        ret = RK_MPI_VENC_ReleaseStream(ptsThumbCfg->photo_venc_chn, &stThumbFrame);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);

      } else {
        RKADK_LOGW("Get thumb frame failed[%x]", ret);
        system("cat /proc/vcodec/enc/venc_info");
        stData.pu8DataBuf = pu8JpgData;
        stData.u32DataLen = stFrame.pstPack->u32Len;
        stData.u32CamId = pHandle->u32CamId;
        stData.bStreamEnd = true;
        pHandle->pDataRecvFn(&stData);
      }

      RKADK_LOGD("Photo success, seq = %d, len = %d", stFrame.u32Seq, stFrame.pstPack->u32Len);
      RKADK_BUFINFO("take photo[%d]", pstPhotoCfg->venc_chn);
      ret = RK_MPI_VENC_ReleaseStream(pstPhotoCfg->venc_chn, &stFrame);
      if (ret != RK_SUCCESS)
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);

      pHandle->u32PhotoCnt--;
      if (pHandle->u32PhotoCnt == 0) {
        if (pHandle->bFmtChange) {
          RK_MPI_VENC_StopRecvFrame(pHandle->stFmtChange.u32VencChn);
          RK_MPI_VENC_ResetChn(pHandle->stFmtChange.u32VencChn);
        }

        RK_MPI_VENC_StopRecvFrame(pstPhotoCfg->venc_chn);
        RK_MPI_VENC_ResetChn(pstPhotoCfg->venc_chn);
        RK_MPI_VENC_StopRecvFrame(ptsThumbCfg->photo_venc_chn);
        RK_MPI_VENC_ResetChn(ptsThumbCfg->photo_venc_chn);
      }
    }
  }

  if (pu8Photo)
    munmap(pu8Photo, u32PhotoLen);

  RKADK_LOGD("Exit get jpeg thread");
  return NULL;
}

static int RKADK_PHOTO_SetVencAttr(RKADK_PHOTO_HANDLE_S *pHandle,
                                    RKADK_PHOTO_THUMB_ATTR_S stThumbAttr,
                                    VENC_CHN_ATTR_S *pstVencAttr) {
  int ret;
  RKADK_U32 u32MaxWidth = 0, u32MaxHeight = 0;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;
  VENC_ATTR_JPEG_S *pstAttrJpege = &(pstVencAttr->stVencAttr.stAttrJpege);

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(pHandle->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  if (pstPhotoCfg->jpeg_slice) {
    ret = RKADK_PHOTO_SetSliceParam(pHandle, pstSensorCfg, pstPhotoCfg);
    if (ret)
      return -1;

    RKADK_PHOTO_SetViSliceParam(pHandle, pstPhotoCfg, pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width,
                                pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height);

    u32MaxWidth = pstPhotoCfg->max_slice_width;
    u32MaxHeight = pstPhotoCfg->max_slice_height;
  } else {
    u32MaxWidth = pstPhotoCfg->max_width;
    u32MaxHeight = pstPhotoCfg->max_height;
  }

  memset(pstVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
  pstVencAttr->stVencAttr.enType = RK_VIDEO_ID_JPEG;
  pstVencAttr->stVencAttr.enPixelFormat = pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
  pstVencAttr->stVencAttr.u32MaxPicWidth = u32MaxWidth;
  pstVencAttr->stVencAttr.u32MaxPicHeight = u32MaxHeight;
  pstVencAttr->stVencAttr.u32PicWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32PicHeight = pstPhotoCfg->image_height;
  pstVencAttr->stVencAttr.u32VirWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32VirHeight = pstPhotoCfg->image_height;
  pstVencAttr->stVencAttr.u32StreamBufCnt = 1;
  pstVencAttr->stVencAttr.u32BufSize = u32MaxWidth * u32MaxHeight;

  pstAttrJpege->bSupportDCF = (RK_BOOL)stThumbAttr.bSupportDCF;
  pstAttrJpege->stMPFCfg.u8LargeThumbNailNum =
      stThumbAttr.stMPFAttr.sCfg.u8LargeThumbNum;
  if (pstAttrJpege->stMPFCfg.u8LargeThumbNailNum >
      RKADK_MPF_LARGE_THUMB_NUM_MAX)
    pstAttrJpege->stMPFCfg.u8LargeThumbNailNum = RKADK_MPF_LARGE_THUMB_NUM_MAX;

  switch (stThumbAttr.stMPFAttr.eMode) {
  case RKADK_PHOTO_MPF_SINGLE:
    pstAttrJpege->enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    pstAttrJpege->stMPFCfg.astLargeThumbNailSize[0].u32Width =
        UPALIGNTO(stThumbAttr.stMPFAttr.sCfg.astLargeThumbSize[0].u32Width, 4);
    pstAttrJpege->stMPFCfg.astLargeThumbNailSize[0].u32Height =
        UPALIGNTO(stThumbAttr.stMPFAttr.sCfg.astLargeThumbSize[0].u32Height, 2);
    break;
  case RKADK_PHOTO_MPF_MULTI:
    pstAttrJpege->enReceiveMode = VENC_PIC_RECEIVE_MULTI;
    for (int i = 0; i < pstAttrJpege->stMPFCfg.u8LargeThumbNailNum; i++) {
      pstAttrJpege->stMPFCfg.astLargeThumbNailSize[i].u32Width = UPALIGNTO(
          stThumbAttr.stMPFAttr.sCfg.astLargeThumbSize[i].u32Width, 4);
      pstAttrJpege->stMPFCfg.astLargeThumbNailSize[i].u32Height = UPALIGNTO(
          stThumbAttr.stMPFAttr.sCfg.astLargeThumbSize[i].u32Height, 2);
    }
    break;
  default:
    pstAttrJpege->enReceiveMode = VENC_PIC_RECEIVE_BUTT;
    break;
  }

  return 0;
}

static void RKADK_PHOTO_CreateVencCombo(RKADK_S32 s32ChnId,
                                        VENC_CHN_ATTR_S *pstVencChnAttr,
                                        RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg) {
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_CHN_BUF_WRAP_S stVencChnBufWrap;
  VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
  VENC_COMBO_ATTR_S stComboAttr;
  VENC_JPEG_PARAM_S stJpegParam;
  memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
  memset(&stVencChnBufWrap, 0, sizeof(VENC_CHN_BUF_WRAP_S));
  memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
  memset(&stComboAttr, 0, sizeof(VENC_COMBO_ATTR_S));
  memset(&stJpegParam, 0, sizeof(stJpegParam));

  RK_MPI_VENC_CreateChn(s32ChnId, pstVencChnAttr);

  stVencChnBufWrap.bEnable = RK_TRUE;
  RK_MPI_VENC_SetChnBufWrapAttr(s32ChnId, &stVencChnBufWrap);

  stVencChnRefBufShare.bEnable = RK_TRUE;
  RK_MPI_VENC_SetChnRefBufShareAttr(s32ChnId, &stVencChnRefBufShare);

  stRecvParam.s32RecvPicNum = 1;
  RK_MPI_VENC_StartRecvFrame(s32ChnId, &stRecvParam);

  stComboAttr.bEnable = RK_TRUE;
  stComboAttr.s32ChnId = pstPhotoCfg->combo_venc_chn;
  RK_MPI_VENC_SetComboAttr(s32ChnId, &stComboAttr);

  stJpegParam.u32Qfactor = pstPhotoCfg->qfactor;
  RK_MPI_VENC_SetJpegParam(s32ChnId, &stJpegParam);
}

static void RKADK_PHOTO_SetVideoChn(RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg,
                                    RKADK_U32 u32CamId, MPP_CHN_S *pstViChn,
                                    MPP_CHN_S *pstVencChn, MPP_CHN_S *pstSrcVpssChn,
                                    MPP_CHN_S *pstDstVpssChn) {
  pstViChn->enModId = RK_ID_VI;
  pstViChn->s32DevId = u32CamId;
  pstViChn->s32ChnId = pstPhotoCfg->vi_attr.u32ViChn;

  pstSrcVpssChn->enModId = RK_ID_VPSS;
  pstSrcVpssChn->s32DevId = pstPhotoCfg->vpss_grp;
  pstSrcVpssChn->s32ChnId = pstPhotoCfg->vpss_chn;

  pstDstVpssChn->enModId = RK_ID_VPSS;
  pstDstVpssChn->s32DevId = pstPhotoCfg->vpss_grp;
  pstDstVpssChn->s32ChnId = 0; //When vpss is dst, chn is equal to 0

  pstVencChn->enModId = RK_ID_VENC;
  pstVencChn->s32DevId = 0;
  pstVencChn->s32ChnId = pstPhotoCfg->venc_chn;
}

static bool RKADK_PHOTO_EnableJpegSlice(RKADK_U32 u32CamId,
                                 RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg) {
  bool bJpegSlice = false;
  RKADK_U32 u32ViWidth = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width;
  RKADK_U32 u32ViHeight = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height;

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  RKADK_LOGI("Only RV1106_1103 or RV1103B or RV1126B support jpeg slice");
  return false;
#endif

  if (!pstPhotoCfg->jpeg_slice)
    return false;

#ifdef JPEG_SLICE
  if (pstPhotoCfg->image_width != u32ViWidth ||
      pstPhotoCfg->image_height != u32ViHeight) {
    RKADK_LOGD("In[%d, %d], Out[%d, %d]", u32ViWidth, u32ViHeight,
               pstPhotoCfg->image_width, pstPhotoCfg->image_height);
    bJpegSlice = true;
  }
#endif

  return bJpegSlice;
}

static bool RKADK_PHOTO_FormatChange(RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg) {
  //FBC
  if (pstPhotoCfg->vi_attr.stChnAttr.enCompressMode == COMPRESS_AFBC_16x16) {
    return true;
  }

  return false;
}

static bool RKADK_PHOTO_IsUseVpss(RKADK_U32 u32CamId,
                                 RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg, RKADK_U32 *u32VpssBufCnt) {
  RKADK_U32 u32ViWidth = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width;
  RKADK_U32 u32ViHeight = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height;

  if (pstPhotoCfg->jpeg_slice)
    return false;

  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return false;
  }

  if (pstPhotoCfg->image_width != u32ViWidth ||
      pstPhotoCfg->image_height != u32ViHeight) {
    RKADK_LOGD("In[%d, %d], Out[%d, %d]", u32ViWidth, u32ViHeight,
               pstPhotoCfg->image_width, pstPhotoCfg->image_height);
    if (u32VpssBufCnt)
      *u32VpssBufCnt = 1;
    return true;
  }

  if (pstPhotoCfg->enable_vpss) {
    if (u32VpssBufCnt)
      *u32VpssBufCnt = 1;
    return true;
  }

#ifdef RV1126_1109
  if (pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat == RK_FMT_YUV422SP) {
    if (u32VpssBufCnt)
      *u32VpssBufCnt = 1;
    return true;
  }
#endif

  if (pstPhotoCfg->post_aiisp) {
    if (u32VpssBufCnt)
      *u32VpssBufCnt = 0;
    return true;
  }

  return false;
}

static void RKADK_PHOTO_ResetAttr(RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg,
                                  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg,
                                  VENC_CHN_ATTR_S *pstVencAttr,
                                  VI_CHN_ATTR_S *pstViAttr,
                                  VPSS_CHN_ATTR_S *pstVpssAttr) {
  pstVencAttr->stVencAttr.u32PicWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32PicHeight = pstPhotoCfg->image_height;
  pstVencAttr->stVencAttr.u32VirWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32VirHeight = pstPhotoCfg->image_height;

  pstViAttr->stSize.u32Width = pstPhotoCfg->image_width;
  pstViAttr->stSize.u32Height = pstPhotoCfg->image_height;

  pstVpssAttr->u32Width = pstPhotoCfg->image_width;
  pstVpssAttr->u32Height = pstPhotoCfg->image_height;
}

static int RKADK_PHOTO_CreateVpssChn(RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg,
                                    RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg,
                                    RKADK_PHOTO_ATTR_S *pstPhotoAttr, RKADK_U32 u32VpssBufCnt) {
  int ret = 0;
  VPSS_GRP_ATTR_S stGrpAttr;
  VPSS_CHN_ATTR_S stChnAttr;

  memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));

  stGrpAttr.u32MaxW = pstSensorCfg->max_width;
  stGrpAttr.u32MaxH = pstSensorCfg->max_height;
  stGrpAttr.enPixelFormat = pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
  stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;
  stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
  stGrpAttr.stFrameRate.s32DstFrameRate = -1;
#ifdef RV1126B
  stGrpAttr.u32MaxQueue = 1;
#endif

  stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
  stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
  stChnAttr.enPixelFormat = pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
  stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stChnAttr.stFrameRate.s32DstFrameRate = -1;
  stChnAttr.u32Width = pstPhotoCfg->max_width;
  stChnAttr.u32Height = pstPhotoCfg->max_height;
  stChnAttr.u32Depth = 0;
  stChnAttr.u32FrameBufCnt = u32VpssBufCnt;
  if (u32VpssBufCnt)
    stChnAttr.enChnMode = VPSS_CHN_MODE_USER;
  else
    stChnAttr.enChnMode = VPSS_CHN_MODE_PASSTHROUGH;
  ret = RKADK_MPI_VPSS_Init(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn,
                            &stGrpAttr, &stChnAttr);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VPSS_Init vpss_grp[%d] vpss_chn[%d] falied[%x]",
                pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, ret);
    goto failed;
  }
  RKADK_BUFINFO("create vpss[%d, %d] end", pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn);

  ret = RK_MPI_VPSS_GetChnAttr(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, &stChnAttr);
  if (ret) {
    RKADK_LOGE("RK_MPI_VPSS_GetChnAttr vpss_grp[%d] vpss_chn[%d] falied[%x]",
                pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, ret);
    goto failed;
  }

  stChnAttr.u32Width = pstPhotoCfg->image_width;
  stChnAttr.u32Height = pstPhotoCfg->image_height;
  ret = RK_MPI_VPSS_SetChnAttr(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, &stChnAttr);
  if (ret) {
    RKADK_LOGE("RK_MPI_VPSS_SetChnAttr vpss_grp[%d] vpss_chn[%d] falied[%x]",
                pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, ret);
    goto failed;
  }

  if (pstPhotoCfg->post_aiisp)
    RKADK_MEDIA_EnablePostIsp(pstPhotoAttr->u32CamId, RKADK_STREAM_TYPE_SNAP, pstPhotoAttr->pstPostIspAttr);

  return 0;

failed:
  RKADK_MPI_VPSS_DeInit(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn);
  return ret;
}

static int RKADK_PHOTO_CreateFmtChangeChn(RKADK_PHOTO_HANDLE_S *pHandle, RKADK_U32 framerate) {
  int ret = 0;
  VENC_CHN_ATTR_S stVencAttr;
  VDEC_CHN_ATTR_S stVdecAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;
  RKADK_U32 u32BitRate = pHandle->stFmtChange.u32BitRate;
  RKADK_U32 u32Profile = pHandle->stFmtChange.u32Profile;
  VENC_RECV_PIC_PARAM_S stRecvParam;

  if (pHandle->stFmtChange.u32VencChn >= VENC_MAX_CHN_NUM) {
    RKADK_LOGE("Invalid FMT vencChn[%d], max value[%d]", pHandle->stFmtChange.u32VencChn, VENC_MAX_CHN_NUM);
    return -1;
  }

  if (pHandle->stFmtChange.u32VdecChn >= VDEC_MAX_CHN_NUM) {
    RKADK_LOGE("Invalid FMT vdecChn[%d], max value[%d]", pHandle->stFmtChange.u32VdecChn, VDEC_MAX_CHN_NUM);
    return -1;
  }

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  if (u32BitRate <= 0) {
    RKADK_LOGW("Use default bitrate[4M]");
    u32BitRate = 4 * 1024 * 1024;
  }

  if (u32Profile <= 0) {
    RKADK_LOGW("Use default profile[77]");
    u32Profile = 77;
  }

  memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
  stVencAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
  stVencAttr.stRcAttr.stH264Cbr.u32Gop = framerate;
  stVencAttr.stRcAttr.stH264Cbr.u32BitRate = u32BitRate / 1000;
  stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
  stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = framerate;
  stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
  stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = framerate;
  stVencAttr.stRcAttr.stH264FixQp.u32IQp = 20;
  stVencAttr.stRcAttr.stH264FixQp.u32PQp = 20;
  stVencAttr.stRcAttr.stH264FixQp.u32BQp = 20;
  stVencAttr.stVencAttr.enType = RK_VIDEO_ID_AVC;
  stVencAttr.stVencAttr.enPixelFormat = pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat;
  stVencAttr.stVencAttr.u32MaxPicWidth = pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width;
  stVencAttr.stVencAttr.u32MaxPicHeight = pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height;
  stVencAttr.stVencAttr.u32BufSize = stVencAttr.stVencAttr.u32MaxPicWidth * stVencAttr.stVencAttr.u32MaxPicHeight;
  stVencAttr.stVencAttr.u32PicWidth = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width;
  stVencAttr.stVencAttr.u32PicHeight = pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height;
  stVencAttr.stVencAttr.u32VirWidth = stVencAttr.stVencAttr.u32PicWidth;
  stVencAttr.stVencAttr.u32VirHeight = stVencAttr.stVencAttr.u32PicHeight;
  stVencAttr.stVencAttr.u32Profile = u32Profile;
  stVencAttr.stVencAttr.u32StreamBufCnt = 1;

  ret = RK_MPI_VENC_CreateChn(pHandle->stFmtChange.u32VencChn, &stVencAttr);
  if (ret) {
    RKADK_LOGE("Create FMT venc[%d] failed[%x]", pHandle->stFmtChange.u32VencChn, ret);
    return ret;
  }
  RKADK_LOGD("Create FMT Venc[%d]", pHandle->stFmtChange.u32VencChn);

  // must, for no streams callback running failed
  stRecvParam.s32RecvPicNum = 1;
  ret = RK_MPI_VENC_StartRecvFrame(pHandle->stFmtChange.u32VencChn, &stRecvParam);
  if (ret) {
    RKADK_LOGE("StartRecvFrame FMT VENC[%d] failed[%x]", pHandle->stFmtChange.u32VencChn, ret);
    RK_MPI_VENC_DestroyChn(pHandle->stFmtChange.u32VencChn);
    return ret;
  }

  memset(&stVdecAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  stVdecAttr.enMode = VIDEO_MODE_FRAME;
  stVdecAttr.enType = RK_VIDEO_ID_AVC;
  stVdecAttr.u32PicWidth = stVencAttr.stVencAttr.u32PicWidth;
  stVdecAttr.u32PicHeight = stVencAttr.stVencAttr.u32PicHeight;
  stVdecAttr.u32FrameBufCnt = 2;
  stVdecAttr.u32StreamBufCnt = 1;
#if defined(RV1106_1103) || defined(RV1103B)
  stVdecAttr.u32FrameBufDepth = 1;
#endif
  ret = RK_MPI_VDEC_CreateChn(pHandle->stFmtChange.u32VdecChn, &stVdecAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("create FMT vdec[%d] failed[%x]", pHandle->stFmtChange.u32VdecChn, ret);
    RK_MPI_VENC_DestroyChn(pHandle->stFmtChange.u32VencChn);
    return ret;
  }
  RKADK_LOGD("Create FMT Vdec[%d]", pHandle->stFmtChange.u32VdecChn);

  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
  stVdecParam.enType = RK_VIDEO_ID_AVC;
  stVdecParam.stVdecPictureParam.enPixelFormat = RK_FMT_YUV420SP;
  stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;
  ret = RK_MPI_VDEC_SetChnParam(pHandle->stFmtChange.u32VdecChn, &stVdecParam);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("set vdec chn[%d] param failed[%x]", pHandle->stFmtChange.u32VdecChn, ret);
    goto failed;
  }

  ret = RK_MPI_VDEC_StartRecvStream(pHandle->stFmtChange.u32VdecChn);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("start recv vdec[%d] failed[%x]", pHandle->stFmtChange.u32VdecChn, ret);
    goto failed;
  }

  return 0;

failed:
  RK_MPI_VENC_DestroyChn(pHandle->stFmtChange.u32VencChn);
  RK_MPI_VDEC_DestroyChn(pHandle->stFmtChange.u32VdecChn);
  return ret;
}

static int RKADK_PHOTO_CreateVideoChn(RKADK_PHOTO_HANDLE_S *pHandle, RKADK_PHOTO_ATTR_S *pstPhotoAttr,
                                    RKADK_U32 u32VpssBufCnt) {
  int ret = 0;
  VENC_CHN_ATTR_S stVencAttr;
  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pstPhotoAttr->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  ptsThumbCfg = RKADK_PARAM_GetThumbCfg(pstPhotoAttr->u32CamId);
  if (!ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(pstPhotoAttr->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  // Create VI
  ret = RKADK_MPI_VI_Init(pstPhotoAttr->u32CamId, pstPhotoCfg->vi_attr.u32ViChn,
                          &(pstPhotoCfg->vi_attr.stChnAttr));
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_Init failed[%x]", ret);
    return ret;
  }
  RKADK_BUFINFO("create vi[%d]", pstPhotoCfg->vi_attr.u32ViChn);

  // Create VPSS
  if (pHandle->bUseVpss) {
    ret = RKADK_PHOTO_CreateVpssChn(pstPhotoCfg, pstSensorCfg, pstPhotoAttr, u32VpssBufCnt);
    if (ret)
      goto failed;
  }

  // Create VENC
  ret = RKADK_PHOTO_SetVencAttr(pHandle, pstPhotoAttr->stThumbAttr, &stVencAttr);
  if (ret)
    goto failed;

  if (pstPhotoCfg->enable_combo) {
    RKADK_LOGD("Select combo mode");
    RKADK_PHOTO_CreateVencCombo(pstPhotoCfg->venc_chn, &stVencAttr, pstPhotoCfg);
    RKADK_BUFINFO("create venc[%d] combo", pstPhotoCfg->venc_chn);
  } else {
    ret = RK_MPI_VENC_CreateChn(pstPhotoCfg->venc_chn, &stVencAttr);
    if (ret) {
      RKADK_LOGE("Create Venc failed[%x]", ret);
      goto failed;
    }
    RKADK_LOGD("CamId[%d] venc[%d] create", pHandle->u32CamId, pstPhotoCfg->venc_chn);
    RKADK_BUFINFO("create venc[%d]", pstPhotoCfg->venc_chn);

    VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
    memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
    stVencChnRefBufShare.bEnable = RK_TRUE;
    RK_MPI_VENC_SetChnRefBufShareAttr(pstPhotoCfg->venc_chn, &stVencChnRefBufShare);

    VENC_JPEG_PARAM_S stJpegParam;
    memset(&stJpegParam, 0, sizeof(VENC_JPEG_PARAM_S));
    stJpegParam.u32Qfactor = pstPhotoCfg->qfactor;
    if (pHandle->stSliceParam.bJpegSlice)
      stJpegParam.u32MCUPerECS = pHandle->stSliceParam.u32MCUPerECS;
    RK_MPI_VENC_SetJpegParam(pstPhotoCfg->venc_chn, &stJpegParam);

    // must, for no streams callback running failed
    VENC_RECV_PIC_PARAM_S stRecvParam;
    stRecvParam.s32RecvPicNum = 1;
    ret = RK_MPI_VENC_StartRecvFrame(pstPhotoCfg->venc_chn, &stRecvParam);
    if (ret) {
      RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed[%x]", ret);
      goto failed;
    }
  }

  if (pHandle->bFmtChange) {
    ret = RKADK_PHOTO_CreateFmtChangeChn(pHandle, pstSensorCfg->framerate);
    if (ret) {
      RKADK_LOGE("Create format chaneg chn failed");
      return ret;
    }
  }

  // must be before bind, interrupt data flow
  RK_MPI_VENC_StopRecvFrame(pstPhotoCfg->venc_chn);
  RK_MPI_VENC_ResetChn(pstPhotoCfg->venc_chn);
  if (pHandle->bFmtChange) {
    RK_MPI_VENC_StopRecvFrame(pHandle->stFmtChange.u32VencChn);
    RK_MPI_VENC_ResetChn(pHandle->stFmtChange.u32VencChn);
  }

  ret = ThumbnailInit(pstPhotoAttr->u32CamId, RKADK_THUMB_MODULE_PHOTO, ptsThumbCfg);
  if (ret) {
    RKADK_LOGE("Thumbnail venc [%d] Init failed[%x]",
                ptsThumbCfg->photo_venc_chn, ret);
    goto failed;
  }

#ifndef THUMB_NORMAL
  ThumbnailChnBind(pstPhotoCfg->venc_chn, ptsThumbCfg->photo_venc_chn);
#endif

  //if use isp, set mirror/flip using aiq
  if (!pstSensorCfg->used_isp) {
    RKADK_STREAM_TYPE_E enStrmType = RKADK_STREAM_TYPE_SNAP;
    if (pstSensorCfg->mirror)
      RKADK_MEDIA_ToggleVencMirror(pstPhotoAttr->u32CamId, enStrmType, pstSensorCfg->mirror);
    if (pstSensorCfg->flip)
      RKADK_MEDIA_ToggleVencFlip(pstPhotoAttr->u32CamId, enStrmType, pstSensorCfg->flip);
  }

  return 0;

failed:
  RK_MPI_VENC_DestroyChn(pstPhotoCfg->venc_chn);

  if (pHandle->bUseVpss)
    RKADK_MPI_VPSS_DeInit(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn);

  RKADK_MPI_VI_DeInit(pstPhotoAttr->u32CamId, pstPhotoCfg->vi_attr.u32ViChn);
  return ret;
}

static int RKADK_PHOTO_DestoryVideoChn(RKADK_PHOTO_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = NULL;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  ptsThumbCfg = RKADK_PARAM_GetThumbCfg(pHandle->u32CamId);
  if (!ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  // Destory VENC
  ret = RK_MPI_VENC_DestroyChn(pstPhotoCfg->venc_chn);
  if (ret) {
    RKADK_LOGE("Destory VENC[%d] failed[%x]", pstPhotoCfg->venc_chn, ret);
    return ret;
  }
  RKADK_LOGD("CamId[%d] venc[%d] destory ok", pHandle->u32CamId, pstPhotoCfg->venc_chn);

  // Destory format change chn
  if (pHandle->bFmtChange) {
    ret = RK_MPI_VDEC_StopRecvStream(pHandle->stFmtChange.u32VdecChn);
    if (ret) {
      RKADK_LOGE("StopRecvFrame FMT VDEC[%d] failed[%x]", pHandle->stFmtChange.u32VdecChn, ret);
      return ret;
    }

    ret = RK_MPI_VDEC_DestroyChn(pHandle->stFmtChange.u32VdecChn);
    if (ret) {
      RKADK_LOGE("Destory FMT VDEC[%d] failed[%x]", pHandle->stFmtChange.u32VdecChn, ret);
      return ret;
    }
    RKADK_LOGD("Destory FMT Vdec[%d]", pHandle->stFmtChange.u32VdecChn);

    ret = RK_MPI_VENC_StopRecvFrame(pHandle->stFmtChange.u32VencChn);
    if (ret) {
      RKADK_LOGE("StopRecvFrame FMT VENC[%d] failed[%d]", pHandle->stFmtChange.u32VencChn, ret);
      return ret;
    }

    ret = RK_MPI_VENC_DestroyChn(pHandle->stFmtChange.u32VencChn);
    if (ret) {
      RKADK_LOGE("DeInit FMT VENC[%d] failed[%x]", pHandle->stFmtChange.u32VencChn, ret);
      return ret;
    }
    RKADK_LOGD("Destory FMT Venc[%d]", pHandle->stFmtChange.u32VencChn);
  }

  // Destory VPSS
  if (pHandle->bUseVpss) {
    ret = RKADK_MPI_VPSS_DeInit(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn);
    if (ret) {
      RKADK_LOGE("DeInit VPSS[%d] failed[%x]", pstPhotoCfg->vpss_chn, ret);
      return ret;
    }
  }

  // Destory VI
  ret = RKADK_MPI_VI_DeInit(pHandle->u32CamId, pHandle->u32ViChn);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_DeInit failed[%x]", ret);
    return ret;
  }

  ret = ThumbnailDeInit(pHandle->u32CamId, RKADK_THUMB_MODULE_PHOTO, ptsThumbCfg);
  if (ret) {
    RKADK_LOGE("ThumbnailDeInit failed[%x]", ret);
    return ret;
  }

  return 0;
}

static int RKADK_PHOTO_BindChn(RKADK_PHOTO_HANDLE_S *pHandle) {
  int ret = 0;
  MPP_CHN_S stViChn, stVencChn, stSrcVpssChn, stDstVpssChn;
  MPP_CHN_S stFmtVencChn, stFmtVdecChn;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  RKADK_PHOTO_SetVideoChn(pstPhotoCfg, pHandle->u32CamId, &stViChn, &stVencChn,
                     &stSrcVpssChn, &stDstVpssChn);

  stFmtVencChn.enModId = RK_ID_VENC;
  stFmtVencChn.s32DevId = pHandle->u32CamId;
  stFmtVencChn.s32ChnId = pHandle->stFmtChange.u32VencChn;

  stFmtVdecChn.enModId = RK_ID_VDEC;
  stFmtVdecChn.s32DevId = pHandle->u32CamId;
  stFmtVdecChn.s32ChnId = pHandle->stFmtChange.u32VdecChn;

  if (pHandle->bUseVpss) {
    // VPSS Bind VENC
    ret = RKADK_MPI_SYS_Bind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("Bind VPSS[%d, %d] to VENC[%d] failed[%x]", stSrcVpssChn.s32DevId,
                 stSrcVpssChn.s32ChnId, stVencChn.s32ChnId, ret);
      return ret;
    }

    if (pHandle->bFmtChange) {
      // FMT VDEC Bind VPSS
      ret = RK_MPI_SYS_Bind(&stFmtVdecChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("Bind FMT VDEC[%d] to VPSS[%d, %d] failed[%x]", stFmtVdecChn.s32ChnId,
                   stDstVpssChn.s32DevId, stDstVpssChn.s32ChnId, ret);
        RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
        return ret;
      }
      RKADK_LOGD("Bind FMT VDEC[%d] to VPSS[%d, %d]", stFmtVdecChn.s32ChnId,
                 stDstVpssChn.s32DevId, stDstVpssChn.s32ChnId);

      // FMT VENC Bind FMT VDEC
      ret = RK_MPI_SYS_Bind(&stFmtVencChn, &stFmtVdecChn);
      if (ret) {
        RKADK_LOGE("Bind FMT VENC[%d] to FMT VDEC[%d] failed[%x]", stFmtVencChn.s32ChnId,
                    stFmtVdecChn.s32ChnId, ret);
        RK_MPI_SYS_UnBind(&stFmtVdecChn, &stDstVpssChn);
        RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
        return ret;
      }
      RKADK_LOGD("Bind FMT VENC[%d] to FMT VDEC[%d]", stFmtVencChn.s32ChnId, stFmtVdecChn.s32ChnId);

      // VI Bind FMT VENC
      ret = RK_MPI_SYS_Bind(&stViChn, &stFmtVencChn);
      if (ret) {
        RKADK_LOGE("Bind VI[%d] to FMT VENC[%d] failed[%x]", stViChn.s32ChnId, stFmtVencChn.s32ChnId, ret);
        RK_MPI_SYS_UnBind(&stFmtVencChn, &stFmtVdecChn);
        RK_MPI_SYS_UnBind(&stFmtVdecChn, &stDstVpssChn);
        RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
        return ret;
      }
      RKADK_LOGD("Bind VI[%d] to FMT VENC[%d]", stViChn.s32ChnId, stFmtVencChn.s32ChnId);
    } else {
      // VI Bind VPSS
      ret = RKADK_MPI_SYS_Bind(&stViChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("Bind VI[%d] to VPSS[%d, %d] failed[%x]", stViChn.s32ChnId,
                   stDstVpssChn.s32DevId, stDstVpssChn.s32ChnId, ret);
        RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
        return ret;
      }
    }
  } else {
    if (!pstPhotoCfg->enable_combo) {
      if (pHandle->bFmtChange) {
        // FMT VDEC Bind VENC
        ret = RK_MPI_SYS_Bind(&stFmtVdecChn, &stVencChn);
        if (ret) {
          RKADK_LOGE("Bind FMT VDEC[%d] to VENC[%d] failed[%x]", stFmtVdecChn.s32ChnId, stVencChn.s32ChnId, ret);
          return ret;
        }
        RKADK_LOGD("Bind FMT VDEC[%d] to VENC[%d]", stFmtVdecChn.s32ChnId, stVencChn.s32ChnId);

        // FMT VENC Bind FMT VDEC
        ret = RK_MPI_SYS_Bind(&stFmtVencChn, &stFmtVdecChn);
        if (ret) {
          RKADK_LOGE("Bind FMT VENC[%d] to FMT VDEC[%d] failed[%x]", stFmtVencChn.s32ChnId,
                      stFmtVdecChn.s32ChnId, ret);
          RK_MPI_SYS_UnBind(&stFmtVdecChn, &stVencChn);
          return ret;
        }
        RKADK_LOGD("Bind FMT VENC[%d] to FMT VDEC[%d]", stFmtVencChn.s32ChnId, stFmtVdecChn.s32ChnId);

        // VI Bind FMT VENC
        ret = RK_MPI_SYS_Bind(&stViChn, &stFmtVencChn);
        if (ret) {
          RKADK_LOGE("Bind VI[%d] to FMT VENC[%d] failed[%x]", stViChn.s32ChnId, stFmtVencChn.s32ChnId, ret);
          RK_MPI_SYS_UnBind(&stFmtVencChn, &stFmtVdecChn);
          RK_MPI_SYS_UnBind(&stFmtVdecChn, &stVencChn);
          return ret;
        }
        RKADK_LOGD("Bind VI[%d] to FMT VENC[%d]", stViChn.s32ChnId, stFmtVencChn.s32ChnId);
      } else {
        // VI Bind VENC
        ret = RKADK_MPI_SYS_Bind(&stViChn, &stVencChn);
        if (ret) {
          RKADK_LOGE("Bind VI[%d] to VENC[%d] failed[%x]", stViChn.s32ChnId,
                      stVencChn.s32ChnId, ret);
          return ret;
        }
      }
    }
  }

  // venc bind vdec modifies the venc stop state, which is restored here
  RK_MPI_VENC_StopRecvFrame(pstPhotoCfg->venc_chn);
  RK_MPI_VENC_ResetChn(pstPhotoCfg->venc_chn);
  if (pHandle->bFmtChange) {
    RK_MPI_VENC_StopRecvFrame(pHandle->stFmtChange.u32VencChn);
    RK_MPI_VENC_ResetChn(pHandle->stFmtChange.u32VencChn);
  }

  RKADK_BUFINFO("photo bind[%d, %d, %d]", stViChn.s32ChnId, stSrcVpssChn.s32ChnId, stVencChn.s32ChnId);
  return 0;
}

static int RKADK_PHOTO_UnBindChn(RKADK_PHOTO_HANDLE_S *pHandle) {
  int ret = 0;
  MPP_CHN_S stViChn, stVencChn, stSrcVpssChn, stDstVpssChn;
  MPP_CHN_S stFmtVencChn, stFmtVdecChn;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  RKADK_PHOTO_SetVideoChn(pstPhotoCfg, pHandle->u32CamId, &stViChn, &stVencChn,
                     &stSrcVpssChn, &stDstVpssChn);
  stViChn.s32ChnId = pHandle->u32ViChn;

  stFmtVencChn.enModId = RK_ID_VENC;
  stFmtVencChn.s32DevId = pHandle->u32CamId;
  stFmtVencChn.s32ChnId = pHandle->stFmtChange.u32VencChn;

  stFmtVdecChn.enModId = RK_ID_VDEC;
  stFmtVdecChn.s32DevId = pHandle->u32CamId;
  stFmtVdecChn.s32ChnId = pHandle->stFmtChange.u32VdecChn;

  if (pHandle->bUseVpss) {
    // VPSS UnBind VENC
    ret = RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("UnBind VPSS[%d] to VENC[%d] failed[%d]", stSrcVpssChn.s32ChnId,
                 stVencChn.s32ChnId, ret);
      return ret;
    }

    if (pHandle->bFmtChange) {
      // FMT VDEC UnBind VPSS
      ret = RK_MPI_SYS_UnBind(&stFmtVdecChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("UnBind FMT VDEC[%d] to VPSS[%d, %d] failed[%x]", stFmtVdecChn.s32ChnId,
                   stDstVpssChn.s32DevId, stDstVpssChn.s32ChnId, ret);
        return ret;
      }
      RKADK_LOGD("UnBind FMT VDEC[%d] to VPSS[%d, %d]", stFmtVdecChn.s32ChnId, stDstVpssChn.s32DevId, stDstVpssChn.s32ChnId);

      // FMT VENC UnBind FMT VDEC
      ret = RK_MPI_SYS_UnBind(&stFmtVencChn, &stFmtVdecChn);
      if (ret) {
        RKADK_LOGE("UnBind FMT VENC[%d] to FMT VDEC[%d] failed[%x]", stFmtVencChn.s32ChnId,
                    stFmtVdecChn.s32ChnId, ret);
        return ret;
      }
      RKADK_LOGD("UnBind FMT VENC[%d] to FMT VDEC[%d]", stFmtVencChn.s32ChnId, stFmtVdecChn.s32ChnId);

      // VI UnBind FMT VENC
      ret = RK_MPI_SYS_UnBind(&stViChn, &stFmtVencChn);
      if (ret) {
        RKADK_LOGE("UnBind VI[%d] to FMT VENC[%d] failed[%x]", stViChn.s32ChnId, stFmtVencChn.s32ChnId, ret);
        return ret;
      }
      RKADK_LOGD("UnBind VI[%d] to FMT VENC[%d]", stViChn.s32ChnId, stFmtVencChn.s32ChnId);
    } else {
      // VI UnBind VPSS
      ret = RKADK_MPI_SYS_UnBind(&stViChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("UnBind VI[%d] to VPSS[%d] failed[%d]", stViChn.s32ChnId,
                   stSrcVpssChn.s32ChnId, ret);
        return ret;
      }
    }
  } else {
    if (!pstPhotoCfg->enable_combo) {
      if (pHandle->bFmtChange) {
        // FMT VDEC UnBind VENC
        ret = RK_MPI_SYS_UnBind(&stFmtVdecChn, &stVencChn);
        if (ret) {
          RKADK_LOGE("UnBind FMT VDEC[%d] to VENC[%d] failed[%x]", stFmtVdecChn.s32ChnId, stVencChn.s32ChnId, ret);
          return ret;
        }
        RKADK_LOGD("UnBind FMT VDEC[%d] to VENC[%d]", stFmtVdecChn.s32ChnId, stVencChn.s32ChnId);

        // FMT VENC UnBind FMT VDEC
        ret = RK_MPI_SYS_UnBind(&stFmtVencChn, &stFmtVdecChn);
        if (ret) {
          RKADK_LOGE("UnBind FMT VENC[%d] to FMT VDEC[%d] failed[%x]", stFmtVencChn.s32ChnId,
                      stFmtVdecChn.s32ChnId, ret);
          return ret;
        }
        RKADK_LOGD("UnBind FMT VENC[%d] to FMT VDEC[%d]", stFmtVencChn.s32ChnId, stFmtVdecChn.s32ChnId);

        // VI UnBind FMT VENC
        ret = RK_MPI_SYS_UnBind(&stViChn, &stFmtVencChn);
        if (ret) {
          RKADK_LOGE("UnBind VI[%d] to FMT VENC[%d] failed[%x]", stViChn.s32ChnId, stFmtVencChn.s32ChnId, ret);
          return ret;
        }
        RKADK_LOGD("UnBind VI[%d] to FMT VENC[%d]", stViChn.s32ChnId, stFmtVencChn.s32ChnId);
      } else {
        // VI UnBind VENC
        ret = RKADK_MPI_SYS_UnBind(&stViChn, &stVencChn);
        if (ret) {
          RKADK_LOGE("UnBind VI[%d] to VENC[%d] failed[%d]", stViChn.s32ChnId,
                    stVencChn.s32ChnId, ret);
          return ret;
        }
      }
    }
  }

  return 0;
}

RKADK_S32 RKADK_PHOTO_Init(RKADK_PHOTO_ATTR_S *pstPhotoAttr, RKADK_MW_PTR *ppHandle) {
  int ret = 0;
  bool bSysInit = false;
  char name[RKADK_THREAD_NAME_LEN];
  RKADK_U32 u32VpssBufCnt = 0;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;
  RKADK_PHOTO_HANDLE_S *pHandle = NULL;

  RKADK_CHECK_POINTER(pstPhotoAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstPhotoAttr->u32CamId, RKADK_FAILURE);

  if (*ppHandle) {
    RKADK_LOGE("Photo[%d] has been initialized", pstPhotoAttr->u32CamId);
    return -1;
  }

  RKADK_LOGI("Photo[%d] Init Start...", pstPhotoAttr->u32CamId);
  RKADK_BUFINFO("enter photo[%d]", pstPhotoAttr->u32CamId);

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pstPhotoAttr->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  bSysInit = RKADK_MPI_SYS_CHECK();
  if (!bSysInit) {
    RKADK_LOGE("System is not initialized");
    return -1;
  }

  pHandle = (RKADK_PHOTO_HANDLE_S *)malloc(sizeof(RKADK_PHOTO_HANDLE_S));
  if (!pHandle) {
    RKADK_LOGE("malloc photo handle failed");
    return -1;
  }
  memset(pHandle, 0, sizeof(RKADK_PHOTO_HANDLE_S));
  pHandle->u32CamId = pstPhotoAttr->u32CamId;
  pHandle->pDataRecvFn = pstPhotoAttr->pfnPhotoDataProc;
  pHandle->userdata = pstPhotoAttr->userdata;
  pHandle->u32ViChn = pstPhotoCfg->vi_attr.u32ViChn;
  pHandle->bUseVpss = RKADK_PHOTO_IsUseVpss(pstPhotoAttr->u32CamId, pstPhotoCfg, &u32VpssBufCnt);
  pHandle->stSliceParam.bJpegSlice = RKADK_PHOTO_EnableJpegSlice(pstPhotoAttr->u32CamId, pstPhotoCfg);
  pHandle->bFmtChange = RKADK_PHOTO_FormatChange(pstPhotoCfg);
  memcpy(&pHandle->stFmtChange, &pstPhotoAttr->stFmtChange, sizeof(RKADK_PHOTO_FMT_CHANGE_S));

  ret = RKADK_PHOTO_CreateVideoChn(pHandle, pstPhotoAttr, u32VpssBufCnt);
  if (ret)
    goto failed;

  if (pHandle->stSliceParam.bJpegSlice) {
    pHandle->bGetJpeg = true;
    if (pstPhotoAttr->bUseTdeJpegSlice)
      ret = pthread_create(&pHandle->stSliceParam.sliceTid, NULL, RKADK_PHOTO_TdeSliceProc, pHandle);
    else
      ret = pthread_create(&pHandle->stSliceParam.sliceTid, NULL, RKADK_PHOTO_SliceProc, pHandle);
    if (ret) {
      RKADK_LOGE("Create slice(%d) thread failed [%d]", pstPhotoAttr->u32CamId, ret);
      goto failed;
    }
    snprintf(name, sizeof(name), "Slice_%d", pstPhotoCfg->venc_chn);
    pthread_setname_np(pHandle->stSliceParam.sliceTid, name);
  } else {
    ret = RKADK_PHOTO_BindChn(pHandle);
    if (ret)
      goto failed;

    pHandle->bGetJpeg = true;
    ret = pthread_create(&pHandle->tid, NULL, RKADK_PHOTO_GetJpeg, pHandle);
    if (ret) {
      RKADK_LOGE("Create get jpg(%d) thread failed [%d]", pstPhotoAttr->u32CamId, ret);
      goto failed;
    }
    snprintf(name, sizeof(name), "PhotoGetJpeg_%d", pstPhotoCfg->venc_chn);
    pthread_setname_np(pHandle->tid, name);
  }

  *ppHandle = (RKADK_MW_PTR)pHandle;
  RKADK_LOGI("Photo[%d] Init End...", pstPhotoAttr->u32CamId);
  return 0;

failed:
  pHandle->bGetJpeg = false;
  if (pHandle->stSliceParam.sliceTid) {
    ret = pthread_join(pHandle->stSliceParam.sliceTid, NULL);
    if (ret)
      RKADK_LOGE("Exit slice thread failed!");
    pHandle->stSliceParam.sliceTid = 0;
  }

  if (pHandle->tid) {
    ret = pthread_join(pHandle->tid, NULL);
    if (ret)
      RKADK_LOGE("Exit get jpeg thread failed!");
    pHandle->tid = 0;
  }

  if (pHandle)
    free(pHandle);

  return ret;
}

RKADK_S32 RKADK_PHOTO_DeInit(RKADK_MW_PTR pHandle) {
  int ret;
  RKADK_PHOTO_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_PHOTO_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

  RKADK_LOGI("Photo[%d] DeInit Start...", pstHandle->u32CamId);

  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      RKADK_PARAM_GetPhotoCfg(pstHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

#if 1
  // The current version cannot be forced to exit
  ret = RK_MPI_VENC_StopRecvFrame(pstPhotoCfg->venc_chn);
  if (ret) {
    RKADK_LOGE("StopRecvFrame VENC[%d] failed[%d]", pstPhotoCfg->venc_chn, ret);
    return ret;
  }
#else
  VENC_RECV_PIC_PARAM_S stRecvParam;
  stRecvParam.s32RecvPicNum = 1;
  ret = RK_MPI_VENC_StartRecvFrame(stVencChn.s32ChnId, &stRecvParam);
  if (ret) {
    RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed[%x]", ret);
    return ret;
  }
#endif

  pstHandle->bGetJpeg = false;
  if (pstHandle->tid) {
    ret = pthread_join(pstHandle->tid, NULL);
    if (ret)
      RKADK_LOGE("Exit get jpeg thread failed!");
    pstHandle->tid = 0;
  }

  if (pstHandle->stSliceParam.bJpegSlice) {
    if (pstHandle->stSliceParam.sliceTid) {
      ret = pthread_join(pstHandle->stSliceParam.sliceTid, NULL);
      if (ret)
        RKADK_LOGE("Exit slice thread failed!");
      pstHandle->stSliceParam.sliceTid = 0;
    }
  } else {
    ret = RKADK_PHOTO_UnBindChn(pstHandle);
    if (ret) {
      RKADK_LOGE("RKADK_PHOTO_UnBindChn failed");
      return ret;
    }
  }

  ret = RKADK_PHOTO_DestoryVideoChn(pstHandle);
  if (ret) {
    RKADK_LOGE("RKADK_PHOTO_DestoryVideoChn failed");
    return ret;
  }

  pstHandle->pDataRecvFn = NULL;
  RKADK_LOGI("Photo[%d] DeInit End...", pstHandle->u32CamId);

  if (pHandle) {
    free(pHandle);
    pHandle = NULL;
  }

  return 0;
}

RKADK_S32 RKADK_PHOTO_TakePhoto(RKADK_MW_PTR pHandle, RKADK_TAKE_PHOTO_ATTR_S *pstAttr) {
  int ret = 0;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  RKADK_PHOTO_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_PHOTO_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

  if (pstHandle->u32PhotoCnt > 0) {
    RKADK_LOGW("The last photo shoot wasn't over, u32PhotoCnt: %d", pstHandle->u32PhotoCnt);
    return 0;
  }

  if (pstHandle->bReseting) {
    RKADK_LOGW("Now reset, can't photos");
    return 0;
  }

  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      RKADK_PARAM_GetPhotoCfg(pstHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  if (pstAttr->enPhotoType == RKADK_PHOTO_TYPE_LAPSE) {
    // TODO
    RKADK_LOGE("nonsupport photo type = %d", pstAttr->enPhotoType);
    return -1;
  }

  memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
  if (pstAttr->enPhotoType == RKADK_PHOTO_TYPE_SINGLE)
    stRecvParam.s32RecvPicNum = 1;
  else
    stRecvParam.s32RecvPicNum = pstAttr->unPhotoTypeAttr.stMultipleAttr.s32Count;

  pstHandle->u32PhotoCnt = stRecvParam.s32RecvPicNum;
  if (pstHandle->stSliceParam.bJpegSlice)
    stRecvParam.s32RecvPicNum *= pstHandle->stSliceParam.u32SliceCount;

  RKADK_LOGI("Photo[%d] Take photo number = %d, s32RecvPicNum: %d", pstHandle->u32CamId, pstHandle->u32PhotoCnt, stRecvParam.s32RecvPicNum);

#ifndef THUMB_NORMAL
  ret = RK_MPI_VENC_StartRecvFrame(pstPhotoCfg->venc_chn, &stRecvParam);
#else
  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = RKADK_PARAM_GetThumbCfg(pstHandle->u32CamId);
  if (!ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  ret = RK_MPI_VENC_StartRecvFrame(pstPhotoCfg->venc_chn, &stRecvParam);
  if(ret) {
    RKADK_LOGE("StartRecvFrame venc[%d] failed[%x]", pstPhotoCfg->venc_chn, ret);
    return ret;
  }

  stRecvParam.s32RecvPicNum = pstHandle->u32PhotoCnt;
  if (pstHandle->bFmtChange) {
    ret = RK_MPI_VENC_StartRecvFrame(pstHandle->stFmtChange.u32VencChn, &stRecvParam);
    if(ret) {
      RKADK_LOGE("StartRecvFrame FMT venc[%d] failed[%x]", pstHandle->stFmtChange.u32VencChn, ret);
      return ret;
    }
  }

  ret = RK_MPI_VENC_StartRecvFrame(ptsThumbCfg->photo_venc_chn, &stRecvParam);
  if(ret) {
    RKADK_LOGE("Start recv thumb failed[%x]", ret);
    return ret;
  }
#endif

  return ret;
}

RKADK_S32 RKADK_PHOTO_Reset(RKADK_MW_PTR *pHandle) {
  int ret;
  bool bPhoto;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  MPP_CHN_S stViChn, stVencChn, stSrcVpssChn, stDstVpssChn;
  VENC_CHN_ATTR_S stVencAttr;
  VI_CHN_ATTR_S stViAttr;
  VPSS_CHN_ATTR_S stVpssAttr;
  RKADK_PHOTO_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(*pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_PHOTO_HANDLE_S *)*pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  RKADK_LOGE("nonsupport dynamic setting resolution, please recreate!");
  return -1;
#endif

  if (pstHandle->u32PhotoCnt > 0) {
    RKADK_LOGE("The last photo shoot wasn't over, u32PhotoCnt: %d", pstHandle->u32PhotoCnt);
    return -1;
  }

  if (pstHandle->bFmtChange) {
    RKADK_LOGE("enable format change, not support reset");
    return -1;
  }

  RKADK_LOGI("Photo[%d] Reset start...", pstHandle->u32CamId);

  pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(pstHandle->u32CamId);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(pstHandle->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  if (pstPhotoCfg->enable_combo) {
    RKADK_LOGE("Photo combo venc[%d], not support reset",
                pstPhotoCfg->combo_venc_chn);
    return -1;
  }

  pstHandle->bReseting = true;

  RKADK_PHOTO_SetVideoChn(pstPhotoCfg, pstHandle->u32CamId, &stViChn, &stVencChn,
                     &stSrcVpssChn, &stDstVpssChn);
  stViChn.s32ChnId = pstHandle->u32ViChn;

  memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
  memset(&stViAttr, 0, sizeof(VI_CHN_ATTR_S));
  memset(&stVpssAttr, 0, sizeof(VPSS_CHN_ATTR_S));

  if (!pstHandle->stSliceParam.bJpegSlice) {
    ret = RK_MPI_VI_GetChnAttr(pstHandle->u32CamId, stViChn.s32ChnId, &stViAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Photo get vi[%d] attr failed[%x]", stViChn.s32ChnId, ret);
      goto EXIT;
    }
  }

  ret = RK_MPI_VENC_GetChnAttr(stVencChn.s32ChnId, &stVencAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Photo get venc[%d] attr failed[%x]", stVencChn.s32ChnId, ret);
    goto EXIT;
  }

  if (pstHandle->bUseVpss) {
    ret = RK_MPI_VPSS_GetChnAttr(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, &stVpssAttr);
    if (ret) {
      RKADK_LOGE("Photo get vpss grp[%d] chn[%d] attr failed[%x]",
                  pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, ret);
      goto EXIT;
    }
  }

  bPhoto = RKADK_MEDIA_CompareResolution(&stVencAttr,
                                  pstPhotoCfg->image_width,
                                  pstPhotoCfg->image_height);
  if (!bPhoto) {
    RKADK_LOGE("Photo dose not reset venc attr");
    goto EXIT;
  }

  RKADK_PHOTO_ResetAttr(pstSensorCfg, pstPhotoCfg,
                        &stVencAttr, &stViAttr, &stVpssAttr);

  if (pstHandle->stSliceParam.bJpegSlice) {
    ret = RKADK_PHOTO_SetSliceParam(pstHandle, pstSensorCfg, pstPhotoCfg);
    if (ret)
      goto EXIT;

    RKADK_PHOTO_SetViSliceParam(pstHandle, pstPhotoCfg, pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width,
                                pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height);
  } else if (pstHandle->bUseVpss) {
    // VPSS UnBind VENC
    ret = RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("UnBind VPSS[%d] to VENC[%d] failed[%x]", stSrcVpssChn.s32ChnId,
                 stVencChn.s32ChnId, ret);
      goto EXIT;
    }
  } else {
    ret = RKADK_MPI_SYS_UnBind(&stViChn, &stVencChn);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Photo VI UnBind VENC [%d %d]fail %x", stViChn.s32ChnId,
                  stVencChn.s32ChnId, ret);
      goto EXIT;
    }
  }

  ret = RK_MPI_VENC_SetChnAttr(stVencChn.s32ChnId, &stVencAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Photo set venc[%d] attr failed %x",
                stVencChn.s32ChnId, ret);
    goto EXIT;
  }

  if (pstHandle->stSliceParam.bJpegSlice) {
    VENC_JPEG_PARAM_S stJpegParam;
    memset(&stJpegParam, 0, sizeof(VENC_JPEG_PARAM_S));

#if 0
    //RK_MPI_VENC_GetJpegParam not yet released
    ret = RK_MPI_VENC_GetJpegParam(stVencChn.s32ChnId, &stJpegParam);
    if (ret) {
      RKADK_LOGE("GetJpegParam failed[%x]", ret);
      goto EXIT;
    }
#else
    stJpegParam.u32Qfactor = pstPhotoCfg->qfactor;
#endif

    stJpegParam.u32MCUPerECS = pstHandle->stSliceParam.u32MCUPerECS;
    ret = RK_MPI_VENC_SetJpegParam(stVencChn.s32ChnId, &stJpegParam);
    if (ret) {
      RKADK_LOGE("SetJpegParam failed[%x]", ret);
      goto EXIT;
    }
  } else if (pstHandle->bUseVpss) {
    ret = RK_MPI_VPSS_SetChnAttr(pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, &stVpssAttr);
    if (ret) {
      RKADK_LOGE("Photo set vpss grp[%d] chn[%d] attr falied[%x]",
                  pstPhotoCfg->vpss_grp, pstPhotoCfg->vpss_chn, ret);
      goto EXIT;
    }

    ret = RKADK_MPI_SYS_Bind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("Photo VPSS[%d] Bind VENC[%d] failed[%x]", stSrcVpssChn.s32ChnId,
                 stVencChn.s32ChnId, ret);
      goto EXIT;
    }
  } else {
    ret = RK_MPI_VI_SetChnAttr(pstHandle->u32CamId, stViChn.s32ChnId,
                              &stViAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Photo set VI[%d] attr failed %x", stViChn.s32ChnId, ret);
      goto EXIT;
    }

    ret = RKADK_MPI_SYS_Bind(&stViChn, &stVencChn);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Photo VI Bind VENC [%d %d] fail %x",stViChn.s32ChnId,
                  stVencChn.s32ChnId, ret);
      goto EXIT;
    }
  }

  pstHandle->bReseting = false;
  RKADK_LOGI("Photo[%d] Reset end...", pstHandle->u32CamId);
  return 0;

EXIT:
  pstHandle->bReseting = false;
  return -1;
}

static RKADK_S32 RKADK_PHOTO_GetJpgResolution(RKADK_CHAR *pcFileName,
                                        RKADK_PHOTO_DATA_ATTR_S *pstDataAttr) {
  RKADK_U32 cur = 0;

  if (pstDataAttr->pu8Buf[0] != 0xFF || pstDataAttr->pu8Buf[1] != 0xD8) {
    RKADK_LOGD("Invalid jpeg data");
    return -1;
  }

  cur += 2;
  while(cur + 4 + 4 < pstDataAttr->u32BufSize) {
    if (pstDataAttr->pu8Buf[cur] != 0xFF) {
      RKADK_LOGE("Bad Jpg file, 0xFF expected at offset 0x%x", cur);
      break;
    }

    if (pstDataAttr->pu8Buf[cur + 1] == 0xC0 || pstDataAttr->pu8Buf[cur + 1] == 0xC2) {
      cur += 5;
      pstDataAttr->u32Height = bswap_16(*(RKADK_U16 *) (pstDataAttr->pu8Buf + cur));
      cur += 2;
      pstDataAttr->u32Width = bswap_16(*(RKADK_U16 *) (pstDataAttr->pu8Buf + cur));
      RKADK_LOGD("%s w*h[%d, %d]", pcFileName, pstDataAttr->u32Width, pstDataAttr->u32Height);
      return 0;
    }

    cur += 2;
    cur += bswap_16(*(RKADK_U16 *) (pstDataAttr->pu8Buf + cur));
  }

  return -1;
}

static RKADK_S32 RKADK_PHOTO_BuildInThm(FILE *fd,
                                        RKADK_THUMB_ATTR_S *pstThumbAttr) {

  char tag[JPG_THUMB_TAG_LEN];
  RKADK_U32 u32Size, u32totalSize;

  // offset: 4btye tag
  if (fseek(fd, -JPG_THUMB_TAG_LEN, SEEK_END)) {
    RKADK_LOGE("seek file failed");
    return -1;
  }

  // read thm tag
  if (fread(tag, JPG_THUMB_TAG_LEN, 1, fd) != 1) {
    RKADK_LOGE("read jpg thumb tag failed");
    return -1;
  }

  if (tag[0] == 't' || tag[1] == 'h' || tag[2] == 'm') {
    RKADK_LOGD("already exist thm[%d] tag, cover", tag[3]);

    // offset: 4btye tag + 4btye size
    if (fseek(fd, -(JPG_THUMB_TAG_LEN + 4), SEEK_CUR)) {
      RKADK_LOGE("seek file failed");
      return -1;
    }

    // read thm size
    if (fread(&u32Size, 4, 1, fd) != 1) {
      RKADK_LOGE("read jpg thumb tag failed");
      return -1;
    }

    u32totalSize = pstThumbAttr->u32BufSize + 16;
    if (u32totalSize < u32Size)
      u32Size = u32totalSize;

    // offset: thm data size + 4byte thm size
    if (fseek(fd, -(u32Size + 4), SEEK_CUR)) {
      RKADK_LOGE("seek file failed");
      return -1;
    }
  }

  if (fwrite(pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize, 1, fd) != 1) {
    RKADK_LOGE("write thm data failed");
    return -1;
  }

  if (fwrite((char *)&pstThumbAttr->u32Width, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm width failed");
    return -1;
  }

  if (fwrite((char *)&pstThumbAttr->u32Height, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm height failed");
    return -1;
  }

  if (fwrite((char *)&pstThumbAttr->u32VirWidth, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual width failed");
    return -1;
  }

  if (fwrite((char *)&pstThumbAttr->u32VirHeight, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual height failed");
    return -1;
  }

  // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
  u32Size = pstThumbAttr->u32BufSize + 16;
  if (fwrite((char *)&u32Size, sizeof(RKADK_U32), 1, fd) != 1) {
    RKADK_LOGE("write thm len failed");
    return -1;
  }

  tag[0] = 't';
  tag[1] = 'h';
  tag[2] = 'm';
  tag[3] = pstThumbAttr->enType;

  if (fwrite(tag, JPG_THUMB_TAG_LEN, 1, fd) != 1) {
    RKADK_LOGE("write thm tag failed");
    return -1;
  }

  RKADK_LOGD("done");
  return 0;
}

static RKADK_S32 RKADK_PHOTO_GetThmInFile(FILE *fd,
                                          RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret = -1;
  bool bMallocBuf = false;
  char tag[JPG_THUMB_TAG_LEN];
  RKADK_U32 u32Size = 0;

  if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG)
    return -1;

  // seek to file end
  if (fseek(fd, 0, SEEK_END)) {
    RKADK_LOGE("seek file end failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (!pstThumbAttr->pu8Buf)
    bMallocBuf = true;

  while (1) {
    // offset: 4btye tag
    if (fseek(fd, -JPG_THUMB_TAG_LEN, SEEK_CUR)) {
      RKADK_LOGE("seek file failed, errno[%d, %s]", errno, strerror(errno));
      break;
    }

    // read thm tag
    if (fread(tag, JPG_THUMB_TAG_LEN, 1, fd) != 1) {
      RKADK_LOGE("read jpg thumb tag failed, errno[%d, %s]", errno, strerror(errno));
      break;
    }

    if (tag[2] == 0xFF && tag[3] == 0xD9) {
      RKADK_LOGD("read jpg EOF tag(0xFFD9)");
      break;
    }

    if (tag[0] != 't' || tag[1] != 'h' || tag[2] != 'm') {
      RKADK_LOGD("can't read thm[%d] tag", pstThumbAttr->enType);
      break;
    }

    RKADK_LOGD("tag[0] = %d, %c", tag[0], tag[0]);
    RKADK_LOGD("tag[1] = %d, %c", tag[1], tag[1]);
    RKADK_LOGD("tag[2] = %d, %c", tag[2], tag[2]);
    RKADK_LOGD("tag[3] = %d", tag[3]);
    RKADK_LOGD("pstThumbAttr->enType = %d", pstThumbAttr->enType);

    // offset: 4btye tag + 4btye size
    if (fseek(fd, -(JPG_THUMB_TAG_LEN + 4), SEEK_CUR)) {
      RKADK_LOGE("seek file failed, errno[%d, %s]", errno, strerror(errno));
      break;
    }

    // read thm size
    if (fread(&u32Size, 4, 1, fd) != 1) {
      RKADK_LOGE("read jpg thumb tag failed, errno[%d, %s]", errno, strerror(errno));
      break;
    }
    RKADK_LOGD("u32Size = %d", u32Size);

    if (tag[3] == pstThumbAttr->enType) {
      // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
      RKADK_U32 u32DataLen = u32Size - 16;

      // offset: thm data size + 4byte thm size
      if (fseek(fd, -(u32Size + 4), SEEK_CUR)) {
        RKADK_LOGE("seek file failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      if (bMallocBuf) {
        pstThumbAttr->pu8Buf = (RKADK_U8 *)malloc(u32DataLen);
        if (!pstThumbAttr->pu8Buf) {
          RKADK_LOGE("malloc thumbnail buffer[%d] failed", u32DataLen);
          break;
        }
        RKADK_LOGD("malloc thumbnail buffer[%p], u32DataLen[%d]",
                   pstThumbAttr->pu8Buf, u32DataLen);

        pstThumbAttr->u32BufSize = u32DataLen;
      } else {
        if (u32DataLen > pstThumbAttr->u32BufSize)
          RKADK_LOGW("buffer size[%d] < thumbnail data len[%d]",
                     pstThumbAttr->u32BufSize, u32DataLen);
        else
          pstThumbAttr->u32BufSize = u32DataLen;
      }

      // read thm data
      if (fread(pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize, 1, fd) != 1) {
        RKADK_LOGE("read jpg thumb data failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      // seek the remain data
      if (u32DataLen > pstThumbAttr->u32BufSize) {
        if (fseek(fd, u32DataLen - pstThumbAttr->u32BufSize, SEEK_CUR)) {
          RKADK_LOGE("seek remain data failed, errno[%d, %s]", errno, strerror(errno));
          break;
        }
      }

      if (fread(&(pstThumbAttr->u32Width), 4, 1, fd) != 1) {
        RKADK_LOGE("read jpg thumb width failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      if (fread(&(pstThumbAttr->u32Height), 4, 1, fd) != 1) {
        RKADK_LOGE("read jpg thumb height failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      if (fread(&(pstThumbAttr->u32VirWidth), 4, 1, fd) != 1) {
        RKADK_LOGE("read jpg thumb virtual width failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      if (fread(&(pstThumbAttr->u32VirHeight), 4, 1, fd) != 1) {
        RKADK_LOGE("read jpg thumb virtual height failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }

      ret = 0;
      RKADK_LOGD("[%d, %d, %d, %d]", pstThumbAttr->u32Width,
                 pstThumbAttr->u32Height, pstThumbAttr->u32VirWidth,
                 pstThumbAttr->u32VirHeight);
      RKADK_LOGD("done");
      break;
    } else {
      if (fseek(fd, -(u32Size + 4), SEEK_CUR)) {
        RKADK_LOGE("seek failed, errno[%d, %s]", errno, strerror(errno));
        break;
      }
    }
  }

  if (ret) {
    if (fseek(fd, 0, SEEK_SET)) {
      RKADK_LOGE("seek jpg file header failed, errno[%d, %s]", errno, strerror(errno));
      ret = 0;
    }

    if (bMallocBuf)
      RKADK_PHOTO_ThumbBufFree(pstThumbAttr);
  }

  return ret;
}

static RKADK_S32 RKADK_PHOTO_GetJpgThm(FILE *fd, RKADK_CHAR *pszFileName,
                                   RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret = -1, i;
  RKADK_U16 u16ExifLen;
  RKADK_U8 *pFile;
  RKADK_S64 len = 0, cur = 0, exif_end = 0;

  if (fseek(fd, 0, SEEK_END) || (len = ftell(fd)) == -1 ||
      fseek(fd, 0, SEEK_SET)) {
    RKADK_LOGE("seek %s failed", pszFileName);
    return -1;
  }

  pFile = (RKADK_U8 *)mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(fd), 0);
  if (pFile == MAP_FAILED) {
    RKADK_LOGE("mmap %s failed, errno: %d", pszFileName, errno);
    return -1;
  }

  while (cur + 4 + 4 < len) {
    if (pFile[cur] != 0xFF) {
      RKADK_LOGE("Bad Jpg file, 0xFF expected at offset 0x%llx", cur);
      goto exit;
    }

    if (pFile[cur + 1] == 0xD8 || pFile[cur + 1] == 0xD9) {
      cur += 2;
    } else if (pFile[cur + 1] == 0xE1) {
      int _tmp;
      _tmp = cur + 1 + 2 + 1;
      if (pFile[_tmp + 0] == 'E' && pFile[_tmp + 1] == 'x' &&
          pFile[_tmp + 2] == 'i' && pFile[_tmp + 3] == 'f') {
        /* Found the 0xFFE1 (EXIF) */
        cur += 2;
        break;
      }
    }

    cur += 2;
    cur += bswap_16(*(RKADK_U16 *) (pFile + cur));
  }

  /* Instead of parsing Exif, searching 0xFFD8 */
  u16ExifLen = bswap_16(*(RKADK_U16 *) (pFile + cur));

  if (u16ExifLen + cur >= len) {
    RKADK_LOGE("Bad Jpg file, Exif len exceed at offset 0x%llx", cur);
    goto exit;
  }

  exif_end = cur + u16ExifLen;
  for (i = 2; i < u16ExifLen; i++) {
    if (pFile[cur + i] != 0xFF || pFile[cur + i + 1] != 0xD8)
      continue;

    cur += i;
    /* Found the thumbnail */
    break;
  }

  /* pFile[cur, exif_end) is the thumbnail */
  if (!pstThumbAttr->pu8Buf) {
    pstThumbAttr->pu8Buf = (RKADK_U8 *)malloc(exif_end - cur);
    if (!pstThumbAttr->pu8Buf) {
      RKADK_LOGE("malloc jpg thumb buffer failed, len = %lld", exif_end - cur);
      goto exit;
    }

    pstThumbAttr->u32BufSize = exif_end - cur;
    RKADK_LOGD("malloc jpg thumb buffer[%p, %d]", pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize);
  } else {
    if ((int)pstThumbAttr->u32BufSize < (exif_end - cur))
        RKADK_LOGW("buffer size[%d] < thm data size[%lld]",
                   pstThumbAttr->u32BufSize, exif_end - cur);
    else
      pstThumbAttr->u32BufSize = exif_end - cur;
  }

  memcpy(pstThumbAttr->pu8Buf, pFile + cur, pstThumbAttr->u32BufSize);
  ret = RKADK_SUCCESS;

exit:
  munmap(pFile, len);
  return ret;
}

static RKADK_S32 RKADK_PHOTO_GetThumb(RKADK_U32 u32CamId,
                                      RKADK_CHAR *pszFileName,
                                      RKADK_JPG_THUMB_TYPE_E eThmType,
                                      RKADK_THUMB_ATTR_S *pstThumbAttr) {
  FILE *fd = NULL;
  RKADK_S32 ret = -1, result;
  RKADK_THUMB_ATTR_S stTmpThmAttr;
  RKADK_THUMB_ATTR_S *pstThmAttr = NULL;
  struct stat stStatBuf;
  struct utimbuf stTimebuf;
  bool bFree = false;

  RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!ptsThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  if (!pstThumbAttr->u32Width || !pstThumbAttr->u32Height) {
    pstThumbAttr->u32Width = UPALIGNTO(ptsThumbCfg->thumb_width, 4);
    pstThumbAttr->u32Height = UPALIGNTO(ptsThumbCfg->thumb_height, 2);
  }

  if (!pstThumbAttr->u32VirWidth || !pstThumbAttr->u32VirHeight) {
    pstThumbAttr->u32VirWidth = pstThumbAttr->u32Width;
    pstThumbAttr->u32VirHeight = pstThumbAttr->u32Height;
  }

  fd = fopen(pszFileName, "r+");
  if (!fd) {
    RKADK_LOGE("open %s failed, errno[%d, %s]", pszFileName, errno, strerror(errno));
    return -1;
  }

  memset(&stTimebuf, 0, sizeof(struct utimbuf));
  result = stat(pszFileName, &stStatBuf);
  if (result) {
    RKADK_LOGW("stat[%s] failed[%d]", pszFileName, result);
  } else {
    stTimebuf.actime = stStatBuf.st_atime;
    stTimebuf.modtime = stStatBuf.st_mtime;
  }

  ret = RKADK_PHOTO_GetThmInFile(fd, pstThumbAttr);
  if (!ret)
    goto exit;

  memset(&stTmpThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG)
    pstThmAttr = pstThumbAttr;
  else
    pstThmAttr = &stTmpThmAttr;

  ret = RKADK_PHOTO_GetJpgThm(fd, pszFileName, pstThmAttr);
  if (ret) {
    RKADK_LOGE("Get Jpg thumbnail failed");
    goto exit;
  }

  if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG)
    goto exit;

  if (RKADK_PHOTO_GetJpgResolution(pszFileName, &stTmpThmAttr)) {
    RKADK_LOGE("get %s jpg thumb resolution failed, use default", pszFileName);
    stTmpThmAttr.u32Width = ptsThumbCfg->thumb_width;
    stTmpThmAttr.u32VirWidth = stTmpThmAttr.u32Width;
    stTmpThmAttr.u32Height = ptsThumbCfg->thumb_height;
    stTmpThmAttr.u32VirHeight = stTmpThmAttr.u32Height;
  }

  stTmpThmAttr.enType = RKADK_THUMB_TYPE_JPEG;
  ret = ThumbnailJpgDecode(&stTmpThmAttr, pstThumbAttr, &bFree);
  if (!ret) {
    if (RKADK_PHOTO_BuildInThm(fd, pstThumbAttr))
      RKADK_LOGE("RKADK_PHOTO_BuildInThm failed");
  }

exit:
  if (fd)
    fclose(fd);

  if (bFree)
    RKADK_PHOTO_ThumbBufFree(&stTmpThmAttr);

  if (stTimebuf.actime != 0 && stTimebuf.modtime != 0) {
    result = utime(pszFileName, &stTimebuf);
    if (result)
      RKADK_LOGW("utime[%s] failed[%d]", pszFileName, result);
  }

  return ret;
}

RKADK_S32 RKADK_PHOTO_GetThmInJpg(RKADK_U32 u32CamId,
                                  RKADK_CHAR *pszFileName,
                                  RKADK_JPG_THUMB_TYPE_E eThmType,
                                  RKADK_U8 *pu8Buf, RKADK_U32 *pu32Size) {
  int ret;
  RKADK_THUMB_ATTR_S stThumbAttr;

  RKADK_CHECK_POINTER(pszFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pu8Buf, RKADK_FAILURE);

  memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));

  stThumbAttr.u32Width = 0;
  stThumbAttr.u32Height = 0;
  stThumbAttr.u32VirWidth = 0;
  stThumbAttr.u32VirHeight = 0;
  stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stThumbAttr.pu8Buf = pu8Buf;
  stThumbAttr.u32BufSize = *pu32Size;
  stThumbAttr.s32VdecChn = THM_VDEC_CHN;
  if (stThumbAttr.s32VdecChn < 0)
    stThumbAttr.s32VdecChn = 0;
  stThumbAttr.s32VpssGrp = THM_VPSS_GRP;
  stThumbAttr.s32VpssChn = THM_VPSS_CHN;

  ret = RKADK_PHOTO_GetThumb(u32CamId, pszFileName, eThmType, &stThumbAttr);
  *pu32Size = stThumbAttr.u32BufSize;

  return ret;
}

RKADK_S32 RKADK_PHOTO_ThumbBufFree(RKADK_THUMB_ATTR_S *pstThumbAttr) {
  return RKADK_MEDIA_FrameFree((RKADK_FRAME_ATTR_S *)pstThumbAttr);
}

RKADK_S32 RKADK_PHOTO_GetThmInJpgEx(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                                    RKADK_JPG_THUMB_TYPE_E eThmType,
                                    RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret;

  RKADK_CHECK_POINTER(pszFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstThumbAttr, RKADK_FAILURE);

  if (pstThumbAttr->s32VdecChn < 0)
    pstThumbAttr->s32VdecChn = THM_VDEC_CHN;

  if (pstThumbAttr->s32VpssGrp < 0)
    pstThumbAttr->s32VpssGrp = THM_VPSS_GRP;

  if (pstThumbAttr->s32VpssChn < 0)
    pstThumbAttr->s32VpssChn = THM_VPSS_CHN;

  ret = RKADK_PHOTO_GetThumb(u32CamId, pszFileName, eThmType, pstThumbAttr);
  if (ret)
    RKADK_PHOTO_ThumbBufFree(pstThumbAttr);

  return ret;
}

RKADK_S32 RKADK_PHOTO_GetData(RKADK_CHAR *pcFileName,
                              RKADK_PHOTO_DATA_ATTR_S *pstDataAttr) {
  int ret = -1;
  RKADK_S32 s32ReadSize = 0, s32DataSize = 0;
  RKADK_THUMB_ATTR_S stSrcDateAttr;
  bool bFree = false;

  RKADK_CHECK_POINTER(pcFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstDataAttr, RKADK_FAILURE);

  if (pstDataAttr->s32VdecChn < 0)
    pstDataAttr->s32VdecChn = GET_DATA_VDEC_CHN;

  if (pstDataAttr->s32VpssGrp < 0)
    pstDataAttr->s32VpssGrp = GET_DATA_VPSS_GRP;

  if (pstDataAttr->s32VpssChn < 0)
    pstDataAttr->s32VpssChn = GET_DATA_VPSS_CHN;

  if (pstDataAttr->enType == RKADK_THUMB_TYPE_JPEG) {
    RKADK_LOGE("Invalid type = %d", pstDataAttr->enType);
    return -1;
  }

  FILE *fd = fopen(pcFileName, "rb");
  if (!fd) {
    RKADK_LOGE("Could not open %s, errno[%d, %s]", pcFileName, errno, strerror(errno));
    return -1;
  }

  if (fseek(fd, 0, SEEK_END) || (s32DataSize = ftell(fd)) == -1 ||
      fseek(fd, 0, SEEK_SET)) {
    RKADK_LOGE("get %s size failed, errno[%d, %s]", pcFileName, errno, strerror(errno));
    goto exit;
  }

  memset(&stSrcDateAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stSrcDateAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stSrcDateAttr.pu8Buf = (RKADK_U8 *)malloc(s32DataSize);
  if (!stSrcDateAttr.pu8Buf) {
    RKADK_LOGE("malloc failed, len: %d", s32DataSize);
    goto exit;
  }
  RKADK_LOGD("malloc jpg buffer[%p, %d]", stSrcDateAttr.pu8Buf, s32DataSize);

  memset(stSrcDateAttr.pu8Buf, 0, s32DataSize);
  s32ReadSize = fread(stSrcDateAttr.pu8Buf, 1, s32DataSize, fd);
  if (s32ReadSize != s32DataSize)
    RKADK_LOGW("u32ReadSize[%d] != u32DataSize[%d], errno[%d, %s], feof[%d]", s32ReadSize, s32DataSize, errno, strerror(errno), feof(fd));

  stSrcDateAttr.u32BufSize = s32ReadSize;
  if (RKADK_PHOTO_GetJpgResolution(pcFileName, &stSrcDateAttr)) {
    RKADK_LOGE("get %s jpg thumb resolution failed", pcFileName);
    goto exit;
  }

  if (stSrcDateAttr.u32Width <= 0 || stSrcDateAttr.u32Height <= 0) {
    RKADK_LOGE("Invalid jpg[%s] thumb resolution[%d, %d] failed", pcFileName, stSrcDateAttr.u32Width, stSrcDateAttr.u32Height);
    goto exit;
  }

  if (!pstDataAttr->u32Width || !pstDataAttr->u32Height) {
    RKADK_LOGD("use default resolution[%d, %d]", stSrcDateAttr.u32Width, stSrcDateAttr.u32Height);
    pstDataAttr->u32Width = stSrcDateAttr.u32Width;
    pstDataAttr->u32VirWidth = pstDataAttr->u32Width;
    pstDataAttr->u32Height = stSrcDateAttr.u32Height;
    pstDataAttr->u32VirHeight = pstDataAttr->u32Height;
  }

  ret = ThumbnailJpgDecode(&stSrcDateAttr, (RKADK_THUMB_ATTR_S *)pstDataAttr, &bFree);

exit:
  if (fd)
    fclose(fd);

  if (bFree)
    RKADK_PHOTO_FreeData(&stSrcDateAttr);

  if (ret)
    RKADK_PHOTO_FreeData(pstDataAttr);

  return ret;
}

RKADK_S32 RKADK_PHOTO_FreeData(RKADK_PHOTO_DATA_ATTR_S *pstDataAttr) {
  return RKADK_MEDIA_FrameFree((RKADK_FRAME_ATTR_S *)pstDataAttr);
}

RKADK_S32 RKADK_PHOTO_GetDataChnCreate(RKADK_U32 u32CamId, RKADK_PHOTO_DATA_CHN_ATTR_S *pstDataChnAttr) {
  RKADK_CHECK_POINTER(pstDataChnAttr, RKADK_FAILURE);

  RKADK_PARAM_PHOTO_CFG_S *ptsPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
  if (!ptsPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg[%d] failed", u32CamId);
    return -1;
  }

  if (pstDataChnAttr->u32SrcWidth <= 0 || pstDataChnAttr->u32SrcHeight <= 0) {
    RKADK_LOGI("invalid src resolution[%d, %d], use default[%d, %d]", pstDataChnAttr->u32SrcWidth,
                pstDataChnAttr->u32SrcHeight, ptsPhotoCfg->max_width, ptsPhotoCfg->max_height);
    pstDataChnAttr->u32SrcWidth = ptsPhotoCfg->image_width;
    pstDataChnAttr->u32SrcHeight = ptsPhotoCfg->image_height;
  }

  if (pstDataChnAttr->u32DstWidth <= 0 || pstDataChnAttr->u32DstHeight <= 0) {
    RKADK_LOGI("invalid dst width[%d, %d], use default[%d, %d]", pstDataChnAttr->u32DstWidth,
                pstDataChnAttr->u32DstHeight, ptsPhotoCfg->image_width, ptsPhotoCfg->image_height);
    pstDataChnAttr->u32DstWidth = ptsPhotoCfg->image_width;
    pstDataChnAttr->u32DstHeight = ptsPhotoCfg->image_height;
  }

  if (pstDataChnAttr->s32VdecChn < 0)
    pstDataChnAttr->s32VdecChn = GET_DATA_VDEC_CHN;

  if (pstDataChnAttr->s32VpssGrp < 0)
    pstDataChnAttr->s32VpssGrp = GET_DATA_VPSS_GRP;

  if (pstDataChnAttr->s32VpssChn < 0)
    pstDataChnAttr->s32VpssChn = GET_DATA_VPSS_CHN;

  return GetThmChnCreate((RKADK_THUMB_CHN_ATTR_S *)pstDataChnAttr);
}

RKADK_S32 RKADK_PHOTO_GetThmChnDestory(RKADK_U32 u32CamId, RKADK_PHOTO_DATA_CHN_ATTR_S *pstDataChnAttr) {
  RKADK_CHECK_POINTER(pstDataChnAttr, RKADK_FAILURE);

  RKADK_PARAM_PHOTO_CFG_S *ptsPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
  if (!ptsPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg[%d] failed", u32CamId);
    return -1;
  }

  if (pstDataChnAttr->u32SrcWidth <= 0 || pstDataChnAttr->u32SrcHeight <= 0) {
    RKADK_LOGI("invalid src resolution[%d, %d], use default[%d, %d]", pstDataChnAttr->u32SrcWidth,
                pstDataChnAttr->u32SrcHeight, ptsPhotoCfg->max_width, ptsPhotoCfg->max_height);
    pstDataChnAttr->u32SrcWidth = ptsPhotoCfg->max_width;
    pstDataChnAttr->u32SrcHeight = ptsPhotoCfg->max_height;
  }

  if (pstDataChnAttr->u32DstWidth <= 0 || pstDataChnAttr->u32DstHeight <= 0) {
    RKADK_LOGI("invalid dst width[%d, %d], use default[%d, %d]", pstDataChnAttr->u32DstWidth,
                pstDataChnAttr->u32DstHeight, ptsPhotoCfg->image_width, ptsPhotoCfg->image_height);
    pstDataChnAttr->u32DstWidth = ptsPhotoCfg->image_width;
    pstDataChnAttr->u32DstHeight = ptsPhotoCfg->image_height;
  }

  if (pstDataChnAttr->s32VdecChn < 0)
    pstDataChnAttr->s32VdecChn = GET_DATA_VDEC_CHN;

  if (pstDataChnAttr->s32VpssGrp < 0)
    pstDataChnAttr->s32VpssGrp = GET_DATA_VPSS_GRP;

  if (pstDataChnAttr->s32VpssChn < 0)
    pstDataChnAttr->s32VpssChn = GET_DATA_VPSS_CHN;

  return GetThmChnDestory((RKADK_THUMB_CHN_ATTR_S *)pstDataChnAttr);
}
