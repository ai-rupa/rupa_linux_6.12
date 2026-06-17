/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "uvc_mpi_vi.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_vi.h"
#include "uvc_log.h"
#include "uvc_video.h"
#include <cstring>

typedef struct _rkMpiVICtx {
  UVC_VI_CHN_CTX_S stChnCtx;
  COMPRESS_MODE_E enCompressMode;
  VI_DEV_ATTR_S stDevAttr;
  VI_DEV_BIND_PIPE_S stBindPipe;
  VI_CHN_ATTR_S stChnAttr;
  VI_CHN_STATUS_S stChnStatus;
  VI_PARAM_MOD_S stModParam;
} MPI_VI_CTX_S;

UVC_VI_CHN_CTX_S uvc_get_vi_chn_ctx(UVC_MPI_CFG uvcCfg,
                                    MpiViChannelType chnType) {
  UVC_VI_CHN_CTX_S viChnCtx;
  memset(&viChnCtx, 0, sizeof(UVC_VI_CHN_CTX_S));

  viChnCtx.devId = uvcCfg.vi_cfg[chnType].dev_id;
  viChnCtx.pipeId = viChnCtx.devId;
  viChnCtx.channelId = uvcCfg.vi_cfg[chnType].channel_id;

  return viChnCtx;
}

static MPI_VI_CTX_S uvc_get_vi_ctx(UVC_MPI_CFG *uvcCfg,
                                   MpiViChannelType chnType) {
  MPI_VI_CTX_S viCtx;
  memset(&viCtx, 0, sizeof(MPI_VI_CTX_S));

  LOG_INFO("chnType:%d uvc out:%dx%d, vi assign_resolution:%dx%d vi_name:%s\n",
           chnType, uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height,
           uvcCfg->vi_cfg[chnType].assign_width,
           uvcCfg->vi_cfg[chnType].assign_height,
           uvcCfg->vi_cfg[chnType].dev_name);

  //  this is for uvc vi/nn vi different cfg.
  switch (chnType) {
  case MPI_VI_CHANNEL_TYPE_UVC:
  case MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME:
    if (uvcCfg->vi_cfg[chnType].assign_width <= 0 ||
        uvcCfg->vi_cfg[chnType].assign_height <=
            0) { // not assign the vi uvc output
      if (uvcCfg->uvc_cfg.width < uvcCfg->vi_cfg[chnType].min_width ||
          uvcCfg->uvc_cfg.height <
              uvcCfg->vi_cfg[chnType]
                  .min_height) { // not support this resolution(too small)
        uvcCfg->vi_cfg[chnType].assign_width =
            uvcCfg->vi_cfg[chnType].min_width;
        uvcCfg->vi_cfg[chnType].assign_height =
            uvcCfg->vi_cfg[chnType].min_height;
        LOG_INFO("uvc out:%dx%d, the min resolution:%dx%d\n",
                 uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height,
                 uvcCfg->vi_cfg[chnType].min_width,
                 uvcCfg->vi_cfg[chnType].min_height);
      } else if (uvcCfg->uvc_cfg.width > uvcCfg->vi_cfg[chnType].max_width ||
                 uvcCfg->uvc_cfg.height >
                     uvcCfg->vi_cfg[chnType].max_height) { // not support this
                                                           // resolution(too
                                                           // large)
        uvcCfg->vi_cfg[chnType].assign_width =
            uvcCfg->vi_cfg[chnType].max_width;
        uvcCfg->vi_cfg[chnType].assign_height =
            uvcCfg->vi_cfg[chnType].max_height;
        LOG_INFO("uvc out:%dx%d, the max resolution:%dx%d\n",
                 uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height,
                 uvcCfg->vi_cfg[chnType].max_width,
                 uvcCfg->vi_cfg[chnType].max_height);
      } else {
        uvcCfg->vi_cfg[chnType].assign_width = uvcCfg->uvc_cfg.width;
        uvcCfg->vi_cfg[chnType].assign_height = uvcCfg->uvc_cfg.height;
      }
    }

    viCtx.stChnAttr.u32Depth = uvcCfg->vi_cfg[chnType].depth;

    switch (uvcCfg->uvc_cfg.fcc) {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_UYVY:
      viCtx.stChnAttr.enPixelFormat =
          uvcCfg->vi_cfg[chnType].format;
      break;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_HEVC:
      viCtx.stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
      break;
    default:
      LOG_ERROR("vi not support this fcc 0x%x\n", uvcCfg->uvc_cfg.fcc);
      break;
    }
    break;
  case MPI_VI_CHANNEL_TYPE_NN:
    viCtx.stChnAttr.enPixelFormat = uvcCfg->vi_cfg[chnType].format;
    viCtx.stChnAttr.u32Depth = uvcCfg->vi_cfg[chnType].depth;
    break;
  default:
    LOG_ERROR("vi not support this chnType 0x%x\n", chnType);
    break;
  }

  // common cfg
  viCtx.stChnCtx = uvc_get_vi_chn_ctx(*uvcCfg, chnType);
  viCtx.stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_FALSE;
  viCtx.stChnAttr.stIspOpt.u32BufCount = uvcCfg->vi_cfg[chnType].buf_cnt;
  viCtx.stChnAttr.stIspOpt.u32BufSize =
      0; // uvcCfg.vi_cfg[chnType].width * uvcCfg.vi_cfg[chnType].height * 2;
         // uvcCfg.uvc_cfg.width * uvcCfg.uvc_cfg.height;
  viCtx.stChnAttr.stIspOpt.enMemoryType = uvcCfg->vi_cfg[chnType].memory_type;
  if (strncmp(uvcCfg->vi_cfg[chnType].dev_name, "null", strlen("null"))) {
    memcpy(viCtx.stChnAttr.stIspOpt.aEntityName,
           uvcCfg->vi_cfg[chnType].dev_name,
           strlen(uvcCfg->vi_cfg[chnType].dev_name));
  }
  viCtx.stChnAttr.stFrameRate.s32SrcFrameRate =
      uvcCfg->vi_cfg[chnType].fps_in > 0 ? uvcCfg->vi_cfg[chnType].fps_in : -1;
  viCtx.stChnAttr.stFrameRate.s32DstFrameRate =
      uvcCfg->vi_cfg[chnType].fps_out > 0 ? uvcCfg->vi_cfg[chnType].fps_out
                                          : -1;
  viCtx.stChnAttr.stSize.u32Width = uvcCfg->vi_cfg[chnType].assign_width;
  viCtx.stChnAttr.stSize.u32Height = uvcCfg->vi_cfg[chnType].assign_height;
  viCtx.stChnAttr.stIspOpt.stMaxSize.u32Width = uvcCfg->vi_cfg[chnType].max_width;
  viCtx.stChnAttr.stIspOpt.stMaxSize.u32Height = uvcCfg->vi_cfg[chnType].max_height;
  LOG_INFO("chnType:%d uvc out:%dx%d, vi resolution:%dx%d name:%s\n", chnType,
           uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height,
           viCtx.stChnAttr.stSize.u32Width, viCtx.stChnAttr.stSize.u32Height,
           viCtx.stChnAttr.stIspOpt.aEntityName);

  return viCtx;
}

// todo: multi vi from cfg.
RK_S32 uvc_vi_config(UVC_MPI_CFG *uvcCfg, MpiViChannelType chnType) {
  RK_S32 s32Ret = RK_FAILURE;
  MPI_VI_CTX_S ctx;
  ctx = uvc_get_vi_ctx(uvcCfg, chnType);

  if (uvcCfg->vi_cfg[chnType].isp_online) {
    ctx.stModParam.enViModType = VI_DEV_PIPE_MODE;
    ctx.stModParam.stDevPipeModParam.enDevPipeMode = VI_DEV_PIPE_LEFT_HALF_ONLINE;
    s32Ret = RK_MPI_VI_SetModParam(&ctx.stModParam);
    if (s32Ret != RK_SUCCESS) {
      LOG_ERROR("RK_MPI_VI_SetModParam failed %#X", s32Ret);
      goto __FAILED;
    }
  }

#ifdef RV1126B
  s32Ret = RK_MPI_VI_CtrlRawStream(uvcCfg->vi_cfg[chnType].dev_id,RK_FALSE);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_CtrlRawStream %x\n", s32Ret);
    goto __FAILED;
  }
#endif

  if(uvcCfg->vi_cfg[chnType].dev_id != VI_MAX_PHY_PIPE_NUM) {
    // 0. get dev config status
    s32Ret = RK_MPI_VI_GetDevAttr(ctx.stChnCtx.devId, &ctx.stDevAttr);
    if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
      // 0-1.config dev
      s32Ret = RK_MPI_VI_SetDevAttr(ctx.stChnCtx.devId, &ctx.stDevAttr);
      if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetDevAttr %x\n", s32Ret);
        goto __FAILED;
      }
    }
    // 1.get  dev enable status
    s32Ret = RK_MPI_VI_GetDevIsEnable(ctx.stChnCtx.devId);
    if (s32Ret != RK_SUCCESS) {
      // 1-2.enable dev
      s32Ret = RK_MPI_VI_EnableDev(ctx.stChnCtx.devId);
      if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_EnableDev %x\n", s32Ret);
        goto __FAILED;
      }
      // 1-3.bind dev/pipe
      ctx.stBindPipe.u32Num = ctx.stChnCtx.pipeId;
      ctx.stBindPipe.PipeId[0] = ctx.stChnCtx.pipeId;
      s32Ret = RK_MPI_VI_SetDevBindPipe(ctx.stChnCtx.devId, &ctx.stBindPipe);
      if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetDevBindPipe %x\n", s32Ret);
        goto __FAILED;
      }
    }
  }

  // 2.config channel
  s32Ret = RK_MPI_VI_SetChnAttr(ctx.stChnCtx.pipeId, ctx.stChnCtx.channelId,
                                &ctx.stChnAttr);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_SetChnAttr %x\n", s32Ret);
    goto __FAILED;
  }

  // use wrap encoder mode
  if (uvcCfg->vi_cfg[chnType].wrap_enable &&
      (uvcCfg->uvc_cfg.fcc == V4L2_PIX_FMT_MJPEG ||
       uvcCfg->uvc_cfg.fcc == V4L2_PIX_FMT_H264 ||
       uvcCfg->uvc_cfg.fcc == V4L2_PIX_FMT_HEVC))
  {
    VI_CHN_BUF_WRAP_S stViWrap;
    memset(&stViWrap, 0, sizeof(VI_CHN_BUF_WRAP_S));
    stViWrap.bEnable = RK_TRUE;
    stViWrap.u32BufLine = uvcCfg->vi_cfg[chnType].wrap_buffer_line;
    stViWrap.u32WrapBufferSize = stViWrap.u32BufLine * uvcCfg->vi_cfg[chnType].assign_width * 3 / 2;
    s32Ret = RK_MPI_VI_SetChnWrapBufAttr(ctx.stChnCtx.pipeId, ctx.stChnCtx.channelId, &stViWrap);
    if (s32Ret != RK_SUCCESS)
    {
      LOG_ERROR("RK_MPI_VI_SetChnWrapBufAttr failure:%#X\n", s32Ret);
      goto __FAILED;
    }
  }

__FAILED:
  return s32Ret;
}

RK_S32 uvc_vi_start(UVC_MPI_CFG uvcCfg, MpiViChannelType chnType) {
  RK_S32 s32Ret = RK_FAILURE;
  UVC_VI_CHN_CTX_S ctx;
  ctx = uvc_get_vi_chn_ctx(uvcCfg, chnType);

  // isp&aiq init here

  // 3.enable channel
  LOG_INFO("RK_MPI_VI_EnableChn %x %d %d\n", ctx.devId, ctx.pipeId,
           ctx.channelId);
  s32Ret = RK_MPI_VI_EnableChn(ctx.pipeId, ctx.channelId);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_EnableChn %x\n", s32Ret);
    goto __FAILED;
  }

__FAILED:
  return s32Ret;
}

RK_S32 uvc_vi_stop(UVC_MPI_CFG uvcCfg, MpiViChannelType chnType) {
  RK_S32 s32Ret = RK_FAILURE;
  UVC_VI_CHN_CTX_S ctx;
  ctx = uvc_get_vi_chn_ctx(uvcCfg, chnType);
  // 5. disable one chn
  s32Ret = RK_MPI_VI_DisableChn(ctx.pipeId, ctx.channelId);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_DisableChn %x\n", s32Ret);
    goto __FAILED;
  }

  if(uvcCfg.vi_cfg[chnType].dev_id == VI_MAX_PHY_PIPE_NUM) {
    LOG_INFO("vi_cfg[%d].dev_id is VI_MAX_PHY_PIPE_NUM, skip disable\n", chnType);
    return RK_SUCCESS;
  }
  // 6.disable dev(will diabled all chn)
  s32Ret = RK_MPI_VI_DisableDev(ctx.devId);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_DisableDev %x\n", s32Ret);
    goto __FAILED;
  }

// isp&aiq deinit here

__FAILED:
  return s32Ret;
}

RK_S32 uvc_vi_config_crop(UVC_MPI_CFG uvcCfg, VI_CROP_INFO_S stCropInfo,
                            MpiViChannelType chnType) {
  RK_S32 s32Ret = RK_FAILURE;
  UVC_VI_CHN_CTX_S ctx;
  ctx = uvc_get_vi_chn_ctx(uvcCfg, chnType);
  s32Ret = RK_MPI_VI_SetEptz(ctx.pipeId, ctx.channelId, stCropInfo);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_VI_SetChnCrop fail 0x%x\n", s32Ret);
    return s32Ret;
  }
  return s32Ret;
}
