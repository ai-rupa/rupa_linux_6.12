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
#include "eptz.h"
#include "../face/face_detector.h"
#include "uvc_log.h"
#include "uvc_mpi_config.h"
#include "uvc_mpi_vpss.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#define UVC_EPTZ_PAN_MAX 20.0
#define UVC_EPTZ_PAN_COUNT 40.0
#define UVC_EPTZ_TILT_MAX 20.0
#define UVC_EPTZ_TILT_COUNT 40.0

#define REQUEST16B9 1

// now not need align 2 for rv1106/rv1103
int eptz_align(int align, int val) { return (val - val % align); }

int eptz_init(EPTZ_CTX_S *ctx) {
  if (!ctx->mAiData.face_data)
    ctx->mAiData.face_data =
        (FaceData *)malloc(MAX_OBJECT_NUM * sizeof(FaceData));
  ctx->mInitInfo.eptz_src_width =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_width;
  ctx->mInitInfo.eptz_src_height =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_height;
  ctx->mInitInfo.eptz_dst_width =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_width;
  ctx->mInitInfo.eptz_dst_height =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_height;
  ctx->mInitInfo.camera_dst_width = ctx->mUvcCfg->uvc_cfg.width;
  ctx->mInitInfo.camera_dst_height = ctx->mUvcCfg->uvc_cfg.height;
  ctx->mInitInfo.eptz_npu_width =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_width;
  ctx->mInitInfo.eptz_npu_height =
      ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_height;
  ctx->mInitInfo.eptz_facedetect_score_shold = 0.40;
  ctx->mInitInfo.eptz_zoom_speed = ctx->mUvcCfg->eptz_cfg.zoom_speed;
  ctx->mInitInfo.eptz_fast_move_frame_judge =
      ctx->mUvcCfg->eptz_cfg.fast_move_frame_judge;
  ctx->mInitInfo.eptz_zoom_frame_judge =
      ctx->mUvcCfg->eptz_cfg.zoom_frame_judge;
  ctx->mInitInfo.eptz_threshold_x = 120;
  ctx->mInitInfo.eptz_threshold_y = 100;
  ctx->mInitInfo.eptz_animation = EPTZ_NONLINEAR;
  ctx->mInitInfo.eptz_iterate_x = ctx->mUvcCfg->eptz_cfg.iterate_x;
  ctx->mInitInfo.eptz_iterate_y = ctx->mUvcCfg->eptz_cfg.iterate_y;
  ctx->mInitInfo.eptz_ratio = ctx->mUvcCfg->eptz_cfg.ratio;

  eptzConfigInit(&ctx->mInitInfo);

  ctx->mPosition[0] = 0;
  ctx->mPosition[1] = 0;
  ctx->mPosition[2] = ctx->mInitInfo.eptz_dst_width;
  ctx->mPosition[3] = ctx->mInitInfo.eptz_dst_height;
  if (ctx->mUvcCfg->eptz_cfg.enable_boot) {
    ctx->mSetInfo[UVC_EPTZ_AUTO] = 1;
  } else {
    ctx->mSetInfo[UVC_EPTZ_AUTO] = 0;
  }
  ctx->mCurrentInfo[UVC_EPTZ_ZOOM] = 10;
  ctx->mCurrentInfo[UVC_EPTZ_PAN] = 0;
  ctx->mCurrentInfo[UVC_EPTZ_TILT] = 0;
  ctx->mSetInfo[UVC_EPTZ_ZOOM] = 10;
  ctx->mSetInfo[UVC_EPTZ_PAN] = 0;
  ctx->mSetInfo[UVC_EPTZ_TILT] = 0;

  ctx->mNnResultRatio[HORIZONTAL] = ctx->mInitInfo.eptz_npu_width / 10000.0;
  ctx->mNnResultRatio[VERTICAL] = ctx->mInitInfo.eptz_npu_height / 10000.0;

  ctx->mInit = true;
  LOG_INFO("eptz_info src_wh [%d %d] dst_wh[%d %d], threshold_xy[%d %d] "
           "zoom_speed [%d] fast_move_frame_judge [%d] "
           "zoom_frame_judge [%d] iterate_xy[%d %d] ratio[%.2f] \n",
           ctx->mInitInfo.eptz_src_width, ctx->mInitInfo.eptz_src_height,
           ctx->mInitInfo.camera_dst_width, ctx->mInitInfo.camera_dst_height,
           ctx->mInitInfo.eptz_threshold_x, ctx->mInitInfo.eptz_threshold_y,
           ctx->mInitInfo.eptz_zoom_speed,
           ctx->mInitInfo.eptz_fast_move_frame_judge,
           ctx->mInitInfo.eptz_zoom_frame_judge, ctx->mInitInfo.eptz_iterate_x,
           ctx->mInitInfo.eptz_iterate_y, ctx->mInitInfo.eptz_ratio);

  return 0;
}

int eptz_deinit(EPTZ_CTX_S *ctx) {
  if (!ctx->mAiData.face_data) {
    free(ctx->mAiData.face_data);
    ctx->mAiData.face_data = NULL;
  }
  ctx->mInit = false;

  return 0;
}

void eptz_config(EPTZ_CTX_S *ctx, UVC_CTRL_INFO uvcCtrlInfo) {
  ctx->mSetInfo[uvcCtrlInfo.type] = uvcCtrlInfo.eptz[uvcCtrlInfo.type];
  // memcpy(&ctx->mSetInfo, eptzInfo, sizeof(int) * UVC_EPTZ_MAX);
}

void eptz_process(EPTZ_CTX_S *ctx, FaceInfo *faceinfo) {
  bool mIsZoomSet;

  int mSrcWidth = ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_width;
  int mSrcHeight = ctx->mUvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_UVC].assign_height;
  int mDstWidth = mSrcWidth;
  int mDstHeight = mSrcHeight;

  int mEptzOffsetX = 0;
  int mEptzOffsetY = 0;
  int mEptzWidth = mSrcWidth;
  int mEptzHeight = mSrcHeight;
  if (ctx->mUvcCfg->eptz_cfg.debug) {
    if (access("/tmp/z_10", 0) == 0) {
      LOG_INFO("eptz_process z_10\n");
      ctx->mSetInfo[UVC_EPTZ_ZOOM] = 15;
    }
    if (access("/tmp/p_10", 0) == 0) {
      LOG_INFO("eptz_process p_10\n");
      ctx->mSetInfo[UVC_EPTZ_PAN] = 20;
    }
    if (access("/tmp/t_10", 0) == 0) {
      LOG_INFO("eptz_process t_10\n");
      ctx->mSetInfo[UVC_EPTZ_TILT] = 20;
    }
  }

  if (access("/tmp/eptz", 0) == 0) {
    if (ctx->mInit == false) {
      eptz_init(ctx);
    }
    ctx->mCurrentInfo[UVC_EPTZ_AUTO] = 1;
  } else {
    ctx->mCurrentInfo[UVC_EPTZ_AUTO] = ctx->mSetInfo[UVC_EPTZ_AUTO];
  }

  if (ctx->mCurrentInfo[UVC_EPTZ_AUTO] == 1 && faceinfo) {
    int faceCount = faceinfo->count;
    if (faceCount > MAX_OBJECT_NUM) { // max to MAX_OBJECT_NUM
      LOG_ERROR("eptz_process faceCount:%d exceed max:%d\n", faceCount,
                MAX_OBJECT_NUM);
      faceCount = MAX_OBJECT_NUM;
    }
    ctx->mAiData.face_count = faceCount;
    if (ctx->mUvcCfg->eptz_cfg.debug) {
      LOG_INFO("eptz_process do auto frame \n");
      LOG_INFO("eptz_process faceCount:%d\n", faceCount);
    }
    if (ctx->mAiData.face_count) {
      for (int i = 0; i < faceCount; i++) {
        ctx->mAiData.face_data[i].left =
            faceinfo->boxes[i].x * ctx->mNnResultRatio[HORIZONTAL];
        ctx->mAiData.face_data[i].top =
            faceinfo->boxes[i].y * ctx->mNnResultRatio[VERTICAL];
        ctx->mAiData.face_data[i].right =
            (faceinfo->boxes[i].x + faceinfo->boxes[i].w) *
            ctx->mNnResultRatio[HORIZONTAL];
        ctx->mAiData.face_data[i].bottom =
            (faceinfo->boxes[i].y + faceinfo->boxes[i].h) *
            ctx->mNnResultRatio[VERTICAL];
        ctx->mAiData.face_data[i].score = faceinfo->boxes[i].score;
        if (ctx->mUvcCfg->eptz_cfg.debug) {
          LOG_ERROR(
              "eptz_process i:%d in[%d:%d:%d:%d scode:%0.1f] \n", i,
              ctx->mAiData.face_data[i].left, ctx->mAiData.face_data[i].top,
              ctx->mAiData.face_data[i].right, ctx->mAiData.face_data[i].bottom,
              ctx->mAiData.face_data[i].score);
        }
      }
      calculateClipRect(&ctx->mAiData, ctx->mPosition, false, 0);
      mEptzOffsetX = ctx->mPosition[0];
      mEptzOffsetY = ctx->mPosition[1];
      mEptzWidth = ctx->mPosition[2];
      mEptzHeight = ctx->mPosition[3];

    } else {
      calculateClipRect(&ctx->mAiData, ctx->mPosition, true, 5);
      mEptzOffsetX = ctx->mPosition[0];
      mEptzOffsetY = ctx->mPosition[1];
      mEptzWidth = ctx->mPosition[2];
      mEptzHeight = ctx->mPosition[3];
    }
    if (ctx->mUvcCfg->eptz_cfg.debug) {
      LOG_ERROR("eptz_process out[%d %d %d %d]\n", mEptzOffsetX, mEptzOffsetY,
                mEptzWidth, mEptzHeight);
    }
  }
  if (ctx->mUvcCfg->eptz_cfg.debug) {
    LOG_ERROR(
        "eptz_process enter , mZoomValue %d mZoomValueNow %d mPanValue %d "
        "mPanValueNow %d mTiltValue %d mTiltValueNow %d\n",
        ctx->mSetInfo[UVC_EPTZ_ZOOM], ctx->mCurrentInfo[UVC_EPTZ_ZOOM],
        ctx->mSetInfo[UVC_EPTZ_PAN], ctx->mCurrentInfo[UVC_EPTZ_PAN],
        ctx->mSetInfo[UVC_EPTZ_TILT], ctx->mCurrentInfo[UVC_EPTZ_TILT]);
  }
  if (ctx->mSetInfo[UVC_EPTZ_ZOOM] > 10 ||
      ctx->mCurrentInfo[UVC_EPTZ_ZOOM] > 10) {
    mIsZoomSet = true;
    if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] > ctx->mSetInfo[UVC_EPTZ_ZOOM]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] - 10 > ctx->mSetInfo[UVC_EPTZ_ZOOM])
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] -= 8;
      else if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] - 5 >
               ctx->mSetInfo[UVC_EPTZ_ZOOM])
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] -= 4;
      else
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] -= 1;
    } else if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] <
               ctx->mSetInfo[UVC_EPTZ_ZOOM]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] + 10 < ctx->mSetInfo[UVC_EPTZ_ZOOM])
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] += 8;
      else if (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] + 5 <
               ctx->mSetInfo[UVC_EPTZ_ZOOM])
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] += 4;
      else
        ctx->mCurrentInfo[UVC_EPTZ_ZOOM] += 1;
    }
  } else {
    mIsZoomSet = false;
  }

  int zoomIndex = (ctx->mCurrentInfo[UVC_EPTZ_ZOOM] - 10);
  float ratio = (float)(mEptzWidth) / (float)(mSrcWidth);
  float zoomPerCrease = mSrcWidth * 1.0 / 16.0 / 80.0 * ratio;
  if (ctx->mUvcCfg->eptz_cfg.debug)
    LOG_ERROR("eptz_process zoomIndex %d , ratio %.2f zoomPerCrease %.2f \n",
              zoomIndex, ratio, zoomPerCrease);

  float mEptzWidthF, mEptzHeightF, mEptzOffsetXF, mEptzOffsetYF;
  mEptzWidthF = (float)(mEptzWidth)-zoomIndex * zoomPerCrease * 16;
  mEptzHeightF = (float)(mEptzHeight)-zoomIndex * zoomPerCrease * 9;

  if (mIsZoomSet == false) {
    if (ctx->mCurrentInfo[UVC_EPTZ_PAN] > ctx->mSetInfo[UVC_EPTZ_PAN]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_PAN] - 8 > ctx->mSetInfo[UVC_EPTZ_PAN])
        ctx->mCurrentInfo[UVC_EPTZ_PAN] -= 6;
      else if (ctx->mCurrentInfo[UVC_EPTZ_PAN] - 5 >
               ctx->mSetInfo[UVC_EPTZ_PAN])
        ctx->mCurrentInfo[UVC_EPTZ_PAN] -= 3;
      else
        ctx->mCurrentInfo[UVC_EPTZ_PAN] -= 1;
    } else if (ctx->mCurrentInfo[UVC_EPTZ_PAN] < ctx->mSetInfo[UVC_EPTZ_PAN]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_PAN] + 8 < ctx->mSetInfo[UVC_EPTZ_PAN])
        ctx->mCurrentInfo[UVC_EPTZ_PAN] += 6;
      else if (ctx->mCurrentInfo[UVC_EPTZ_PAN] + 5 <
               ctx->mSetInfo[UVC_EPTZ_PAN])
        ctx->mCurrentInfo[UVC_EPTZ_PAN] += 3;
      else
        ctx->mCurrentInfo[UVC_EPTZ_PAN] += 1;
    }

    if (ctx->mCurrentInfo[UVC_EPTZ_TILT] > ctx->mSetInfo[UVC_EPTZ_TILT]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_TILT] - 8 > ctx->mSetInfo[UVC_EPTZ_TILT])
        ctx->mCurrentInfo[UVC_EPTZ_TILT] -= 6;
      else if (ctx->mCurrentInfo[UVC_EPTZ_TILT] - 5 >
               ctx->mSetInfo[UVC_EPTZ_TILT])
        ctx->mCurrentInfo[UVC_EPTZ_TILT] -= 3;
      else
        ctx->mCurrentInfo[UVC_EPTZ_TILT] -= 1;
    } else if (ctx->mCurrentInfo[UVC_EPTZ_TILT] <
               ctx->mSetInfo[UVC_EPTZ_TILT]) {
      if (ctx->mCurrentInfo[UVC_EPTZ_TILT] + 8 < ctx->mSetInfo[UVC_EPTZ_TILT])
        ctx->mCurrentInfo[UVC_EPTZ_TILT] += 6;
      else if (ctx->mCurrentInfo[UVC_EPTZ_TILT] + 5 <
               ctx->mSetInfo[UVC_EPTZ_TILT])
        ctx->mCurrentInfo[UVC_EPTZ_TILT] += 3;
      else
        ctx->mCurrentInfo[UVC_EPTZ_TILT] += 1;
    }
  } else {
    ctx->mCurrentInfo[UVC_EPTZ_PAN] = ctx->mSetInfo[UVC_EPTZ_PAN];
    ctx->mCurrentInfo[UVC_EPTZ_TILT] = ctx->mSetInfo[UVC_EPTZ_TILT];
  }

  float pan_value = (zoomIndex * zoomPerCrease * 16) *
                    (ctx->mCurrentInfo[UVC_EPTZ_PAN] + UVC_EPTZ_PAN_MAX) /
                    UVC_EPTZ_PAN_COUNT;
  float tilt_value = (zoomIndex * zoomPerCrease * 9) *
                     (ctx->mCurrentInfo[UVC_EPTZ_TILT] + UVC_EPTZ_TILT_MAX) /
                     UVC_EPTZ_TILT_COUNT;
  mEptzOffsetXF = (float)(mEptzOffsetX) + pan_value;
  mEptzOffsetYF = (float)(mEptzOffsetY) + tilt_value;
  if (REQUEST16B9 && ((mDstWidth == 1024 && mDstHeight == 768) ||
                      (mDstWidth == 640 && mDstHeight == 480) ||
                      (mDstWidth == 320 && mDstHeight == 240))) {
    float modify_x = mEptzWidthF - (float)(mEptzHeightF)*4 / 3;
    mEptzWidthF = mEptzWidthF - modify_x;
    mEptzOffsetXF = mEptzOffsetXF + modify_x / 2;
    if (ctx->mUvcCfg->eptz_cfg.debug)
      LOG_ERROR("eptz_process 640x480 2 16:9 modify_x %.3f mEptzWidth %.3f "
                "mEptzOffsetX %.3f \n",
                modify_x, mEptzWidthF, mEptzOffsetXF);
  }

  VPSS_CROP_INFO_S stCropInfo;
  memset(&stCropInfo, 0, sizeof(VPSS_CROP_INFO_S));

  stCropInfo.bEnable = RK_TRUE;
  stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
  stCropInfo.stCropRect.s32X =
      eptz_align(ctx->mUvcCfg->eptz_cfg.align, mEptzOffsetXF);
  stCropInfo.stCropRect.s32Y =
      eptz_align(ctx->mUvcCfg->eptz_cfg.align, mEptzOffsetYF);
  stCropInfo.stCropRect.u32Width =
      eptz_align(ctx->mUvcCfg->eptz_cfg.align, mEptzWidthF);
  stCropInfo.stCropRect.u32Height =
      eptz_align(ctx->mUvcCfg->eptz_cfg.align, mEptzHeightF);

  uvc_vpss_config_crop(*ctx->mUvcCfg, stCropInfo, MPI_VPSS_CHANNEL_TYPE_UVC);

  if (ctx->mUvcCfg->eptz_cfg.debug)
    LOG_ERROR(
        "eptz_process mEPTZ ZOOM pt[%.3f %.3f] 2Align xywh[%d %d %d %d]\n",
        pan_value, tilt_value, stCropInfo.stCropRect.s32X,
        stCropInfo.stCropRect.s32Y, stCropInfo.stCropRect.u32Width,
        stCropInfo.stCropRect.u32Height);
}
