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

#include "rkadk_disp.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_media_comm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef RV1126B
#include "rk_osal.h"
#endif

typedef struct {
  RKADK_U32 u32LayerId;
  RKADK_U32 u32DevId;
  RKADK_U32 u32ChnId;
} RKADK_DISP_VO_CHN_S;

typedef struct {
  bool bReseting;
  bool bSendBuffer;
  pthread_t tid; //send frame to vo thread
  RKADK_DISP_STREAM_CFG_S stStreamCfg;
} RKADK_DISP_STREAM_HANDLE_S;

typedef struct {
  bool bInit;
  RKADK_U32 u32CamId;
  bool bSendBuffer;
  pthread_t tid; //send frame to vo thread

  RKADK_DISP_STREAM_HANDLE_S stSubStreamHandle[RKADK_DISP_SUB_STREAM_NUM];
} RKADK_DISP_HANDLE_S;

static RKADK_DISP_HANDLE_S g_stDispHandle = {
    .bInit = false, .u32CamId = 0, .bSendBuffer = false, .tid= 0};

static int RKADK_DISP_CreateVo(RKADK_DISP_VO_CHN_S stVoChn, RKADK_RECT_S stVoRect,
                               RKADK_U32 rotation, RKADK_PARAM_DISP_CFG_S *pstDispCfg) {
  int ret = RK_SUCCESS;
  RKADK_VO_SPLICE_MODE_E enVoSpliceMode;
  VO_PUB_ATTR_S            stVoPubAttr;
  VO_VIDEO_LAYER_ATTR_S    stLayerAttr;
  VO_CHN_ATTR_S            stChnAttr;
  COMPRESS_MODE_E enCompressMode = COMPRESS_MODE_NONE;

  memset(&stVoPubAttr, 0, sizeof(VO_PUB_ATTR_S));
  memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VO_CHN_ATTR_S));

  stVoPubAttr.enIntfType = RKADK_MEDIA_GetRkVoIntfTpye(RKADK_PARAM_GetIntfType(pstDispCfg->intf_type));
  stVoPubAttr.enIntfSync = VO_OUTPUT_DEFAULT;

  /* Enable Layer */
  enVoSpliceMode = RKADK_PARAM_GetSpliceMode(pstDispCfg->splice_mode);
  stLayerAttr.enPixFormat      = RKADK_PARAM_GetPixFmt(pstDispCfg->img_type, &enCompressMode);
  stLayerAttr.enCompressMode   = enCompressMode;
  stLayerAttr.u32DispFrmRt     = pstDispCfg->frame_rate;
  if (enVoSpliceMode == SPLICE_MODE_BYPASS) {
    stLayerAttr.stDispRect.s32X = stVoRect.u32X;
    stLayerAttr.stDispRect.s32Y = stVoRect.u32Y;
    stLayerAttr.stDispRect.u32Width = stVoRect.u32Width;
    stLayerAttr.stDispRect.u32Height = stVoRect.u32Height;
  }

  stChnAttr.stRect.s32X = stVoRect.u32X;
  stChnAttr.stRect.s32Y = stVoRect.u32Y;
  stChnAttr.stRect.u32Width = stVoRect.u32Width;
  stChnAttr.stRect.u32Height = stVoRect.u32Height;
  stChnAttr.u32FgAlpha = 255;
  stChnAttr.u32BgAlpha = 0;
  stChnAttr.enMirror = MIRROR_NONE;
  stChnAttr.enRotation = (ROTATION_E)rotation;
  stChnAttr.u32Priority = stVoChn.u32ChnId;

  // DispBufLen set to 3 to resolve display frame rate insufficiency
  if (pstDispCfg->layer_buflen <= 0)
    pstDispCfg->layer_buflen = 3;
  ret = RKADK_MPI_VO_Init(stVoChn.u32LayerId, stVoChn.u32DevId, stVoChn.u32ChnId,
                          &stVoPubAttr, &stLayerAttr, &stChnAttr, enVoSpliceMode,
                          pstDispCfg->layer_buflen, VO_LAYER_MODE_GRAPHIC);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_Vo_Init failed, ret = %x", ret);
    return ret;
  }

  RKADK_LOGI("Create vo [dev: %d, layer: %d, chn: %d] success!",
              stVoChn.u32LayerId, stVoChn.u32DevId, stVoChn.u32ChnId);
  return ret;
}

static int RKADK_DISP_DestroyVo(RKADK_DISP_VO_CHN_S stVoChn) {
  int ret = 0;

  ret = RKADK_MPI_VO_DeInit(stVoChn.u32LayerId, stVoChn.u32DevId, stVoChn.u32ChnId);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_Vo_DeInit failed, ret = %x", ret);
    return ret;
  }

  RKADK_LOGI("Destroy vo [dev: %d, layer: %d, chn: %d] success!",
            stVoChn.u32LayerId, stVoChn.u32DevId, stVoChn.u32ChnId);
  return ret;
}

static void RKADK_DISP_SetChn(RKADK_U32 u32CamId,
                              RKADK_PARAM_DISP_CFG_S *pstDispCfg,
                              MPP_CHN_S *pstViChn, MPP_CHN_S *pstVoChn,
                              MPP_CHN_S *pstSrcVpssChn, MPP_CHN_S *pstDstVpssChn) {
  pstViChn->enModId = RK_ID_VI;
  pstViChn->s32DevId = u32CamId;
  pstViChn->s32ChnId = pstDispCfg->vi_attr.u32ViChn;

  pstSrcVpssChn->enModId = RK_ID_VPSS;
  pstSrcVpssChn->s32DevId = pstDispCfg->vpss_grp;
  pstSrcVpssChn->s32ChnId = pstDispCfg->vpss_chn;

  pstDstVpssChn->enModId = RK_ID_VPSS;
  pstDstVpssChn->s32DevId = pstDispCfg->vpss_grp;
  pstDstVpssChn->s32ChnId = 0; //When vpss is dst, chn is equal to 0

  pstVoChn->enModId = RK_ID_VO;
  RKADK_MPI_VO_CheckDefaultID((RKADK_S32 *)&pstDispCfg->vo_device, (RKADK_S32 *)&pstDispCfg->vo_layer);
  pstVoChn->s32DevId = pstDispCfg->vo_layer;
  pstVoChn->s32ChnId = pstDispCfg->vo_chn;
}

static RKADK_S32 RKADK_DISP_Enable(RKADK_U32 u32CamId, RKADK_PARAM_DISP_CFG_S *pstDispCfg,
                                   RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg) {
  int ret = 0;
  VPSS_GRP_ATTR_S stGrpAttr;
  VPSS_CHN_ATTR_S stChnAttr;
  RKADK_DISP_VO_CHN_S stVoChn;
  RKADK_RECT_S stVoRect;

  // Create VI
  ret = RKADK_MPI_VI_Init(u32CamId, pstDispCfg->vi_attr.u32ViChn,
                          &(pstDispCfg->vi_attr.stChnAttr));
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_Init faled %d", ret);
    return ret;
  }
  RKADK_BUFINFO("create vi[%d] end", pstDispCfg->vi_attr.u32ViChn);

  // Create VPSS
  if (pstDispCfg->enable_vpss) {
    memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
    memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));

    stGrpAttr.u32MaxW = pstSensorCfg->max_width;
    stGrpAttr.u32MaxH = pstSensorCfg->max_height;
    stGrpAttr.enPixelFormat = pstDispCfg->vi_attr.stChnAttr.enPixelFormat;
    stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;
    stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stGrpAttr.stFrameRate.s32DstFrameRate = -1;

    stChnAttr.enChnMode = VPSS_CHN_MODE_USER;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enPixelFormat = pstDispCfg->vi_attr.stChnAttr.enPixelFormat;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    stChnAttr.u32Width = pstDispCfg->vi_attr.stChnAttr.stSize.u32Width;
    stChnAttr.u32Height = pstDispCfg->vi_attr.stChnAttr.stSize.u32Height;
    stChnAttr.u32FrameBufCnt = pstDispCfg->vi_attr.stChnAttr.stIspOpt.u32BufCount + 2;
    if (!pstSensorCfg->used_isp) {
      stChnAttr.bMirror = (RK_BOOL)pstSensorCfg->mirror;
      stChnAttr.bFlip = (RK_BOOL)pstSensorCfg->flip;
    }

#if defined(RV1106_1103) || defined(RV1103B)
    //unbind mode
    stChnAttr.u32Depth = 2;
#else
    stChnAttr.u32Depth = 0;
#endif

    ret = RKADK_MPI_VPSS_Init(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn,
                              &stGrpAttr, &stChnAttr);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_VPSS_Init vpss falied[%d]",ret);
      return ret;
    }

    RKADK_BUFINFO("create vpss[%d, %d] end", pstDispCfg->vpss_grp, pstDispCfg->vpss_chn);
  }

  // Create VO
  memset(&stVoChn, 0, sizeof(RKADK_DISP_VO_CHN_S));
  stVoChn.u32LayerId = pstDispCfg->vo_layer;
  stVoChn.u32DevId = pstDispCfg->vo_device;
  stVoChn.u32ChnId = pstDispCfg->vo_chn;

  memset(&stVoRect, 0, sizeof(RKADK_RECT_S));
  stVoRect.u32X = pstDispCfg->x;
  stVoRect.u32Y = pstDispCfg->y;
  stVoRect.u32Width = pstDispCfg->width;
  stVoRect.u32Height = pstDispCfg->height;

  ret = RKADK_DISP_CreateVo(stVoChn, stVoRect, pstDispCfg->rotation, pstDispCfg);
  if (ret) {
    RKADK_LOGE("Create vo [dev: %d, layer: %d, chn: %d] failed, ret = %x",
                pstDispCfg->vo_device, pstDispCfg->vo_layer, pstDispCfg->vo_chn, ret);
    return ret;
  }
  RKADK_BUFINFO("create vo[%d] end", pstDispCfg->vo_chn);

  return 0;
}

static bool RKADK_DISP_CheckParam(RKADK_PARAM_DISP_CFG_S *pstDispCfg) {
  if (pstDispCfg->x < 0 || pstDispCfg->y < 0 || pstDispCfg->width <= 0 ||
      pstDispCfg->height <= 0) {
    RKADK_LOGE("Display rect erro [x: %d, y: %d, width: %d, height: %d]",
              pstDispCfg->x, pstDispCfg->y, pstDispCfg->width, pstDispCfg->height);
    return false;
  }

  return true;
}

static bool RKADK_DISP_CheckRect(RKADK_RECT_S stRect,
                                 RKADK_PARAM_DISP_CFG_S *pstDispCfg, bool bVpss) {
  if ((stRect.u32Width && stRect.u32X >= stRect.u32Width)
      || (stRect.u32Height && stRect.u32Y >= stRect.u32Height)) {
    RKADK_LOGE("invalid rect[x: %d, y: %d, width: %d, height: %d]",
          stRect.u32X, stRect.u32Y, stRect.u32Width, stRect.u32Height);
    return false;
  }

  if (bVpss && pstDispCfg->enable_vpss) {
    if (stRect.u32X + stRect.u32Width > pstDispCfg->vi_attr.stChnAttr.stSize.u32Width) {
      RKADK_LOGE("Vpss crop rect x: %d + width: %d > input width: %d",
                stRect.u32X, stRect.u32Width,
                pstDispCfg->vi_attr.stChnAttr.stSize.u32Width);
      return false;
    }

    if (stRect.u32Y + stRect.u32Y > pstDispCfg->vi_attr.stChnAttr.stSize.u32Height) {
      RKADK_LOGE("Vpss crop rect y: %d + height: %d > input height: %d",
                stRect.u32Y , stRect.u32Y,
                pstDispCfg->vi_attr.stChnAttr.stSize.u32Height);
      return false;
    }
  }

  return true;
}

static void *RKADK_DISP_SendVoFrame(void *arg) {
  int s32Ret;
  VIDEO_FRAME_INFO_S stVideoFrame;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;

  RKADK_DISP_HANDLE_S *pstDispHandle = (RKADK_DISP_HANDLE_S *)arg;
  if (!pstDispHandle) {
    RKADK_LOGE("Get VPSS MB thread invalid param");
    return NULL;
  }

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(pstDispHandle->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg[%d] failed", pstDispHandle->u32CamId);
    return NULL;
  }

  RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(pstDispHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstDispHandle->u32CamId);
    return NULL;
  }

  while (pstDispHandle->bSendBuffer) {
    if (pstDispCfg->enable_vpss) {
      s32Ret = RK_MPI_VPSS_GetChnFrame(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, &stVideoFrame, 1000);
      if (s32Ret == RK_SUCCESS) {
        s32Ret = RK_MPI_VO_SendFrame(pstDispCfg->vo_layer, pstDispCfg->vo_chn, &stVideoFrame, -1);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VO_SendFrame failed[%x]", s32Ret);

        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, &stVideoFrame);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VPSS_ReleaseChnFrame failed[%x]", s32Ret);
      } else {
        RKADK_LOGE("RK_MPI_VPSS_GetChnFrame[%d, %d] timeout[%x]", pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, s32Ret);
      }
    } else {
      s32Ret = RK_MPI_VI_GetChnFrame(pstSensorCfg->dev_id, pstDispCfg->vi_attr.u32ViChn, &stVideoFrame, 1000);
      if (s32Ret == RK_SUCCESS) {
        s32Ret = RK_MPI_VO_SendFrame(pstDispCfg->vo_layer, pstDispCfg->vo_chn, &stVideoFrame, -1);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VO_SendFrame failed[%x]", s32Ret);

        s32Ret = RK_MPI_VI_ReleaseChnFrame(pstSensorCfg->dev_id, pstDispCfg->vi_attr.u32ViChn, &stVideoFrame);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VI_ReleaseChnFrame failed[%x]", s32Ret);
      } else {
        RKADK_LOGE("RK_MPI_VI_GetChnFrame[%d, %d] timeout[%x]", pstSensorCfg->dev_id, pstDispCfg->vi_attr.u32ViChn, s32Ret);
      }
    }
  }

  RKADK_LOGD("Exit!");
  return NULL;
}

static void *RKADK_DISP_SendSubVoFrame(void *arg) {
  int s32Ret;
  VIDEO_FRAME_INFO_S stVideoFrame;

  RKADK_DISP_STREAM_HANDLE_S *pstStreamHandle = (RKADK_DISP_STREAM_HANDLE_S *)arg;
  if (!pstStreamHandle) {
    RKADK_LOGE("Get VPSS MB thread invalid param");
    return NULL;
  }

  RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(pstStreamHandle->stStreamCfg.s32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstStreamHandle->stStreamCfg.s32CamId);
    return NULL;
  }

  while (pstStreamHandle->bSendBuffer) {
    if (pstStreamHandle->stStreamCfg.enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
      s32Ret = RK_MPI_VDEC_GetFrame(pstStreamHandle->stStreamCfg.stVdecCfg.u32VdecChn, &stVideoFrame, 1000);
      if (s32Ret == RK_SUCCESS) {
          s32Ret = RK_MPI_VO_SendFrame(pstDispCfg->vo_layer, pstStreamHandle->stStreamCfg.stVoCfg.u32VoChn, &stVideoFrame, -1);
          if (s32Ret != RK_SUCCESS)
            RKADK_LOGE("RK_MPI_VO_SendFrame failed[%x]", s32Ret);

          s32Ret = RK_MPI_VDEC_ReleaseFrame(pstStreamHandle->stStreamCfg.stVdecCfg.u32VdecChn, &stVideoFrame);
          if (s32Ret != RK_SUCCESS)
            RKADK_LOGE("RK_MPI_VDEC_ReleaseFrame failed[%x]", s32Ret);
      } else {
        RKADK_LOGD("RK_MPI_VDEC_GetFrame[%d] timeout[%x]", pstStreamHandle->stStreamCfg.stVdecCfg.u32VdecChn, s32Ret);
      }
    } else {
      s32Ret = RK_MPI_VPSS_GetChnFrame(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, &stVideoFrame, 1000);
      if (s32Ret == RK_SUCCESS) {
        s32Ret = RK_MPI_VO_SendFrame(pstDispCfg->vo_layer, pstDispCfg->vo_chn, &stVideoFrame, -1);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VO_SendFrame failed[%x]", s32Ret);

        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, &stVideoFrame);
        if (s32Ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VPSS_ReleaseChnFrame failed[%x]", s32Ret);
      } else {
        RKADK_LOGD("RK_MPI_VPSS_GetChnFrame[%d, %d] timeout[%x]", pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, s32Ret);
      }
    }
  }

  RKADK_LOGD("Exit send vo thread!");
  return NULL;
}


static RKADK_S32 DispCreateVdec(RKADK_DISP_VDEC_CFG_S *pstVdecCfg) {
  RKADK_S32 ret = RKADK_SUCCESS;
  VDEC_CHN_ATTR_S stAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
  VDEC_MOD_PARAM_S stModParam;

  memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
  memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
  memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));

  stAttr.enMode = pstVdecCfg->u32DecodeMode;
  stAttr.enType = RKADK_MEDIA_GetRkCodecType(pstVdecCfg->enCodecType);
  stAttr.u32PicWidth = pstVdecCfg->u32SrcWidth;
  stAttr.u32PicHeight = pstVdecCfg->u32SrcHeight;

  if (stAttr.enType == RK_VIDEO_ID_Unused) {
    RKADK_LOGE("Codec type[%d] is invalid", pstVdecCfg->enCodecType);
    return -1;
  }

  if (pstVdecCfg->u32FrameBufCnt > 0)
    stAttr.u32FrameBufCnt = pstVdecCfg->u32FrameBufCnt;
  else
    stAttr.u32FrameBufCnt = 3;

  if (pstVdecCfg->u32StreamBufCnt > 0)
    stAttr.u32StreamBufCnt = pstVdecCfg->u32StreamBufCnt;
  else
    stAttr.u32StreamBufCnt = 8;
  stAttr.u32FrameBufDepth = stAttr.u32StreamBufCnt;

  /*
    * if decode 10bit stream, need specify the u32FrameBufSize,
    * other conditions can be set to 0, calculated internally.
    */
  if (pstVdecCfg->u32FrameBufSize > 0)
    stAttr.u32FrameBufSize = pstVdecCfg->u32FrameBufSize;

  ret = RK_MPI_VDEC_CreateChn(pstVdecCfg->u32VdecChn, &stAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Create vdec chn[%d] failed[%x]", pstVdecCfg->u32VdecChn, ret);
    return ret;
  }

  stVdecParam.stVdecVideoParam.enCompressMode = COMPRESS_MODE_NONE;
  stVdecParam.stVdecVideoParam.enOutputOrder = pstVdecCfg->enOutputOrder;
  ret = RK_MPI_VDEC_SetChnParam(pstVdecCfg->u32VdecChn, &stVdecParam);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Set chn[%d] param failed[%x]", pstVdecCfg->u32VdecChn, ret);
    goto failed;
  }

  ret = RK_MPI_VDEC_StartRecvStream(pstVdecCfg->u32VdecChn);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("start recv chn[%d] failed[%x]", pstVdecCfg->u32VdecChn, ret);
    goto failed;
  }

  RKADK_LOGD("Vdec[%d] create success", pstVdecCfg->u32VdecChn);
  return RKADK_SUCCESS;

failed:
  RK_MPI_VDEC_DestroyChn(pstVdecCfg->u32VdecChn);
  return RKADK_FAILURE;
}

static RKADK_S32 DispDestroyVdec(RKADK_DISP_VDEC_CFG_S *pstVdecCfg) {
  RKADK_S32 ret = 0;

  ret = RK_MPI_VDEC_StopRecvStream(pstVdecCfg->u32VdecChn);
  if (ret) {
    RKADK_LOGE("Stop Vdec[%d] stream failed[%x]", pstVdecCfg->u32VdecChn, ret);
    return ret;
  }

  ret = RK_MPI_VDEC_DestroyChn(pstVdecCfg->u32VdecChn);
  if (ret) {
    RKADK_LOGE("Destroy Vdec channel[%d] failed[%x]", pstVdecCfg->u32VdecChn, ret);
    return ret;
  }

  RKADK_LOGD("Vdec[%d] destory success", pstVdecCfg->u32VdecChn);
  return 0;
}

RKADK_S32 RKADK_DISP_SendVideoStream(RKADK_MW_PTR pHandle, RKADK_U32 u32VdecChn, RKADK_DISP_DATA_S *pstData) {
  RKADK_S32 ret = 0, i = 0;
  VDEC_STREAM_S stStream;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_DISP_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(pstData, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstData->data, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_DISP_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

  if (pstData->bBypass && !pstData->pFreeCB) {
    RKADK_LOGE("Bypass mode, must set pFreeCB");
    return -1;
  }

  for (i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    if (pstHandle->stSubStreamHandle[i].stStreamCfg.stVdecCfg.u32VdecChn == u32VdecChn)
      break;
  }

  if (i == RKADK_DISP_SUB_STREAM_NUM) {
    RKADK_LOGE("Vdec[%d] not found", u32VdecChn);
    return -1;
  }

  if (pstHandle->stSubStreamHandle[i].stStreamCfg.enStreamType != RKADK_STREAM_TYPE_THIRD_STREAM) {
    RKADK_LOGE("Vdec[%d] is not third-party stream, skip send stream", u32VdecChn);
    return -1;
  }

  if(pstHandle->stSubStreamHandle[i].bReseting) {
    RKADK_LOGI("Vdec[%d] is reseting, skip send stream", u32VdecChn);
    if (pstData->pFreeCB)
      pstData->pFreeCB(pstData->data);
    return 0;
  }

  memset(&stStream, 0, sizeof(VDEC_STREAM_S));
  stStream.u64PTS = pstData->u64Pts;
  stStream.u32Len = pstData->u32Len;
  stStream.bEndOfStream = pstData->bEofFlag ? RK_TRUE : RK_FALSE;
  stStream.bEndOfFrame = pstData->bEofFlag ? RK_TRUE : RK_FALSE;
  stStream.bBypassMbBlk = pstData->bBypass? RK_TRUE : RK_FALSE;

  memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
  stMbExtConfig.pOpaque = (RKADK_VOID *)pstData->data;
  stMbExtConfig.pu8VirAddr = (RK_U8 *)pstData->data;
  stMbExtConfig.u64Size = pstData->u32Len;
  if (stStream.bBypassMbBlk)
    stMbExtConfig.pFreeCB = pstData->pFreeCB;

  ret = RK_MPI_SYS_CreateMB(&stStream.pMbBlk, &stMbExtConfig);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_SYS_CreateMB failed[%x]", ret);
    return -1;
  }

__RETRY:
  ret = RK_MPI_VDEC_SendStream(u32VdecChn, &stStream, -1);
  if (ret != RK_SUCCESS) {
    if (!pstHandle->bInit) {
      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
      return 0;
    }

    RKADK_LOGD("RK_MPI_VDEC_SendStream[%d] failed[%x]", u32VdecChn, ret);
#ifdef RV1126B
    rkos_msleep(1llu);
#else
    usleep(1000);
#endif
    goto  __RETRY;
  }

  RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
  return 0;
}

static RKADK_S32 RKADK_DISP_SubStreamDeInit(RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_DISP_VO_CHN_S stDispVoChn;
  RKADK_DISP_STREAM_CFG_S *pstSubStream = NULL;
  RKADK_PRAAM_VI_ATTR_S *pSubViChnAttr = NULL;
  RKADK_PARAM_DISP_CFG_S *pstDispCfg = NULL, *pstSubDispCfg = NULL;
  MPP_CHN_S stViChn, stVoChn, stSrcVpssChn, stDstVpssChn;

  pstDispCfg = RKADK_PARAM_GetDispCfg(pHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pHandle->u32CamId);
    return -1;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    pstSubStream = &pHandle->stSubStreamHandle[i].stStreamCfg;
    if (!pstSubStream->bEnable)
      continue;

    stVoChn.enModId = RK_ID_VO;
    stVoChn.s32DevId = pstDispCfg->vo_layer;
    stVoChn.s32ChnId = pstSubStream->stVoCfg.u32VoChn;

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      pstSubDispCfg = RKADK_PARAM_GetDispCfg(pstSubStream->s32CamId);
      if (!pstSubDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstSubStream->s32CamId);
        return -1;
      }

      RKADK_DISP_SetChn(pstSubStream->s32CamId, pstSubDispCfg, &stViChn, &stVoChn, &stSrcVpssChn, &stDstVpssChn);

#if defined(RV1106_1103) || defined(RV1103B)
      pHandle->stSubStreamHandle[i].bSendBuffer = false;
      if (pHandle->stSubStreamHandle[i].tid) {
        RKADK_LOGD("Request to cancel get vpss[%d] mb thread...", stSrcVpssChn.s32ChnId);
        ret = pthread_join(pHandle->stSubStreamHandle[i].tid, NULL);
        if (ret)
          RKADK_LOGE("Exit get vpss[%d] mb thread failed!", stSrcVpssChn.s32ChnId);
        else
          RKADK_LOGI("Exit get vpss[%d] mb thread ok", stSrcVpssChn.s32ChnId);
        pHandle->stSubStreamHandle[i].tid = 0;
      }
#else
      // VPSS UnBind VO
      ret = RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVoChn);
      if (ret) {
          RKADK_LOGE("UnBind VPSS[%d, %d] to VO[%d] failed[%x]", stSrcVpssChn.s32DevId,
                    stSrcVpssChn.s32ChnId, stVoChn.s32ChnId, ret);
        return ret;
      }
#endif

      // VI UnBind VPSS
      ret = RKADK_MPI_SYS_UnBind(&stViChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("UnBind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId,
                    stDstVpssChn.s32DevId, ret);
        return ret;
      }
    } else if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
#if 0
      // VDEC UnBind VO
      MPP_CHN_S stVdecChn;
      stVdecChn.enModId = RK_ID_VDEC;
      stVdecChn.s32DevId = 0;
      stVdecChn.s32ChnId = pstSubStream->stVdecCfg.u32VdecChn;
      ret = RK_MPI_SYS_UnBind(&stVdecChn, &stVoChn);
      if (ret) {
        RKADK_LOGE("UnBind VDEC[%d] to VO[%d, %d] failed[%x]", pstSubStream->stVdecCfg.u32VdecChn, stVoChn.s32DevId, stVoChn.s32ChnId, ret);
        return ret;
      }
      RKADK_LOGD("UnBind VDEC[%d] to VO[%d, %d] success", pstSubStream->stVdecCfg.u32VdecChn, stVoChn.s32DevId, stVoChn.s32ChnId);
#else
      pHandle->stSubStreamHandle[i].bSendBuffer = false;
      if (pHandle->stSubStreamHandle[i].tid) {
        RKADK_LOGD("Request to cancel get vdec[%d] mb thread...", pstSubStream->stVdecCfg.u32VdecChn);
        ret = pthread_join(pHandle->stSubStreamHandle[i].tid, NULL);
        if (ret)
          RKADK_LOGE("Exit get vdec[%d] mb thread failed!", pstSubStream->stVdecCfg.u32VdecChn);
        else
          RKADK_LOGI("Exit get vdec[%d] mb thread ok", pstSubStream->stVdecCfg.u32VdecChn);
        pHandle->stSubStreamHandle[i].tid = 0;
      }
#endif
    } else {
      ret = RKADK_PARAM_GetViAttr(pstSubStream->s32CamId, pstSubStream->enStreamType, &pSubViChnAttr);
      if (ret || !pSubViChnAttr) {
        RKADK_LOGE("RKADK_PARAM_GetViAttr[%d, %d] failed", pstSubStream->s32CamId, pstSubStream->enStreamType);
        return ret;
      }

      stViChn.enModId = RK_ID_VI;
      stViChn.s32DevId = RKADK_PARAM_GetViDevId(pstSubStream->s32CamId);
      stViChn.s32ChnId = pSubViChnAttr->u32ViChn;

      //VI UnBind VO
      ret = RK_MPI_SYS_UnBind(&stViChn, &stVoChn);
      if (ret) {
        RKADK_LOGE("UnBind VI[%d, %d] to VO[%d, %d] failed[%x]", stViChn.s32DevId, stViChn.s32ChnId, stVoChn.s32DevId, stVoChn.s32ChnId, ret);
      }
      RKADK_LOGD("UnBind VI[%d, %d] to VO[%d, %d] success", stViChn.s32DevId, stViChn.s32ChnId, stVoChn.s32DevId, stVoChn.s32ChnId);
    }

    memset(&stDispVoChn, 0, sizeof(RKADK_DISP_VO_CHN_S));
    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      stDispVoChn.u32LayerId = pstSubDispCfg->vo_layer;
      stDispVoChn.u32DevId = pstSubDispCfg->vo_device;
      stDispVoChn.u32ChnId = pstSubDispCfg->vo_chn;
    } else {
      stDispVoChn.u32LayerId = pstDispCfg->vo_layer;
      stDispVoChn.u32DevId = pstDispCfg->vo_device;
      stDispVoChn.u32ChnId = pstSubStream->stVoCfg.u32VoChn;
    }
    ret = RKADK_DISP_DestroyVo(stDispVoChn);
    if (ret)
      RKADK_LOGE("Destory VO[%d] failed(%d)", stVoChn.s32ChnId, ret);

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
      // Destory Vdec
      ret = DispDestroyVdec(&pstSubStream->stVdecCfg);
      if (ret)
        RKADK_LOGE("Destory Vdec[%d] failed(%d)", pstSubStream->stVdecCfg.u32VdecChn, ret);
    } else {
      if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
        ret = RKADK_MPI_VPSS_DeInit(pstSubDispCfg->vpss_grp, pstSubDispCfg->vpss_chn);
        if (ret)
          RKADK_LOGE("DeInit VPSS[%d, %d] failed[%x]", pstSubDispCfg->vpss_grp, pstSubDispCfg->vpss_chn, ret);
      }

      ret = RKADK_MPI_VI_DeInit(pstSubStream->s32CamId, stViChn.s32ChnId);
      if (ret)
        RKADK_LOGE("RKADK_MPI_VI_DeInit failed(%d)", ret);
    }
  }

  return ret;
}

static RKADK_S32 RKADK_DISP_SubStreamInit(RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_DISP_VO_CHN_S stDispVoChn;
  RKADK_DISP_STREAM_CFG_S *pstSubStream = NULL;
  RKADK_PRAAM_VI_ATTR_S *pSubViChnAttr = NULL;
  RKADK_PARAM_DISP_CFG_S *pstDispCfg;
  MPP_CHN_S stViChn, stVoChn, stSrcVpssChn, stDstVpssChn;
  RKADK_RECT_S stVoRect;

  pstDispCfg = RKADK_PARAM_GetDispCfg(pHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pHandle->u32CamId);
    return -1;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    pstSubStream = &pHandle->stSubStreamHandle[i].stStreamCfg;
    if (!pstSubStream->bEnable)
      continue;

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      RKADK_PARAM_SENSOR_CFG_S *pstSubSensorCfg = RKADK_PARAM_GetSensorCfg(pstSubStream->s32CamId);
      if (!pstSubSensorCfg) {
        RKADK_LOGE("RKADK_PARAM_GetSensorCfg[%d] failed", pstSubStream->s32CamId);
        goto failed;
      }

      RKADK_PARAM_DISP_CFG_S *pstSubDispCfg = RKADK_PARAM_GetDispCfg(pstSubStream->s32CamId);
      if (!pstSubDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstSubStream->s32CamId);
        goto failed;
      }

      RKADK_DISP_SetChn(pstSubStream->s32CamId, pstSubDispCfg, &stViChn, &stVoChn, &stSrcVpssChn, &stDstVpssChn);

      ret = RKADK_DISP_Enable(pstSubStream->s32CamId, pstSubDispCfg, pstSubSensorCfg);
      if (ret) {
        RKADK_LOGE("RKADK_DISP_Enable[%d] failed", pstSubStream->s32CamId);
        goto failed;
      }

#if defined(RV1106_1103) || defined(RV1103B)
      char name[RKADK_THREAD_NAME_LEN];
      pHandle->stSubStreamHandle[i].bSendBuffer = true;
      ret = pthread_create(&pHandle->stSubStreamHandle[i].tid, NULL, RKADK_DISP_SendSubVoFrame, &pHandle->stSubStreamHandle[i]);
      if (ret) {
        RKADK_LOGE("Create get vpss mb(%d) thread failed %d", stSrcVpssChn.s32ChnId, ret);
        goto failed;
      }
      snprintf(name, sizeof(name), "SendSubVoFrame_%d_%d", stSrcVpssChn.s32DevId, stSrcVpssChn.s32ChnId);
      pthread_setname_np(pHandle->stSubStreamHandle[i].tid, name);
      RKADK_LOGD("Create get vpss[%d, %d] mb thread  ok", stSrcVpssChn.s32DevId, stSrcVpssChn.s32ChnId);
#else
      // Bind VPSS to VO
      ret = RKADK_MPI_SYS_Bind(&stSrcVpssChn, &stVoChn);
      if (ret) {
        RKADK_LOGE("Bind VPSS[%d, %d] to VO[%d] failed[%x]", stSrcVpssChn.s32DevId,
                    stSrcVpssChn.s32ChnId, stVoChn.s32ChnId, ret);
        goto failed;
      }
#endif

      // VI Bind VPSS
      ret = RKADK_MPI_SYS_Bind(&stViChn, &stDstVpssChn);
      if (ret) {
        RKADK_LOGE("Bind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId, stDstVpssChn.s32DevId, ret);
        goto failed;
      }

      continue;
    } else {
      memcpy(&stVoRect, &pstSubStream->stVoCfg.stVoRect, sizeof(RKADK_RECT_S));
      if (stVoRect.u32Width == 0 || stVoRect.u32Height == 0) {
        RKADK_LOGE("invalid rect[%d, %d, %d, %d]", stVoRect.u32X, stVoRect.u32Y, stVoRect.u32Width, stVoRect.u32Height);
        return -1;
      }

      if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
        // Create Vdec
        ret = DispCreateVdec(&pstSubStream->stVdecCfg);
        if (ret) {
          RKADK_LOGE("DispCreateVdec failed[%x]", ret);
          goto failed;
        }
      } else {
        ret = RKADK_PARAM_GetViAttr(pstSubStream->s32CamId, pstSubStream->enStreamType, &pSubViChnAttr);
        if (ret || !pSubViChnAttr) {
          RKADK_LOGE("RKADK_PARAM_GetViAttr[%d, %d] failed", pstSubStream->s32CamId, pstSubStream->enStreamType);
          goto failed;
        }

        // Create sub stream VI
        ret = RKADK_MPI_VI_Init(pstSubStream->s32CamId, pSubViChnAttr->u32ViChn, &(pSubViChnAttr->stChnAttr));
        if (ret) {
          RKADK_LOGE("RKADK_MPI_VI_Init failed[%x]", ret);
          goto failed;
        }
      }

      // Create sub stream VO
      memset(&stDispVoChn, 0, sizeof(RKADK_DISP_VO_CHN_S));
      stDispVoChn.u32LayerId = pstDispCfg->vo_layer;
      stDispVoChn.u32DevId = pstDispCfg->vo_device;
      stDispVoChn.u32ChnId = pstSubStream->stVoCfg.u32VoChn;
      ret = RKADK_DISP_CreateVo(stDispVoChn, stVoRect, pstSubStream->stVoCfg.rotation, pstDispCfg);
      if (ret) {
        RKADK_LOGE("Create vo [dev: %d, layer: %d, chn: %d] failed, ret = %x",
                    stDispVoChn.u32LayerId, stDispVoChn.u32DevId, stDispVoChn.u32ChnId, ret);
        goto failed;
      }

      stVoChn.enModId = RK_ID_VO;
      stVoChn.s32DevId = pstDispCfg->vo_layer;
      stVoChn.s32ChnId = pstSubStream->stVoCfg.u32VoChn;

      if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
#if 0
        // VDEC Bind VO
        MPP_CHN_S stVdecChn;
        stVdecChn.enModId = RK_ID_VDEC;
        stVdecChn.s32DevId = 0;
        stVdecChn.s32ChnId = pstSubStream->stVdecCfg.u32VdecChn;
        ret = RK_MPI_SYS_Bind(&stVdecChn, &stVoChn);
        if (ret) {
          RKADK_LOGE("Bind VDEC[%d] to VO[%d, %d] failed[%x]", pstSubStream->stVdecCfg.u32VdecChn, stVoChn.s32DevId, stVoChn.s32ChnId, ret);
          goto failed;
        }
        RKADK_LOGD("Bind VDEC[%d] to VO[%d, %d] success", pstSubStream->stVdecCfg.u32VdecChn, stVoChn.s32DevId, stVoChn.s32ChnId);
#else
        char name[RKADK_THREAD_NAME_LEN];
        pHandle->stSubStreamHandle[i].bSendBuffer = true;
        ret = pthread_create(&pHandle->stSubStreamHandle[i].tid, NULL, RKADK_DISP_SendSubVoFrame, &pHandle->stSubStreamHandle[i]);
        if (ret) {
          RKADK_LOGE("Create get vdec mb(%d) thread failed %d", pstSubStream->stVdecCfg.u32VdecChn, ret);
          goto failed;
        }
        snprintf(name, sizeof(name), "SendSubVoFrame_%d", pstSubStream->stVdecCfg.u32VdecChn);
        pthread_setname_np(pHandle->stSubStreamHandle[i].tid, name);
        RKADK_LOGD("Create get vdec[%d] mb thread ok", pstSubStream->stVdecCfg.u32VdecChn);
#endif
      } else {
        // VI Bind VO
        stViChn.enModId = RK_ID_VI;
        stViChn.s32DevId = RKADK_PARAM_GetViDevId(pstSubStream->s32CamId);
        stViChn.s32ChnId = pSubViChnAttr->u32ViChn;
        ret = RK_MPI_SYS_Bind(&stViChn, &stVoChn);
        if (ret) {
          RKADK_LOGE("Bind VI[%d, %d] to VO[%d, %d] failed[%x]", stViChn.s32DevId, stViChn.s32ChnId, stVoChn.s32DevId, stVoChn.s32ChnId, ret);
          goto failed;
        }
        RKADK_LOGD("Bind VI[%d, %d] to VO[%d, %d] success", stViChn.s32DevId, stViChn.s32ChnId, stVoChn.s32DevId, stVoChn.s32ChnId);
      }
    }
  }

  return 0;

failed:
  RKADK_DISP_SubStreamDeInit(pHandle);
  return ret;
}

static RKADK_S32 DispInit(RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  bool bSysInit = false;
  RKADK_DISP_VO_CHN_S stDispVoChn;
  MPP_CHN_S stViChn, stVoChn, stSrcVpssChn, stDstVpssChn;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  RKADK_LOGI("Disp u32CamId[%d] Init Start...", pHandle->u32CamId);

  bSysInit = RKADK_MPI_SYS_CHECK();
  if (!bSysInit) {
    RKADK_LOGE("System is not initialized");
    return -1;
  }

  RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(pHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pHandle->u32CamId);
    return -1;
  }

  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(pHandle->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  if (!RKADK_DISP_CheckParam(pstDispCfg))
    return -1;

  RKADK_DISP_SetChn(pHandle->u32CamId, pstDispCfg, &stViChn, &stVoChn,
                    &stSrcVpssChn, &stDstVpssChn);
  ret = RKADK_DISP_Enable(pHandle->u32CamId, pstDispCfg, pstSensorCfg);
  if (ret) {
    RKADK_LOGE("RKADK_DISP_Enable failed(%d)", ret);
    goto failed;
  }

  ret = RKADK_DISP_SubStreamInit(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_DISP_SubStreamInit failed(%d)", ret);
    goto failed;
  }

#if defined(RV1106_1103) || defined(RV1103B)
  char name[RKADK_THREAD_NAME_LEN];
  pHandle->bSendBuffer = true;
  ret = pthread_create(&pHandle->tid, NULL, RKADK_DISP_SendVoFrame, pHandle);
  if (ret) {
    RKADK_LOGE("Create get vpss mb(%d) thread failed %d", stSrcVpssChn.s32ChnId, ret);
    goto failed;
  }
  snprintf(name, sizeof(name), "SendVoFrame_%d_%d", stSrcVpssChn.s32DevId, stSrcVpssChn.s32ChnId);
  pthread_setname_np(pHandle->tid, name);
#else
  if (pstDispCfg->enable_vpss) {
    // Bind VPSS to VO
    ret = RKADK_MPI_SYS_Bind(&stSrcVpssChn, &stVoChn);
    if (ret) {
      RKADK_LOGE("Bind VPSS[%d, %d] to VO[%d] failed[%x]", stSrcVpssChn.s32DevId,
                  stSrcVpssChn.s32ChnId, stVoChn.s32ChnId, ret);
      goto failed;
    }
  }
#endif

  if (pstDispCfg->enable_vpss) {
    // VI Bind VPSS
    ret = RKADK_MPI_SYS_Bind(&stViChn, &stDstVpssChn);
    if (ret) {
      RKADK_LOGE("Bind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId,
                  stDstVpssChn.s32DevId, ret);
      goto failed;
    }
  } else {
    // VI Bind VO
    ret = RK_MPI_SYS_Bind(&stViChn, &stVoChn);
    if (ret) {
      RKADK_LOGE("Bind VI[%d] to VO[%d] failed[%x]", stViChn.s32ChnId, stVoChn.s32ChnId, ret);
      goto failed;
    }
  }

  RKADK_LOGI("Disp u32CamId[%d] Init End...", pHandle->u32CamId);
  return 0;

failed:
  RKADK_LOGE("Disp u32CamId[%d] Init failed...", pHandle->u32CamId);
  if (pstDispCfg->enable_vpss) {
    RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVoChn);
    RKADK_MPI_SYS_UnBind(&stViChn, &stDstVpssChn);
  } else {
    RK_MPI_SYS_UnBind(&stViChn, &stVoChn);
  }

  memset(&stDispVoChn, 0, sizeof(RKADK_DISP_VO_CHN_S));
  stDispVoChn.u32LayerId = pstDispCfg->vo_layer;
  stDispVoChn.u32DevId = pstDispCfg->vo_device;
  stDispVoChn.u32ChnId = pstDispCfg->vo_chn;
  RKADK_DISP_DestroyVo(stDispVoChn);

  if (pstDispCfg->enable_vpss)
    RKADK_MPI_VPSS_DeInit(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn);

  RKADK_MPI_VI_DeInit(pHandle->u32CamId, stViChn.s32ChnId);
  return ret;
}

static RKADK_S32 DispDeInit(RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_DISP_VO_CHN_S stDispVoChn;
  MPP_CHN_S stViChn, stVoChn, stSrcVpssChn, stDstVpssChn;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  RKADK_LOGI("Disp u32CamId[%d] DeInit Start...", pHandle->u32CamId);

  RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(pHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("Live RKADK_PARAM_GetDispCfg[%d] failed", pHandle->u32CamId);
    return -1;
  }

  ret = RKADK_DISP_SubStreamDeInit(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_DISP_SubStreamDeInit failed[%x]", ret);
    return ret;
  }

  RKADK_DISP_SetChn(pHandle->u32CamId, pstDispCfg, &stViChn,
                    &stVoChn, &stSrcVpssChn, &stDstVpssChn);

#if defined(RV1106_1103) || defined(RV1103B)
  pHandle->bSendBuffer = false;
  if (pHandle->tid) {
    RKADK_LOGD("Request to cancel get vpss[%d] mb thread...", stSrcVpssChn.s32ChnId);
    ret = pthread_join(pHandle->tid, NULL);
    if (ret)
      RKADK_LOGE("Exit get vpss[%d] mb thread failed!", stSrcVpssChn.s32ChnId);
    else
      RKADK_LOGI("Exit get vpss[%d] mb thread ok", stSrcVpssChn.s32ChnId);
    pHandle->tid = 0;
  }
#else
  if (pstDispCfg->enable_vpss) {
    // VPSS UnBind VO
    ret = RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVoChn);
    if (ret) {
      RKADK_LOGE("UnBind VPSS[%d, %d] to VO[%d] failed[%x]", stSrcVpssChn.s32DevId,
                stSrcVpssChn.s32ChnId, stVoChn.s32ChnId, ret);
      return ret;
    }
  }
#endif

  if (pstDispCfg->enable_vpss) {
    // VI UnBind VPSS
    ret = RKADK_MPI_SYS_UnBind(&stViChn, &stDstVpssChn);
    if (ret) {
      RKADK_LOGE("UnBind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId,
                  stDstVpssChn.s32DevId, ret);
      return ret;
    }
  } else {
    // VI UnBind VO
    ret = RK_MPI_SYS_UnBind(&stViChn, &stVoChn);
    if (ret) {
      RKADK_LOGE("UnBind VI[%d] to VO[%d] failed[%x]", stViChn.s32ChnId, stVoChn.s32ChnId, ret);
      return ret;
    }
  }

  memset(&stDispVoChn, 0, sizeof(RKADK_DISP_VO_CHN_S));
  stDispVoChn.u32LayerId = pstDispCfg->vo_layer;
  stDispVoChn.u32DevId = pstDispCfg->vo_device;
  stDispVoChn.u32ChnId = pstDispCfg->vo_chn;
  ret = RKADK_DISP_DestroyVo(stDispVoChn);
  if (ret) {
    RKADK_LOGE("Destory VO[%d] failed(%d)", stVoChn.s32ChnId, ret);
    return ret;
  }

  if (pstDispCfg->enable_vpss) {
    ret = RKADK_MPI_VPSS_DeInit(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn);
    if (ret) {
      RKADK_LOGE("DeInit VPSS[%d] failed[%x]", pstDispCfg->vpss_chn, ret);
      return ret;
    }
  }

  ret = RKADK_MPI_VI_DeInit(pHandle->u32CamId, stViChn.s32ChnId);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_DeInit failed(%d)", ret);
    return ret;
  }

  RKADK_LOGI("Disp u32CamId[%d] DeInit End...", pHandle->u32CamId);
  return 0;
}

RKADK_S32 RKADK_DISP_Init(RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  if (g_stDispHandle.bInit) {
    RKADK_LOGI("Disp u32CamId[%d] has been initialized", u32CamId);
    return 0;
  }

  g_stDispHandle.u32CamId = u32CamId;
  memset(g_stDispHandle.stSubStreamHandle, 0, sizeof(RKADK_DISP_STREAM_HANDLE_S) * RKADK_DISP_SUB_STREAM_NUM);
  ret = DispInit(&g_stDispHandle);
  if (ret) {
    RKADK_LOGE("Disp u32CamId[%d] Init failed(%d)", u32CamId, ret);
    return ret;
  }

  g_stDispHandle.bInit = true;
  return 0;
}

RKADK_S32 RKADK_DISP_DeInit(RKADK_U32 u32CamId) {
  int ret = 0;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  if (!g_stDispHandle.bInit || g_stDispHandle.u32CamId != u32CamId) {
    RKADK_LOGE("Disp u32CamId[%d] not initialized", u32CamId);
    return -1;
  }

  ret = DispDeInit(&g_stDispHandle);
  if (ret) {
    RKADK_LOGE("Disp u32CamId[%d] DeInit failed(%d)", u32CamId, ret);
    return ret;
  }

  g_stDispHandle.bInit = false;
  return 0;
}

RKADK_S32 RKADK_DISP_InitEx(RKADK_DISP_CFG_S *pstAttr, RKADK_MW_PTR *ppHandle) {
  int ret = 0;
  RKADK_DISP_HANDLE_S *pHandle = NULL;

  RKADK_CHECK_POINTER(pstAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstAttr->u32CamId, RKADK_FAILURE);

  if (*ppHandle) {
    RKADK_LOGE("Disp[%d] has been initialized", pstAttr->u32CamId);
    return -1;
  }

  pHandle = (RKADK_DISP_HANDLE_S *)malloc(sizeof(RKADK_DISP_HANDLE_S));
  if (!pHandle) {
    RKADK_LOGE("malloc display handle failed");
    return -1;
  }
  memset(pHandle, 0, sizeof(RKADK_DISP_HANDLE_S));

  pHandle->u32CamId = pstAttr->u32CamId;
  for(int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++)
    memcpy(&pHandle->stSubStreamHandle[i].stStreamCfg, &pstAttr->stSubStream[i], sizeof(RKADK_DISP_STREAM_CFG_S));

  ret = DispInit(pHandle);
  if (ret) {
    RKADK_LOGE("Disp u32CamId[%d] Init failed(%d)", pstAttr->u32CamId, ret);
    free(pHandle);
    return ret;
  }

  pHandle->bInit = true;
  *ppHandle = (RKADK_MW_PTR)pHandle;
  return 0;
}

RKADK_S32 RKADK_DISP_DeInitEx(RKADK_MW_PTR pHandle) {
  int ret = 0;
  RKADK_DISP_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_DISP_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

  pstHandle->bInit = false;
  ret = DispDeInit(pstHandle);
  if (ret) {
    RKADK_LOGE("Disp u32CamId[%d] DeInit failed(%d)", pstHandle->u32CamId, ret);
    return ret;
  }

  free(pHandle);
  return 0;
}

static RKADK_S32 DispPauseVoChn(RKADK_PARAM_DISP_CFG_S *pstDispCfg, RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_U32 u32VoLayer, u32VoChn;
  RKADK_DISP_STREAM_CFG_S *pstSubStream = NULL;

  ret = RK_MPI_VO_PauseChn(pstDispCfg->vo_layer, pstDispCfg->vo_chn);
  if (ret) {
    RKADK_LOGE("Pause vo[layer: %d, chn: %d] failed[%x]",
                pstDispCfg->vo_layer, pstDispCfg->vo_chn, ret);
    return ret;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    pstSubStream = &pHandle->stSubStreamHandle[i].stStreamCfg;
    if (!pstSubStream->bEnable)
      continue;

    u32VoLayer = pstDispCfg->vo_layer;
    u32VoChn = pstSubStream->stVoCfg.u32VoChn;
    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      RKADK_PARAM_DISP_CFG_S *pstSubDispCfg = RKADK_PARAM_GetDispCfg(pstSubStream->s32CamId);
      if (!pstSubDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstSubStream->s32CamId);
        return -1;
      }
      u32VoLayer = pstSubDispCfg->vo_layer;
      u32VoChn = pstSubDispCfg->vo_chn;
    }

    ret = RK_MPI_VO_PauseChn(u32VoLayer, u32VoChn);
    if (ret) {
      RKADK_LOGE("Pause vo[layer: %d, chn: %d] failed[%x]",
                  pstDispCfg->vo_layer, pstSubStream->stVoCfg.u32VoChn, ret);
      return ret;
    }

#ifndef RV1106_1103
    ret = RK_MPI_VO_PauseComposer(u32VoLayer);
    if (ret) {
      RKADK_LOGE("RK_MPI_VO_PauseComposer[%d] failed[%x]", u32VoLayer, ret);
      return ret;
    }
#endif
  }

#ifndef RV1106_1103
  ret = RK_MPI_VO_PauseComposer(pstDispCfg->vo_layer);
  if (ret) {
    RKADK_LOGE("RK_MPI_VO_PauseComposer[%d] failed[%x]", pstDispCfg->vo_layer, ret);
    return ret;
  }
#endif

  return 0;
}

static RKADK_S32 DispResumeVoChn(RKADK_PARAM_DISP_CFG_S *pstDispCfg, RKADK_DISP_HANDLE_S *pHandle) {
  int ret = 0;
  RKADK_U32 u32VoLayer, u32VoChn;
  RKADK_DISP_STREAM_CFG_S *pstSubStream = NULL;

  ret = RK_MPI_VO_ResumeChn(pstDispCfg->vo_layer, pstDispCfg->vo_chn);
  if (ret) {
    RKADK_LOGE("Resume vo[layer: %d, chn: %d] failed[%x]",
                pstDispCfg->vo_layer, pstDispCfg->vo_chn, ret);
    return ret;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    pstSubStream = &pHandle->stSubStreamHandle[i].stStreamCfg;
    if (!pstSubStream->bEnable)
      continue;

    u32VoLayer = pstDispCfg->vo_layer;
    u32VoChn = pstSubStream->stVoCfg.u32VoChn;
    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      RKADK_PARAM_DISP_CFG_S *pstSubDispCfg = RKADK_PARAM_GetDispCfg(pstSubStream->s32CamId);
      if (!pstSubDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstSubStream->s32CamId);
        return -1;
      }
      u32VoLayer = pstSubDispCfg->vo_layer;
      u32VoChn = pstSubDispCfg->vo_chn;
    }

    ret = RK_MPI_VO_ResumeChn(u32VoLayer, u32VoChn);
    if (ret) {
      RKADK_LOGE("Resume vo[layer: %d, chn: %d] failed[%x]", u32VoLayer, u32VoChn, ret);
      return ret;
    }

#ifndef RV1106_1103
    ret = RK_MPI_VO_ResumeComposer(u32VoLayer);
    if (ret) {
      RKADK_LOGE("RK_MPI_VO_ResumeComposer[%d] failed[%x]", u32VoLayer, ret);
      return ret;
    }
#endif

#if defined(RV1126B) || defined(RK3506)
    ret = RK_MPI_VO_SetLayerCleanMode(u32VoLayer, VO_LAYER_CLEAN_MODE_ONCE);
    if (ret) {
      RKADK_LOGE("RK_MPI_VO_SetLayerCleanMode[%d] failed[%x]", u32VoLayer, ret);
      return ret;
    }
#endif
  }

#ifndef RV1106_1103
  ret = RK_MPI_VO_ResumeComposer(pstDispCfg->vo_layer);
  if (ret) {
    RKADK_LOGE("RK_MPI_VO_ResumeComposer[%d] failed[%x]", pstDispCfg->vo_layer, ret);
    return ret;
  }
#endif

#if defined(RV1126B) || defined(RK3506)
  ret = RK_MPI_VO_SetLayerCleanMode(pstDispCfg->vo_layer, VO_LAYER_CLEAN_MODE_ONCE);
  if (ret) {
    RKADK_LOGE("RK_MPI_VO_SetLayerCleanMode[%d] failed[%x]", pstDispCfg->vo_layer, ret);
    return ret;
  }
#endif

  return 0;
}

static RKADK_S32 DispResetVpssAttr(RKADK_U32 u32VpssGrp, RKADK_U32 u32VpssChn, RKADK_RECT_S stVpssCropRect) {
  int ret = 0;
  VPSS_CHN_ATTR_S stVpssChnAttr;
  VPSS_CROP_INFO_S stVpssCropInfo;

  memset(&stVpssChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));
  memset(&stVpssCropInfo, 0, sizeof(VPSS_CROP_INFO_S));

  if (stVpssCropRect.u32Width > 0 && stVpssCropRect.u32Height > 0) {
    ret = RK_MPI_VPSS_GetChnCrop(u32VpssGrp, u32VpssChn, &stVpssCropInfo);
    if (ret != RK_SUCCESS) {
        RKADK_LOGE("Get Vpss[%d, %d] Crop attr failed[%x]", u32VpssGrp, u32VpssChn, ret);
        return ret;
    }

    if (stVpssCropInfo.stCropRect.s32X != stVpssCropRect.u32X
        || stVpssCropInfo.stCropRect.s32Y != stVpssCropRect.u32Y
        || stVpssCropInfo.stCropRect.u32Width != stVpssCropRect.u32Width
        || stVpssCropInfo.stCropRect.u32Height != stVpssCropRect.u32Height) {
      stVpssCropInfo.bEnable = RK_TRUE;
      stVpssCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
      stVpssCropInfo.stCropRect.s32X = stVpssCropRect.u32X;
      stVpssCropInfo.stCropRect.s32Y = stVpssCropRect.u32Y;
      stVpssCropInfo.stCropRect.u32Width = stVpssCropRect.u32Width;
      stVpssCropInfo.stCropRect.u32Height = stVpssCropRect.u32Height;
      ret = RK_MPI_VPSS_SetChnCrop(u32VpssGrp, u32VpssChn, &stVpssCropInfo);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("Set Vpss[%d, %d] Crop attr failed[%x]", u32VpssGrp, u32VpssChn, ret);
        return ret;
      }
    }

    ret = RK_MPI_VPSS_GetChnAttr(u32VpssGrp, u32VpssChn, &stVpssChnAttr);
    if (ret != RK_SUCCESS) {
        RKADK_LOGE("Get Vpss[%d, %d] attr attr failed[%x]", u32VpssGrp, u32VpssChn, ret);
        return ret;
    }

    if (stVpssChnAttr.u32Width != stVpssCropRect.u32Width
        || stVpssChnAttr.u32Height!= stVpssCropRect.u32Height) {
      stVpssChnAttr.u32Width = stVpssCropRect.u32Width;
      stVpssChnAttr.u32Height = stVpssCropRect.u32Height;

      ret = RK_MPI_VPSS_SetChnAttr(u32VpssGrp, u32VpssChn, &stVpssChnAttr);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("Set Vpss[%d, %d] attr attr failed[%x]", u32VpssGrp, u32VpssChn, ret);
        return ret;
      }
    }

    RKADK_LOGD("Reset vpss[%d, %d] rect: [%d, %d, %d, %d]",
                u32VpssGrp, u32VpssChn,
                stVpssCropRect.u32X, stVpssCropRect.u32Y,
                stVpssCropRect.u32Width, stVpssCropRect.u32Height);
  }

  return 0;
}

static RKADK_S32 DispResetVoAttr(RKADK_U32 u32VoLayer, RKADK_U32 u32VoChn, RKADK_S32 s32Priority, RKADK_RECT_S stVoRect) {
  int ret = 0;
  bool bResetAttr = false;
  VO_CHN_ATTR_S stVoChnAttr;

/*
  ret = RK_MPI_VO_RefreshChn(u32VoLayer, u32VoChn);
  if (ret) {
    RKADK_LOGE("Refresh vo [layer: %d, chn: %d] failed!, ret: %x", u32VoLayer, u32VoChn, ret);
    return ret;
  }
*/

  memset(&stVoChnAttr, 0, sizeof(VO_CHN_ATTR_S));
  ret = RK_MPI_VO_GetChnAttr(u32VoLayer, u32VoChn, &stVoChnAttr);
  if (ret) {
    RKADK_LOGE("Display get vo chn: %d attr failed[%d]", u32VoChn, ret);
    return -1;
  }

  if (s32Priority != -1 && stVoChnAttr.u32Priority != s32Priority) {
    RKADK_LOGD("vo[%d] old priority[%d], new priority[%d]", u32VoChn, stVoChnAttr.u32Priority, s32Priority);
    stVoChnAttr.u32Priority = s32Priority;
    bResetAttr = true;
  }

  if ((stVoRect.u32Width && stVoRect.u32X < stVoRect.u32Width)
      && (stVoRect.u32Height && stVoRect.u32Y < stVoRect.u32Height)) {
    if (stVoChnAttr.stRect.s32X != stVoRect.u32X
        || stVoChnAttr.stRect.s32Y != stVoRect.u32Y
        || (stVoRect.u32Width && stVoChnAttr.stRect.u32Width != stVoRect.u32Width)
        || (stVoRect.u32Height && stVoChnAttr.stRect.u32Height != stVoRect.u32Height)) {
      RKADK_LOGD("vo[%d] old rect[%d, %d, %d, %d] new rect[%d, %d, %d, %d]", u32VoChn,
                  stVoChnAttr.stRect.s32X, stVoChnAttr.stRect.s32Y,
                  stVoChnAttr.stRect.u32Width, stVoChnAttr.stRect.u32Height,
                  stVoRect.u32X, stVoRect.u32Y,
                  stVoRect.u32Width, stVoRect.u32Height);
      stVoChnAttr.stRect.s32X = stVoRect.u32X;
      stVoChnAttr.stRect.s32Y = stVoRect.u32Y;
      stVoChnAttr.stRect.u32Width = stVoRect.u32Width;
      stVoChnAttr.stRect.u32Height = stVoRect.u32Height;
      bResetAttr = true;
    }
  }

  if (bResetAttr) {
    ret = RK_MPI_VO_SetChnAttr(u32VoLayer, u32VoChn, &stVoChnAttr);
    if (ret) {
      RKADK_LOGE("Display set vo chn: %d attr failed[%d]", u32VoChn, ret);
      return -1;
    }

    RKADK_LOGD("Reset vo[%d] u32Priority[%d] rect[%d, %d, %d, %d]",
                u32VoChn, stVoChnAttr.u32Priority,
                stVoChnAttr.stRect.s32X, stVoChnAttr.stRect.s32Y,
                stVoChnAttr.stRect.u32Width, stVoChnAttr.stRect.u32Height);
  }

  return 0;
}

static RKADK_S32 DispResetVdecAttr(RKADK_DISP_STREAM_HANDLE_S *pstSubStreamHandle, RKADK_DISP_VDEC_ATTR_S *pstVdecAttr) {
  int ret;
  bool bResetAttr = false;
  VDEC_CHN_ATTR_S stVdecChnAttr;
  RKADK_DISP_VDEC_CFG_S *pstVdecCfg = &pstSubStreamHandle->stStreamCfg.stVdecCfg;

  memset(&stVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  ret = RK_MPI_VDEC_GetChnAttr(pstVdecCfg->u32VdecChn, &stVdecChnAttr);
  if (ret) {
    RKADK_LOGE("Get Vdec[%d] attr failed[%d]", pstVdecCfg->u32VdecChn, ret);
    return -1;
  }

  if (pstVdecAttr->enCodecType != RKADK_CODEC_TYPE_BUTT) {
    RK_CODEC_ID_E enType = RKADK_MEDIA_GetRkCodecType(pstVdecAttr->enCodecType);
    if (enType != stVdecChnAttr.enType) {
      pstVdecCfg->enCodecType = pstVdecAttr->enCodecType;
      bResetAttr = true;
      RKADK_LOGD("vdec[%d] old type[%d], new type[%d]", pstVdecCfg->u32VdecChn, stVdecChnAttr.enType, enType);
    }
  }

  if (pstVdecAttr->u32SrcWidth > 0 && pstVdecAttr->u32SrcHeight > 0) {
    if (pstVdecAttr->u32SrcWidth != stVdecChnAttr.u32PicWidth
      || pstVdecAttr->u32SrcHeight != stVdecChnAttr.u32PicHeight) {
      pstVdecCfg->u32SrcWidth = pstVdecAttr->u32SrcWidth;
      pstVdecCfg->u32SrcHeight = pstVdecAttr->u32SrcHeight;
      bResetAttr = true;
      RKADK_LOGD("vdec[%d] old res[%d, %d], new res[%d, %d]", pstVdecCfg->u32VdecChn,
                  stVdecChnAttr.u32PicWidth, stVdecChnAttr.u32PicHeight,
                  pstVdecCfg->u32SrcWidth, pstVdecCfg->u32SrcHeight);
    }
  }

  if (bResetAttr) {
    pstSubStreamHandle->bReseting = true;

    ret = DispDestroyVdec(pstVdecCfg);
    if (ret) {
      RKADK_LOGE("DispDestroyVdec[%d] failed[%x]", pstVdecCfg->u32VdecChn, ret);
      goto failed;
    }

    ret = DispCreateVdec(pstVdecCfg);
    if (ret) {
      RKADK_LOGE("DispCreateVdec[%d] failed[%x]", pstVdecCfg->u32VdecChn, ret);
      goto failed;
    }

    pstSubStreamHandle->bReseting = false;
  }

  return 0;

failed:
  pstSubStreamHandle->bReseting = false;
  return -1;
}

static RKADK_S32 DispSetAttr(RKADK_DISP_ATTR_S *pstAttr, RKADK_DISP_HANDLE_S *pHandle) {
  int ret;
  RKADK_U32 u32VoLayer, u32VoChn;
  RKADK_PARAM_DISP_CFG_S *pstSubDispCfg = NULL;
  RKADK_DISP_STREAM_CFG_S *pstSubStream = NULL;

  RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(pHandle->u32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pHandle->u32CamId);
    return -1;
  }

  if ((!RKADK_DISP_CheckRect(pstAttr->stVpssCropRect, pstDispCfg, true) ||
      !RKADK_DISP_CheckRect(pstAttr->stVoRect, pstDispCfg, false)))
    return -1;

  if (DispPauseVoChn(pstDispCfg, pHandle)) {
    RKADK_LOGE("DispPauseVoChn failed");
    return -1;
  }

  if (pstDispCfg->enable_vpss) {
    ret = DispResetVpssAttr(pstDispCfg->vpss_grp, pstDispCfg->vpss_chn, pstAttr->stVpssCropRect);
    if (ret) {
      RKADK_LOGE("DispResetVpssAttr[%d, %d] failed", pstDispCfg->vpss_grp, pstDispCfg->vpss_chn);
      return -1;
    }
  }

  ret = DispResetVoAttr(pstDispCfg->vo_layer, pstDispCfg->vo_chn, pstAttr->s32Priority, pstAttr->stVoRect);
  if (ret) {
    RKADK_LOGE("DispResetVoAttr[%d, %d] failed", pstDispCfg->vo_layer, pstDispCfg->vo_chn);
    return -1;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    pstSubStream = &pHandle->stSubStreamHandle[i].stStreamCfg;
    if (!pstSubStream->bEnable)
      continue;

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_THIRD_STREAM) {
      ret = DispResetVdecAttr(&pHandle->stSubStreamHandle[i], &pstAttr->stSubStreamAttr[i].stVdecAttr);
      if (ret) {
        RKADK_LOGE("DispResetVdecAttr[%d] failed", pstSubStream->stVdecCfg.u32VdecChn);
        return -1;
      }
    }

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      pstSubDispCfg = RKADK_PARAM_GetDispCfg(pstSubStream->s32CamId);
      if (!pstSubDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", pstSubStream->s32CamId);
        return -1;
      }

      ret = DispResetVpssAttr(pstSubDispCfg->vpss_grp, pstSubDispCfg->vpss_chn, pstAttr->stSubStreamAttr[i].stVpssCropRect);
      if (ret) {
        RKADK_LOGE("DispResetVpssAttr[%d, %d] failed", pstSubDispCfg->vpss_grp, pstSubDispCfg->vpss_chn);
        return -1;
      }
    }

    if (pstSubStream->enStreamType == RKADK_STREAM_TYPE_DISP) {
      u32VoLayer = pstSubDispCfg->vo_layer;
      u32VoChn = pstSubDispCfg->vo_chn;
    } else {
      u32VoLayer = pstDispCfg->vo_layer;
      u32VoChn = pstSubStream->stVoCfg.u32VoChn;
    }

    ret = DispResetVoAttr(u32VoLayer, u32VoChn, pstAttr->stSubStreamAttr[i].s32Priority, pstAttr->stSubStreamAttr[i].stVoRect);
    if (ret) {
      RKADK_LOGE("DispResetVoAttr[%d, %d] failed", u32VoLayer, u32VoChn);
      return -1;
    }
  }

  if (DispResumeVoChn(pstDispCfg, pHandle))
    RKADK_LOGE("DispResumeVoChn failed");

  return 0;
}

RKADK_S32 RKADK_DISP_SetAttr(RKADK_U32 u32CamId, RKADK_DISP_ATTR_S *pstAttr) {
  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstAttr, RKADK_FAILURE);

  if (!g_stDispHandle.bInit || g_stDispHandle.u32CamId != u32CamId) {
    RKADK_LOGE("Disp u32CamId[%d] not initialized", u32CamId);
    return -1;
  }

  pstAttr->s32Priority = -1;
  return DispSetAttr(pstAttr, &g_stDispHandle);
}

RKADK_S32 RKADK_DISP_SetAttrEx(RKADK_DISP_ATTR_S *pstAttr, RKADK_MW_PTR pHandle) {
  RKADK_DISP_HANDLE_S *pstHandle;

  RKADK_CHECK_POINTER(pstAttr, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstHandle = (RKADK_DISP_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstHandle->u32CamId, RKADK_FAILURE);

  return DispSetAttr(pstAttr, pstHandle);
}
