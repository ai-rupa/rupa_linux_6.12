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
#include "uvc_process.h"
#include "eptz.h"
#include "nn_process.h"
#include "osd.h"
#include "uvc_mpi_config.h"
#include "uvc_mpi_venc.h"
#include "uvc_mpi_vi.h"
#include "uvc_mpi_vpss.h"
#include "uvc_video.h"

#include "uvc_log.h"
#include <cstring>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_rgn.h"
#include "rk_mpi_sys.h"
#include <sys/prctl.h>
#include <thread>
#include "camera_control.h"

#define UVC_DYNAMIC_DEBUG_OUT_CHECK "/tmp/uvc_out"
#define UVC_DYNAMIC_DEBUG_FPS "/tmp/uvc_fps"

#define RK_ALIGN(x, a)         (((x) + (a) - 1) & ~((a) - 1))

static pthread_mutex_t gUvcProcessLock = PTHREAD_MUTEX_INITIALIZER;
static UVC_USERDATA_CALLBACK_T gUvcUserdataCallback[CAM_MAX_NUM];
static UVCProcess *gUvcProcess[CAM_MAX_NUM];

typedef enum _UVC_FMT_TYPE {
  UVC_FMT_TYPE_VI_UVC,             // now only run VI_VPSS_UVC
  UVC_FMT_TYPE_VI_VENC_UVC,        // now only run VI_VPSS_VENC_UVC
  UVC_FMT_TYPE_VI_VPSS_UVC,
  UVC_FMT_TYPE_VI_VPSS_VENC_UVC,
  UVC_FMT_TYPE_MAX,
} UVCFmtType;

typedef struct _UVC_PROCESS_BUFF {
  int fd;
  void *frame;
} UVCProcessBuff;

typedef struct _UVC_VENC_TEMP_INFO {
  VENC_STREAM_S stVencFrame;
  VENC_STREAM_S stReleaseVencFrame;
  int force_idr_cnt;
} UVCVencTempInfo;

typedef struct _UVC_DEBUG_FILE_INFO {
  int debugCnt;
  FILE *pDebugFile;
} UVCDegbuFileInfo;

typedef struct _UVC_PROCESS_CTX {
  UVC_CTRL_INFO mUvcCtx;
  UVC_MPI_CFG mUvcCfg;
  UVC_VI_CHN_CTX_S viChnCtx;
  UVC_VI_CHN_CTX_S viChnCtxIrDarkFrame;
  UVC_VENC_CHN_CTX_S vencChnCtx;
  UVC_VPSS_CHN_CTX_S vpssChnCtx;
  EPTZ_CTX_S eptzCtx;
  NNProcess *nnCtx;
  UVCVencTempInfo vencTempInfo;
  UVCFmtType mFmtType;
  UVCDegbuFileInfo mUvcDebug;
  int mUvcBuffCount;
  bool mStart;
  std::thread *mThread;
  pthread_cond_t mCond;
  pthread_mutex_t mCondMutex;
  struct uvc_buffer *mUvcBuf;
  MB_POOL debugPool;
  int getBufCnt;
  int releaseBufCnt;
  struct timeval enterTime;
  int sendCount;
  struct uvc_device *dev;
  UVC_USERDATA_CALLBACK_P mUserdataCallback;
} UVCProcessCtx;

static UVC_RET_TYPE do_process(UVCProcessCtx *ctx);

extern "C" int32_t uvc_process_register_userdata_cb(
  UVC_USERDATA_CALLBACK_P UserdataCallback) {
  if (UserdataCallback && UserdataCallback->index >= CAM_MAX_NUM) {
    LOG_ERROR("index:%d exceed max:%d\n", UserdataCallback->index, CAM_MAX_NUM);
    return -1;
  }
  memcpy(&gUvcUserdataCallback[UserdataCallback->index], UserdataCallback,
         sizeof(UVC_USERDATA_CALLBACK_T));
  return 0;
}

extern "C" int32_t uvc_process_unregister_userdata_cb(int index) {
  if (index >= CAM_MAX_NUM) {
    LOG_ERROR("index:%d exceed max:%d\n", index, CAM_MAX_NUM);
    return -1;
  }
  memset(&gUvcUserdataCallback[index], 0, sizeof(UVC_USERDATA_CALLBACK_T));
  return 0;
}

// need to actually test whether need this part for prepare.
extern "C" int uvc_process_config(UVC_CTRL_INFO uvcCtrlInfo) {
  RK_S32 s32Ret = RK_FAILURE;

  LOG_INFO(
      "index:%d width = %d, height = %d, fcc = %d, fps = %d, auto_eptz = %d\n",
      uvcCtrlInfo.index, uvcCtrlInfo.width, uvcCtrlInfo.height, uvcCtrlInfo.fcc,
      uvcCtrlInfo.fps, uvcCtrlInfo.eptz[UVC_EPTZ_AUTO]);

  if ((uvcCtrlInfo.fcc == V4L2_PIX_FMT_H264) &&
      !access("/tmp/use_encodec_h265", 0)) {
    LOG_ERROR("force use h265 encodec\n");
    uvcCtrlInfo.fcc = V4L2_PIX_FMT_HEVC;
  }

  if (uvcCtrlInfo.index >= CAM_MAX_NUM) {
    LOG_ERROR("index:%d exceed max:%d\n", uvcCtrlInfo.index, CAM_MAX_NUM);
    return -1;
  }

  if (gUvcProcess[uvcCtrlInfo.index] == NULL) {
    LOG_INFO("new uvc process index %d\n", uvcCtrlInfo.index);
    gUvcProcess[uvcCtrlInfo.index] = new UVCProcess();
  } else {
    LOG_INFO("old uvc process index %d\n", uvcCtrlInfo.index);
  }
  return gUvcProcess[uvcCtrlInfo.index]->configProcess(uvcCtrlInfo);
}

extern "C" int uvc_process_config_eptz(UVC_CTRL_INFO uvcCtrlInfo) {
  RK_S32 s32Ret = RK_FAILURE;
  LOG_INFO(
      "index:%d width = %d, height = %d, fcc = %d, fps = %d, eptz[%d] = %d\n",
      uvcCtrlInfo.index, uvcCtrlInfo.width, uvcCtrlInfo.height, uvcCtrlInfo.fcc,
      uvcCtrlInfo.fps, uvcCtrlInfo.type, uvcCtrlInfo.eptz[uvcCtrlInfo.type]);

  if (uvcCtrlInfo.index >= CAM_MAX_NUM) {
    LOG_ERROR("index:%d exceed max:%d\n", uvcCtrlInfo.index, CAM_MAX_NUM);
    return -1;
  }
  if (gUvcProcess[uvcCtrlInfo.index] == NULL) {
    LOG_ERROR("uvc process index %d not prepared\n", uvcCtrlInfo.index);
    return -1;
  }

  return gUvcProcess[uvcCtrlInfo.index]->configEptz(uvcCtrlInfo);
}

extern "C" int uvc_process_start(UVC_CTRL_INFO uvcCtrlInfo) {
  RK_S32 s32Ret = RK_FAILURE;

  LOG_INFO("index:%d width = %d, height = %d, fcc = %d, fps = %d, eptz = %d \n",
           uvcCtrlInfo.index, uvcCtrlInfo.width, uvcCtrlInfo.height,
           uvcCtrlInfo.fcc, uvcCtrlInfo.fps, uvcCtrlInfo.eptz[UVC_EPTZ_AUTO]);

  if (uvcCtrlInfo.index >= CAM_MAX_NUM) {
    LOG_ERROR("index:%d exceed max:%d\n", uvcCtrlInfo.index, CAM_MAX_NUM);
    return -1;
  }
  if (gUvcProcess[uvcCtrlInfo.index]) {
    gUvcProcess[uvcCtrlInfo.index]->startProcess();
  }

  return 0;
}

extern "C" void uvc_process_stop(UVC_CTRL_INFO uvcCtrlInfo) {
  LOG_INFO("stop uvc process index %d\n", uvcCtrlInfo.index);

  if (gUvcProcess[uvcCtrlInfo.index]) {
    gUvcProcess[uvcCtrlInfo.index]->stopProcess();
  }
}

extern "C" void uvc_process_delete(UVC_CTRL_INFO uvcCtrlInfo) {
  LOG_INFO("delete uvc process index %d %p\n", uvcCtrlInfo.index,
           gUvcProcess[uvcCtrlInfo.index]);

  if (gUvcProcess[uvcCtrlInfo.index]) {
    delete gUvcProcess[uvcCtrlInfo.index];
    gUvcProcess[uvcCtrlInfo.index] = nullptr;
  }
}

extern "C" int uvc_process_release_frame(UVC_CTRL_INFO uvcCtrlInfo,
                                          struct uvc_buffer *buffer) {
  if (gUvcProcess[uvcCtrlInfo.index]) {
    gUvcProcess[uvcCtrlInfo.index]->releaseFrame(buffer);
  }

  return 0;
}

UVCProcessCtx *getUVCProcessCtx(void *ctx) {
  return reinterpret_cast<UVCProcessCtx *>(ctx);
}

int UVCProcess::initialize() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifdef UVC_ENABLE_NN
  ctx->nnCtx = new NNProcess();
#endif
  uvc_mpi_cfg_set_default(&ctx->mUvcCfg);

  return 0;
}

int UVCProcess::deinitialize() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);

#ifdef UVC_ENABLE_NN
  if (ctx->nnCtx){
    ctx->nnCtx->stopProcess();
    delete ctx->nnCtx;
  }
#endif

  if (ctx)
    free(ctx);

  return 0;
}

UVCProcess::UVCProcess() : mCtx(NULL) {
  UVCProcessCtx *ctx = (UVCProcessCtx *)calloc(sizeof(UVCProcessCtx), 1);
  mCtx = reinterpret_cast<void *>(ctx);

  initialize();
}

UVCProcess::~UVCProcess() { deinitialize(); }

int UVCProcess::configVi() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_vi_config(&ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_UVC);
#endif
  ctx->viChnCtx = uvc_get_vi_chn_ctx(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_UVC);

  if(ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame) {
    uvc_vi_config(&ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME);
    ctx->viChnCtxIrDarkFrame = uvc_get_vi_chn_ctx(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME);
  }
  return 0;
}

int UVCProcess::configVenc() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_venc_config(&ctx->mUvcCfg);
#endif
  ctx->vencChnCtx = uvc_get_venc_chn_ctx(ctx->mUvcCfg);

  return 0;
}

int UVCProcess::configVpss() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  uvc_vpss_config(&ctx->mUvcCfg, MPI_VPSS_CHANNEL_TYPE_UVC);
  ctx->vpssChnCtx =
      uvc_get_vpss_chn_ctx(ctx->mUvcCfg, MPI_VPSS_CHANNEL_TYPE_UVC);

  return 0;
}

int UVCProcess::startVi() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  if(ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame) {
    uvc_vi_start(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME);
  }
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_vi_start(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_UVC);
#endif
  return 0;
}

int UVCProcess::stopVi() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_vi_stop(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_UVC);
#endif
  if(ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame)
    uvc_vi_stop(ctx->mUvcCfg, MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME);
  return 0;
}

int UVCProcess::startVpss() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  uvc_vpss_start(ctx->mUvcCfg, MPI_VPSS_CHANNEL_TYPE_UVC);

  return 0;
}

int UVCProcess::stopVpss() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  uvc_vpss_stop(ctx->mUvcCfg, MPI_VPSS_CHANNEL_TYPE_UVC);

  return 0;
}

int UVCProcess::startVenc() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_venc_start(ctx->mUvcCfg);
#endif
  ctx->vencTempInfo.stVencFrame.pstPack =
      reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));
  ctx->vencTempInfo.stReleaseVencFrame.pstPack =
      reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));

  return 0;
}

int UVCProcess::stopVenc() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (!ctx->mUvcCfg.common_cfg.userdata_enable)
    uvc_venc_stop(ctx->mUvcCfg);
#endif
  if (ctx->vencTempInfo.stVencFrame.pstPack)
    free(ctx->vencTempInfo.stVencFrame.pstPack);
  if (ctx->vencTempInfo.stReleaseVencFrame.pstPack)
    free(ctx->vencTempInfo.stReleaseVencFrame.pstPack);

  return 0;
}

int UVCProcess::bindViVpss() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  mSrcChn.enModId = RK_ID_VI;
  mSrcChn.s32DevId = ctx->viChnCtx.devId;
  mSrcChn.s32ChnId = ctx->viChnCtx.channelId;
  mDestChn.enModId = RK_ID_VPSS;
  mDestChn.s32DevId = ctx->vpssChnCtx.group;
  mDestChn.s32ChnId = ctx->vpssChnCtx.channelId;
  ret = RK_MPI_SYS_Bind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::unBindViVpss() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  mSrcChn.enModId = RK_ID_VI;
  mSrcChn.s32DevId = ctx->viChnCtx.devId;
  mSrcChn.s32ChnId = ctx->viChnCtx.channelId;
  mDestChn.enModId = RK_ID_VPSS;
  mDestChn.s32DevId = ctx->vpssChnCtx.group;
  mDestChn.s32ChnId = ctx->vpssChnCtx.channelId;
  ret = RK_MPI_SYS_UnBind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::bindViVenc() {
#ifdef RK_UVC_ENABLE_FASTBOOT
  return 0;
#endif

  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  if (ctx->mUvcCfg.common_cfg.userdata_enable)
    return 0;

  mSrcChn.enModId = RK_ID_VI;
  mSrcChn.s32DevId = ctx->viChnCtx.devId;
  mSrcChn.s32ChnId = ctx->viChnCtx.channelId;
  mDestChn.enModId = RK_ID_VENC;
  mDestChn.s32DevId = ctx->vencChnCtx.devId;
  mDestChn.s32ChnId = ctx->vencChnCtx.channelId;
  ret = RK_MPI_SYS_Bind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::unBindViVenc() {
#ifdef RK_UVC_ENABLE_FASTBOOT
  return 0;
#endif

  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  if (ctx->mUvcCfg.common_cfg.userdata_enable)
    return 0;

  mSrcChn.enModId = RK_ID_VI;
  mSrcChn.s32DevId = ctx->viChnCtx.devId;
  mSrcChn.s32ChnId = ctx->viChnCtx.channelId;
  mDestChn.enModId = RK_ID_VENC;
  mDestChn.s32DevId = ctx->vencChnCtx.devId;
  mDestChn.s32ChnId = ctx->vencChnCtx.channelId;
  ret = RK_MPI_SYS_UnBind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::bindVpssVenc() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  mSrcChn.enModId = RK_ID_VPSS;
  mSrcChn.s32DevId = ctx->vpssChnCtx.group;
  mSrcChn.s32ChnId = ctx->vpssChnCtx.channelId;
  mDestChn.enModId = RK_ID_VENC;
  mDestChn.s32DevId = ctx->vencChnCtx.devId;
  mDestChn.s32ChnId = ctx->vencChnCtx.channelId;
  ret = RK_MPI_SYS_Bind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::unBindVpssVenc() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;
  MPP_CHN_S mSrcChn;
  MPP_CHN_S mDestChn;

  mSrcChn.enModId = RK_ID_VPSS;
  mSrcChn.s32DevId = ctx->vpssChnCtx.group;
  mSrcChn.s32ChnId = ctx->vpssChnCtx.channelId;
  mDestChn.enModId = RK_ID_VENC;
  mDestChn.s32DevId = ctx->vencChnCtx.devId;
  mDestChn.s32ChnId = ctx->vencChnCtx.channelId;
  ret = RK_MPI_SYS_UnBind(&mSrcChn, &mDestChn);

  return ret;
}

int UVCProcess::configProcess(UVC_CTRL_INFO uvcCtrlInfo) {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int32_t use_time_us, now_time_us, last_time_us;
  struct timespec now_tm = {0, 0};
  int check_ret;

  if (memcmp(&ctx->mUvcCtx, &uvcCtrlInfo, sizeof(UVC_CTRL_INFO))) {
    LOG_INFO("index:%d config process paramter change.\n", uvcCtrlInfo.index);
    memcpy(&ctx->mUvcCtx, &uvcCtrlInfo, sizeof(UVC_CTRL_INFO));
    ctx->mUvcCfg.index = uvcCtrlInfo.index;
    ctx->mUvcCfg.uvc_cfg.width = uvcCtrlInfo.width;
    ctx->mUvcCfg.uvc_cfg.height = uvcCtrlInfo.height;
    ctx->mUvcCfg.uvc_cfg.fps = uvcCtrlInfo.fps;
    ctx->mUvcCfg.uvc_cfg.fcc = uvcCtrlInfo.fcc;

    clock_gettime(CLOCK_MONOTONIC, &now_tm);
    last_time_us = now_tm.tv_sec * 1000000LL + now_tm.tv_nsec / 1000; // us
    check_ret = check_uvc_mpi_cfg_file_init(&ctx->mUvcCfg);
    if (check_ret) {
      // when not config need set this change here
      ctx->mUvcCfg.venc_cfg.common_cfg.change |=
          MPI_VENC_COMMON_CFG_CHANGE_CREATE;
    }
    clock_gettime(CLOCK_MONOTONIC, &now_tm);
    now_time_us = now_tm.tv_sec * 1000000LL + now_tm.tv_nsec / 1000; // us
    use_time_us = now_time_us - last_time_us;
    LOG_INFO("check_uvc_mpi_cfg_file_init time:%d us\n", use_time_us);

    // choice the vi uvc fmt
    switch (ctx->mUvcCtx.fcc) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_BGR24:
      if (ctx->mUvcCfg.common_cfg.uvc_enable_vpss)
        ctx->mFmtType = UVC_FMT_TYPE_VI_VPSS_UVC;
      else
        ctx->mFmtType = UVC_FMT_TYPE_VI_UVC;
      ctx->mUvcBuffCount =
          ctx->mUvcCfg.vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].buf_cnt +
          ctx->mUvcCfg.vpss_cfg[MPI_VPSS_CHANNEL_TYPE_UVC].buf_cnt;
      if(ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame) {
        ctx->mUvcBuffCount += ctx->mUvcCfg.vi_cfg[MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME].buf_cnt;
      }
      break;
    case V4L2_PIX_FMT_MJPEG:
      if (ctx->mUvcCfg.common_cfg.uvc_enable_vpss) {
          ctx->mFmtType = UVC_FMT_TYPE_VI_VPSS_VENC_UVC;
      } else {
          ctx->mFmtType = UVC_FMT_TYPE_VI_VENC_UVC;
      }
      ctx->mUvcBuffCount = ctx->mUvcCfg.venc_cfg.mjpeg_cfg.buf_cnt;
      ctx->vencTempInfo.force_idr_cnt = 0;
      break;
    case V4L2_PIX_FMT_H264:
      if (ctx->mUvcCfg.common_cfg.uvc_enable_vpss) {
        ctx->mFmtType = UVC_FMT_TYPE_VI_VPSS_VENC_UVC;
      } else {
        ctx->mFmtType = UVC_FMT_TYPE_VI_VENC_UVC;
      }
      ctx->mUvcBuffCount = ctx->mUvcCfg.venc_cfg.h264_cfg.buf_cnt;
      ctx->vencTempInfo.force_idr_cnt =
          ctx->mUvcCfg.venc_cfg.common_cfg.idr_gop *
          ctx->mUvcCfg.venc_cfg.common_cfg.idr_cnt;

      break;
    case V4L2_PIX_FMT_HEVC:
      if (ctx->mUvcCfg.common_cfg.uvc_enable_vpss) {
        ctx->mFmtType = UVC_FMT_TYPE_VI_VPSS_VENC_UVC;
      } else {
        ctx->mFmtType = UVC_FMT_TYPE_VI_VENC_UVC;
      }
      ctx->mUvcBuffCount = ctx->mUvcCfg.venc_cfg.h265_cfg.buf_cnt;
      ctx->vencTempInfo.force_idr_cnt =
          ctx->mUvcCfg.venc_cfg.common_cfg.idr_gop *
          ctx->mUvcCfg.venc_cfg.common_cfg.idr_cnt;
      break;
    default:
      LOG_ERROR("process not support this fcc 0x%x\n", ctx->mUvcCtx.fcc);
      break;
    }

    if (ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame)
      camera_control_start(ctx->mUvcCfg.vi_cfg[MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME].dev_id, 0, 0, 0, 0, 0);

    set_uvc_control_start(uvcCtrlInfo.video_id, uvcCtrlInfo.width,
                          uvcCtrlInfo.height, uvcCtrlInfo.fps, uvcCtrlInfo.fcc,
                          uvcCtrlInfo.eptz[UVC_EPTZ_AUTO]);

#ifdef RV1126B
    int32_t s32Ret = RK_FAILURE;
    if (rk_isp_need_raw_pipe_ctrl(uvcCtrlInfo.index)) {
      s32Ret = RK_MPI_VI_CtrlRawStream(uvcCtrlInfo.index, RK_FALSE);
      LOG_INFO("raw stream control by rkaiq\n");
    } else {
      s32Ret = RK_MPI_VI_CtrlRawStream(uvcCtrlInfo.index, RK_TRUE);
      LOG_INFO("raw stream control by rockit\n");
    }
    if (s32Ret != RK_SUCCESS) {
      LOG_ERROR("RK_MPI_VI_CtrlRawStream %x\n", s32Ret);
    }
#endif

    switch (ctx->mFmtType) {
    case UVC_FMT_TYPE_VI_UVC:
      configVi();
      break;
    case UVC_FMT_TYPE_VI_VPSS_UVC:
      configVi();
      configVpss();
      break;
    case UVC_FMT_TYPE_VI_VENC_UVC:
      configVi();
      configVenc();
      break;
    case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
      configVi();
      configVpss();
      configVenc();
      break;
    default:
      LOG_ERROR("not support this type:%d\n", ctx->mFmtType);
      break;
    }
  }
#ifdef UVC_ENABLE_NN
  if (ctx->mUvcCfg.common_cfg.nn_enable)
    ctx->nnCtx->configProcess(&ctx->mUvcCfg);
#endif

#ifdef UVC_ENABLE_EPTZ
  if (ctx->mUvcCfg.eptz_cfg.enable) {
    ctx->eptzCtx.mUvcCfg = &ctx->mUvcCfg;
    eptz_init(&ctx->eptzCtx);
  }
#endif

  ctx->dev = reinterpret_cast<struct uvc_device *>(uvcCtrlInfo.dev);

  if (ctx->mUvcCfg.common_cfg.userdata_enable)
    ctx->mUserdataCallback = &gUvcUserdataCallback[uvcCtrlInfo.index];

  return ctx->mUvcBuffCount;
}

int UVCProcess::configCropScale(RECT_S src, RECT_S dst) {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  VPSS_CROP_INFO_S stCropInfo;
  memset(&stCropInfo, 0, sizeof(VPSS_CROP_INFO_S));

  if (memcmp(&src, &dst, sizeof(RECT_S))) {
    stCropInfo.bEnable = RK_TRUE;
    stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
    stCropInfo.stCropRect = dst;
  } else { // workaround
    stCropInfo.bEnable = RK_TRUE;
    stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
    stCropInfo.stCropRect = dst;
  }
  uvc_vpss_config_crop(ctx->mUvcCfg, stCropInfo, MPI_VPSS_CHANNEL_TYPE_UVC);

  return 0;
}

int UVCProcess::configEptz(UVC_CTRL_INFO uvcCtrlInfo) {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  RECT_S srcRect, dstRect;

  if (memcmp(&ctx->mUvcCtx, &uvcCtrlInfo, sizeof(UVC_CTRL_INFO))) {
    LOG_INFO("index:%d config process paramter change type:%d.\n",
             uvcCtrlInfo.index, uvcCtrlInfo.type);
#ifdef UVC_ENABLE_EPTZ
    eptz_config(&ctx->eptzCtx, uvcCtrlInfo);
#else
    LOG_INFO("not support eptz config\n");
#endif
    memcpy(&ctx->mUvcCtx, &uvcCtrlInfo, sizeof(UVC_CTRL_INFO));
  }
  return 0;
}

int UVCProcess::resetCtx() {
  RK_S32 s32Ret;
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);

  if (ctx->mUvcBuf)
    ctx->mUvcBuf = nullptr;
  if (ctx->mUvcDebug.pDebugFile)
    fclose(ctx->mUvcDebug.pDebugFile);
  memset(&ctx->mUvcDebug, 0, sizeof(UVCDegbuFileInfo));
  memset(&ctx->mUvcCtx, 0, sizeof(UVC_CTRL_INFO));
  ctx->getBufCnt = 0;
  ctx->releaseBufCnt = 0;

  return 0;
}

int UVCProcess::startProcess() {
  RK_S32 s32Ret;
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);

  uvc_buffer_init(ctx->dev);

  switch (ctx->mFmtType) {
  case UVC_FMT_TYPE_VI_UVC:
    startVi();
    break;
  case UVC_FMT_TYPE_VI_VPSS_UVC:
    startVi();
    startVpss();
    bindViVpss();
    break;
  case UVC_FMT_TYPE_VI_VENC_UVC:
    startVi();
    startVenc();
    bindViVenc();
    break;
  case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
    startVi();
    startVpss();
    startVenc();
    bindViVpss();
    bindVpssVenc();
    break;
  default:
    LOG_ERROR("not support this type:%d\n", ctx->mFmtType);
    break;
  }

#ifdef UVC_ENABLE_NN
  if (ctx->mUvcCfg.common_cfg.nn_enable)
    ctx->nnCtx->startProcess();
#endif

  ctx->mStart = true;
  // if use one thread do uvc. do not call this thread
  ctx->mThread = new std::thread(threadLoop, reinterpret_cast<void *>(this));

  if (ctx->mUvcCfg.osd_cfg.enable)
    osd_start(&ctx->mUvcCfg);

  return 0;
}

int UVCProcess::stopProcess() {
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);

  ctx->mStart = false;
  if (ctx->mThread) {
    ctx->mThread->join();
    delete ctx->mThread;
    ctx->mThread = nullptr;
  }

#ifdef UVC_ENABLE_EPTZ
  if (ctx->mUvcCfg.eptz_cfg.enable)
    eptz_deinit(&ctx->eptzCtx);
#endif

#ifdef UVC_ENABLE_NN
  if (ctx->mUvcCfg.common_cfg.nn_enable)
    ctx->nnCtx->stopProcess();
#endif

  if (ctx->mUvcCfg.osd_cfg.enable)
    osd_stop(&ctx->mUvcCfg);

  uvc_buffer_deinit(ctx->mUvcCtx.video_id);
  switch (ctx->mFmtType) {
  case UVC_FMT_TYPE_VI_UVC:
    stopVi();
    break;
  case UVC_FMT_TYPE_VI_VENC_UVC:
    unBindViVenc();
    stopVenc();
    stopVi();
    break;
  case UVC_FMT_TYPE_VI_VPSS_UVC:
    unBindViVpss();
    stopVpss();
    stopVi();
    break;
  case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
    unBindViVpss();
    unBindVpssVenc();
    stopVenc();
    stopVpss();
    stopVi();
    break;
  default:
    LOG_ERROR("not support this type:%d\n", ctx->mFmtType);
    break;
  }

  set_uvc_control_stop(ctx->mUvcCtx.video_id);
  if (ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame)
    camera_control_stop(ctx->mUvcCfg.vi_cfg[MPI_VI_CHANNEL_TYPE_IR_BRIGHT_DARK_FRAME].dev_id);

  resetCtx();

  return 0;
}

int UVCProcess::releaseFrame(struct uvc_buffer *uvc_buf) {
  if (!uvc_buf) {
    LOG_ERROR("uvc_buf is null\n");
    return -1;
  }

  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  MB_BLK blk = (MB_BLK)(uvc_buf->frame);

  ctx->releaseBufCnt++;
  LOG_DEBUG("releaseFrame:%p, cnt:%d\n", blk, ctx->releaseBufCnt);

  if (ctx->mUvcCfg.common_cfg.userdata_enable &&
      ctx->mUserdataCallback &&
      ctx->mUserdataCallback->release_user_frame) {
    UVC_USER_FRAME_T user_frame = {0};
    user_frame.width = uvc_buf->width;
    user_frame.height = uvc_buf->height;
    user_frame.mblk = uvc_buf->frame;
    user_frame.m.userptr = uvc_buf->buffer;
    user_frame.fd = uvc_buf->fd;
    user_frame.size = uvc_buf->size;
    user_frame.seq = uvc_buf->seq;
    ctx->mUserdataCallback->release_user_frame(ctx->mUvcCtx.index, &user_frame);
    return 0;
  }

  if (!blk) {
    LOG_ERROR("blk is null\n");
    return -1;
  }

  switch (ctx->mFmtType) {
  case UVC_FMT_TYPE_VI_UVC:
    VIDEO_FRAME_INFO_S stViFrameInfo;
    stViFrameInfo.stVFrame.pMbBlk = blk;
    RK_MPI_VI_ReleaseChnFrame(ctx->viChnCtx.pipeId, ctx->viChnCtx.channelId,
                              &stViFrameInfo);
    break;
  case UVC_FMT_TYPE_VI_VPSS_UVC:
    VIDEO_FRAME_INFO_S stVpssFrameInfo;
    stVpssFrameInfo.stVFrame.pMbBlk = blk;
    RK_MPI_VPSS_ReleaseChnFrame(ctx->vpssChnCtx.group,
                                ctx->vpssChnCtx.channelId, &stVpssFrameInfo);
    break;
  case UVC_FMT_TYPE_VI_VENC_UVC:
  case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
    ctx->vencTempInfo.stReleaseVencFrame.pstPack->pMbBlk = blk;
    RK_MPI_VENC_ReleaseStream(ctx->vencChnCtx.channelId,
                              &ctx->vencTempInfo.stReleaseVencFrame);
    break;
  default:
    LOG_ERROR("not support this type:%d\n", ctx->mFmtType);
    break;
  }

  return 0;
}

int UVCProcess::doProcess() {
  RK_S32 s32Ret;
  UVCProcessCtx *ctx = getUVCProcessCtx(mCtx);
  int ret;

  ret = do_process(ctx);

  return ret;
}

UVC_RET_TYPE queue_uvc_buf(UVCProcessCtx *ctx) {
  if (!ctx->mUvcBuf) {
    ctx->mUvcBuf = uvc_buffer_write_get(ctx->mUvcCtx.video_id);
    LOG_DEBUG("uvc_buffer_write_get (buf: %p)\n", ctx->mUvcBuf);
  }

  return UVC_RET_OK;
}

static int32_t getPicMultiple(PIXEL_FORMAT_E pixelFormat, RK_U32 *mutipleNum, RK_U32 *mutipleDen) {
    RK_U32 tmpMutipleNum = 1;
    RK_U32 tmpMutipleDen = 1;

    switch (pixelFormat) {
      case RK_FMT_YUV400SP: {
        tmpMutipleNum = 1;
        tmpMutipleDen = 1;
      } break;
      case RK_FMT_YUV420SP:
      case RK_FMT_YUV420P:
      case RK_FMT_YUV420P_VU:
      case RK_FMT_YUV420SP_VU:
      case RK_FMT_YUV411SP:
      case RK_FMT_RGB444:
      case RK_FMT_BGR444: {
        tmpMutipleNum = 3;
        tmpMutipleDen = 2;
      } break;
      case RK_FMT_YUV422SP:
      case RK_FMT_YUV422P:
      case RK_FMT_YUV422SP_VU:
      case RK_FMT_YUV422_YUYV:
      case RK_FMT_YUV422_UYVY:
      case RK_FMT_YUV440SP:
      case RK_FMT_YUV422_YVYU:
      case RK_FMT_YUV422_VYUY:
      case RK_FMT_RGB565:
      case RK_FMT_BGR565:
      case RK_FMT_RGB555:
      case RK_FMT_BGR555:
      case RK_FMT_ARGB1555:
      case RK_FMT_ABGR1555:
      case RK_FMT_ARGB4444:
      case RK_FMT_ABGR4444:
      case RK_FMT_BGRA4444:
      case RK_FMT_RGBA4444:
      case RK_FMT_RGBA5551:
      case RK_FMT_BGRA5551: {
        tmpMutipleNum = 2;
        tmpMutipleDen = 1;
      } break;
      case RK_FMT_RGB888:
      case RK_FMT_BGR888:
      case RK_FMT_ARGB8565:
      case RK_FMT_ABGR8565:
      case RK_FMT_YUV444SP:
      case RK_FMT_YUV444:
      case RK_FMT_YUV444P: {
        tmpMutipleNum = 3;
        tmpMutipleDen = 1;
      } break;
      case RK_FMT_ARGB8888:
      case RK_FMT_ABGR8888:
      case RK_FMT_BGRA8888:
      case RK_FMT_RGBA8888: {
        tmpMutipleNum = 4;
        tmpMutipleDen = 1;
      } break;
      case RK_FMT_RGB101010:
      case RK_FMT_BGR101010: {
        tmpMutipleNum = 15;
        tmpMutipleDen = 4;
      } break;
      case RK_FMT_YUV420SP_10BIT: {
        tmpMutipleNum = 15;
        tmpMutipleDen = 8;
      } break;
      case RK_FMT_YUV422SP_10BIT: {
        tmpMutipleNum = 5;
        tmpMutipleDen = 2;
      } break;

      default:
        RK_LOGE("unsupport pixel format 0x%x", pixelFormat);
        return RK_ERR_NOT_SUPPORT;
    }

    *mutipleNum = tmpMutipleNum;
    *mutipleDen = tmpMutipleDen;

    return RK_SUCCESS;
}

static RK_U32 getAfbcBufferSize(const RK_S32 width, const RK_S32 height, const PIXEL_FORMAT_E format) {
    RK_S32 bpp[3];
    RK_S32 planes = 1;
    RK_S32 bodySize = 0;
    RK_S32 headSize = 0;
    RK_S32 totalSize = 0;
    RK_S32 i = 0;
    RK_S32 sbBytes = 0;
    RK_S32 sbNums = 0;
    RK_S32 alignW[3];
    RK_S32 alignH[3];

    alignW[0] = RK_ALIGN(width, 16);
    alignH[0] = RK_ALIGN(height, 16);

    switch (format) {
      case RK_FMT_ARGB8888:
      case RK_FMT_ABGR8888:
      case RK_FMT_BGRA8888:
      case RK_FMT_RGBA8888:
        bpp[0] = 32;
        break;
      case RK_FMT_RGB888:
      case RK_FMT_BGR888:
        bpp[0] = 24;
        break;
      case RK_FMT_RGB565:
      case RK_FMT_BGR565:
        bpp[0] = 16;
        break;
      case RK_FMT_ABGR1555:
      case RK_FMT_BGRA5551:
      case RK_FMT_RGBA5551:
      case RK_FMT_ARGB1555:
        bpp[0] = 16;
        break;
      case RK_FMT_YUV422_YUYV:
      case RK_FMT_YUV422_YVYU:
        planes = 1;
        bpp[0] = 16;
        break;
      case RK_FMT_YUV420SP:
      case RK_FMT_YUV420SP_VU:
        planes = 2;
        bpp[0] = 8;
        bpp[1] = 16;
        alignW[1] = RK_ALIGN(width / 2, 16);
        alignH[1] = RK_ALIGN(height / 2, 16);
        break;
      case RK_FMT_YUV420SP_10BIT:
        planes = 2;
        bpp[0] = 10;
        bpp[1] = 20;
        alignW[1] = RK_ALIGN(width / 2, 16);
        alignH[1] = RK_ALIGN(height / 2, 16);
        break;
      default:
        bpp[0] = 32;
        RK_LOGW("no support format=%x,[%d,%d]", format, width, height);
        break;
    }

    for (i = 0; i < planes; i++) {
        sbBytes = RK_ALIGN(256 * bpp[i] / 8, 128);
        sbNums = alignW[i] * alignH[i] / 256;
        bodySize = sbBytes * sbNums;
        if (planes > 1 && i < 2)
            bodySize = RK_ALIGN(bodySize, 4096);
        headSize = RK_ALIGN(alignW[i] * alignH[i] / 16, 4096);
        totalSize += (bodySize + headSize);
    }

    return totalSize;
}

static int32_t getCommonBufferSize(const PIC_BUF_ATTR_S *config, 
      MB_PIC_CAL_S *result, RK_S32 alignBit) {
    int32_t ret = RK_SUCCESS;
    RK_U32 picMutipleNum = 1;
    RK_U32 picMutipleDen = 1;
    RK_U32 width = 0;
    RK_U32 height = 0;
    RK_U32 virWidth = 0;
    RK_U32 virHeight = 0;
    RK_U32 graphicSize = 0;
    RK_S32 compressMode = 0;
    PIXEL_FORMAT_E pixelFormat;

    if (config == RK_NULL) {
        return RK_ERR_NULL_PTR;
    }

    width = config->u32Width;
    height = config->u32Height;
    compressMode = config->enCompMode;
    pixelFormat = config->enPixelFormat;

    if (width == 0 || height == 0 || compressMode >= COMPRESS_MODE_BUTT) {
        return RK_ERR_ILLEGAL_PARAM;
    }

    memset(result, 0, sizeof(MB_PIC_CAL_S));

    virWidth  = RK_ALIGN(width, alignBit);
    virHeight = RK_ALIGN(height, alignBit);

    if (compressMode == COMPRESS_AFBC_16x16) {
        graphicSize = getAfbcBufferSize(virWidth, virHeight, pixelFormat);
        result->u32MBSize = graphicSize;
        result->u32VirWidth = virWidth;
        result->u32VirHeight = virHeight;
        return ret;
    }

    ret = getPicMultiple(pixelFormat, &picMutipleNum, &picMutipleDen);
    if (ret != RK_SUCCESS) {
        return ret;
    }

    // graphic picture size
    graphicSize = virWidth * virHeight * picMutipleNum / picMutipleDen;

    result->u32MBSize = graphicSize;
    result->u32VirWidth = virWidth;
    result->u32VirHeight = virHeight;

    return ret;
}

static RK_U32 CalcFrameSize(VIDEO_FRAME_INFO_S *frame) {
    if (frame == NULL) {
        return 0;
    }
    RK_S32 err = RK_SUCCESS;
    PIC_BUF_ATTR_S picCalAttr = {0};
    MB_PIC_CAL_S picResult = {0};

    picCalAttr.u32Width = frame->stVFrame.u32Width;
    picCalAttr.u32Height = frame->stVFrame.u32Height;
    picCalAttr.enPixelFormat = frame->stVFrame.enPixelFormat;
    picCalAttr.enCompMode = frame->stVFrame.enCompressMode;
    err = getCommonBufferSize(&picCalAttr, &picResult, 2);
    if (err != RK_SUCCESS) {
        return 0;
    }

    return picResult.u32MBSize;
}

UVC_RET_TYPE dequeue_uvc_buf(UVCProcessCtx *ctx, void *frame) {
  VIDEO_FRAME_INFO_S *videoFrame;
  VENC_STREAM_S *vencFrame;
  struct uvc_buffer uvc_buf = {0};

  if (ctx->mUvcBuf) {
    memcpy(&uvc_buf, ctx->mUvcBuf, sizeof(struct uvc_buffer));

    switch (ctx->mFmtType) {
    case UVC_FMT_TYPE_VI_UVC:
    case UVC_FMT_TYPE_VI_VPSS_UVC:
      videoFrame = reinterpret_cast<VIDEO_FRAME_INFO_S *>(frame);
      uvc_buf.frame = reinterpret_cast<void *>(videoFrame->stVFrame.pMbBlk);
      uvc_buf.buffer = RK_MPI_MB_Handle2VirAddr(
          videoFrame->stVFrame.pMbBlk); // for debug file
      uvc_buf.fd = RK_MPI_MB_Handle2Fd(videoFrame->stVFrame.pMbBlk);
      uvc_buf.seq = videoFrame->stVFrame.u32TimeRef;
      uvc_buf.pts = videoFrame->stVFrame.u64PTS;
      uvc_buf.size = CalcFrameSize(videoFrame);
      uvc_buf.offset = 0;
      RK_MPI_SYS_MmzFlushCache(videoFrame->stVFrame.pMbBlk, RK_TRUE);
      break;
    case UVC_FMT_TYPE_VI_VENC_UVC:
    case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
      vencFrame = reinterpret_cast<VENC_STREAM_S *>(frame);
      uvc_buf.frame = reinterpret_cast<void *>(vencFrame->pstPack->pMbBlk);
      uvc_buf.buffer = RK_MPI_MB_Handle2VirAddr(
          vencFrame->pstPack->pMbBlk); // for debug file
      uvc_buf.fd = RK_MPI_MB_Handle2Fd(vencFrame->pstPack->pMbBlk);
      uvc_buf.seq = vencFrame->u32Seq;
      uvc_buf.pts = vencFrame->pstPack->u64PTS;
      uvc_buf.size = vencFrame->pstPack->u32Len;
      uvc_buf.offset = vencFrame->pstPack->u32Offset;
      if (vencFrame->pstPack->u32Offset) {
        LOG_DEBUG("u32Offset:%d\n", vencFrame->pstPack->u32Offset);
      }
      break;
    default:
      LOG_ERROR("not support this fmt:%d\n", ctx->mFmtType);
      break;
    }

    memcpy(ctx->mUvcBuf, &uvc_buf, sizeof(struct uvc_buffer));
    uvc_buffer_read_set(ctx->mUvcCtx.video_id, ctx->mUvcBuf);
    LOG_DEBUG("debug info!idx:%d frame mb:%p uvc_buf->fd:%d size=%d "
              "uvc_buf->seq:%d \n",
              uvc_buf, uvc_buf.frame, uvc_buf.fd, uvc_buf.size, uvc_buf.seq);
    ctx->mUvcBuf = NULL;
    ctx->getBufCnt++;
    LOG_DEBUG("get buff cnt:%d\n", ctx->getBufCnt);
  }

  return UVC_RET_OK;
}

static UVC_RET_TYPE uvc_userdata_data_process(UVCProcessCtx *ctx) {
  RK_S32 s32Ret;
  UVC_USER_FRAME_T stUserFrame = {0};
  struct uvc_buffer uvc_buf = {0};

  if (!ctx->mUserdataCallback || !ctx->mUserdataCallback->get_user_frame) {
    LOG_DEBUG("uvc userdata get_user_frame null\n");
    return UVC_RET_ERR;
  }

  queue_uvc_buf(ctx);
  if (!ctx->mUvcBuf) {
    LOG_DEBUG("uvc_buffer_write_get null buff\n");
    usleep(2 * 1000);
    return UVC_RET_WAT;
  }

  s32Ret = ctx->mUserdataCallback->get_user_frame(ctx->mUvcCtx.index, &stUserFrame);
  if (s32Ret != RK_SUCCESS) {
    LOG_DEBUG("get user frame err:0x%x\n", s32Ret);
    return UVC_RET_ERR;
  }
  memcpy(&uvc_buf, ctx->mUvcBuf, sizeof(struct uvc_buffer));
  uvc_buf.frame = stUserFrame.mblk;
  uvc_buf.buffer = stUserFrame.m.userptr;
  uvc_buf.fd = stUserFrame.fd;
  uvc_buf.seq = stUserFrame.seq;
  uvc_buf.size = stUserFrame.size;
  memcpy(ctx->mUvcBuf, &uvc_buf, sizeof(struct uvc_buffer));

  uvc_buffer_read_set(ctx->mUvcCtx.video_id, ctx->mUvcBuf);
  LOG_DEBUG("debug info!idx:%d frame mb:%p uvc_buf->fd:%d size=%d "
            "uvc_buf->seq:%d \n",
            uvc_buf, uvc_buf.frame, uvc_buf.fd, uvc_buf.size, uvc_buf.seq);
  ctx->mUvcBuf = NULL;
  ctx->getBufCnt++;
  LOG_DEBUG("get buff cnt:%d\n", ctx->getBufCnt);

  return UVC_RET_OK;
}

static UVC_RET_TYPE uvc_out_yuv_process(UVCProcessCtx *ctx) {
  VIDEO_FRAME_INFO_S videoFrame;
  RK_S32 s32Ret;
  UVC_RET_TYPE ret = UVC_RET_WAT;

  queue_uvc_buf(ctx);
  if (!ctx->mUvcBuf) {
    LOG_DEBUG("uvc_buffer_write_get null buff\n");
    usleep(2 * 1000);
  }
  else
  {
    if (ctx->mFmtType == UVC_FMT_TYPE_VI_UVC)
    {
      if (!ctx->mUvcCfg.common_cfg.uvc_enable_ir_bright_dark_frame)
      {
        s32Ret = RK_MPI_VI_GetChnFrame(ctx->viChnCtx.pipeId,
                                       ctx->viChnCtx.channelId, &videoFrame, 33);
      }
      else
      {
        static int frame_count = 0;
        frame_count++;
        if (frame_count % 2 == 0)
        {
          s32Ret = RK_MPI_VI_GetChnFrame(ctx->viChnCtx.pipeId,
                                         ctx->viChnCtx.channelId, &videoFrame, 130);
        }
        else
        {
          s32Ret = RK_MPI_VI_GetChnFrame(ctx->viChnCtxIrDarkFrame.pipeId,
                                         ctx->viChnCtxIrDarkFrame.channelId, &videoFrame, 130);
        }
      }
    }
    else if (ctx->mFmtType == UVC_FMT_TYPE_VI_VPSS_UVC)
      s32Ret = RK_MPI_VPSS_GetChnFrame(
          ctx->vpssChnCtx.group, ctx->vpssChnCtx.channelId, &videoFrame, 33);
    else
      ret = UVC_RET_ERR;

    if (s32Ret == RK_SUCCESS) {
      ret = dequeue_uvc_buf(ctx, (void *)&videoFrame);
    } else {
      LOG_DEBUG("get yuv timeout:0x%x\n", s32Ret);
    }
  }

  return ret;
}

static UVC_RET_TYPE uvc_out_venc_process(UVCProcessCtx *ctx) {
  RK_S32 s32Ret;
  UVC_RET_TYPE ret = UVC_RET_WAT;

  queue_uvc_buf(ctx);
  // LOG_INFO("test lqh\n");
  if (!ctx->mUvcBuf) {
    LOG_DEBUG("uvc_buffer_write_get null buff\n");
    usleep(2 * 1000);
  } else {
    s32Ret = RK_MPI_VENC_GetStream(ctx->vencChnCtx.channelId,
                                   &ctx->vencTempInfo.stVencFrame, 33);
    if (s32Ret == RK_SUCCESS) {
      ret = dequeue_uvc_buf(ctx, (void *)&ctx->vencTempInfo.stVencFrame);
    } else if (s32Ret == RK_ERR_VENC_NOT_CONFIG) {
      LOG_ERROR("venc chn:%d not config\n", ctx->vencChnCtx.channelId);
      ret = UVC_RET_ERR;
    } else {
      LOG_DEBUG("get venc timeout:0x%x\n", s32Ret);
    }
  }

  return ret;
}

// uvc_video can call this func direct to reduce thread use.
static UVC_RET_TYPE do_process(UVCProcessCtx *ctx) {
  UVC_RET_TYPE ret = UVC_RET_OK;

  if (ctx->mUvcCfg.common_cfg.userdata_enable) {
    uvc_userdata_data_process(ctx);
  } else {
    switch (ctx->mFmtType) {
      case UVC_FMT_TYPE_VI_UVC:
      case UVC_FMT_TYPE_VI_VPSS_UVC:
        ret = uvc_out_yuv_process(ctx);
        break;
      case UVC_FMT_TYPE_VI_VENC_UVC:
      case UVC_FMT_TYPE_VI_VPSS_VENC_UVC:
        ret = uvc_out_venc_process(ctx);
        break;
      default:
        LOG_ERROR("not support this type:%d\n", ctx->mFmtType);
        ret = UVC_RET_ERR;
        break;
    }
  }

#ifdef UVC_ENABLE_EPTZ
  if (ctx->mUvcCfg.eptz_cfg.enable) {
    FaceInfo faceInfo;
    if (ctx->nnCtx != nullptr) {
      faceInfo = ctx->nnCtx->getFaceInfo();
    }
    eptz_process(&ctx->eptzCtx, &faceInfo);
  }
#endif

#if 0 // reduce thread do this
  if (ctx->mUvcCfg.common_cfg.nn_enable) {
    int32_t use_time_us, now_time_us, last_time_us;
    struct timespec now_tm = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &now_tm);
    last_time_us = now_tm.tv_sec * 1000000LL + now_tm.tv_nsec / 1000; // us
    ctx->nnCtx->doProcess();
    clock_gettime(CLOCK_MONOTONIC, &now_tm);
    now_time_us = now_tm.tv_sec * 1000000LL + now_tm.tv_nsec / 1000; // us
    use_time_us = now_time_us - last_time_us;
    LOG_INFO("nn process use time:%d us\n", use_time_us);
  }
#endif

  return ret;
}

void *UVCProcess::threadLoop(void *arg) {
  prctl(PR_SET_NAME, "uvcProcessThread");
  UVCProcess *uvcProcess = reinterpret_cast<UVCProcess *>(arg);
  UVCProcessCtx *ctx = getUVCProcessCtx(uvcProcess->getCtx());
  UVC_RET_TYPE ret = UVC_RET_OK;

  // wait uvc streamon
  while (ctx->mStart && (!uvc_get_user_run_state(ctx->mUvcCtx.video_id) ||
                         !uvc_buffer_write_enable(ctx->mUvcCtx.video_id))) {
    usleep(10 * 1000);
  }

  // when streamon start
  while (ctx->mStart) {
    ret = do_process(ctx);
    if (ret == UVC_RET_ERR)
      break;
  }
  LOG_INFO("thread type:%d exit ret:%d\n", ctx->mFmtType, ret);

  return NULL;
}
