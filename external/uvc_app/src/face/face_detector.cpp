// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "face_detector.h"
#include "uvc_log.h"
#include <cstring>
#include <mutex>

void IFaceDetector::insertFrame(int frameId, void *frame,
                                UVC_VI_CHN_CTX_S *viCtx) {
  if (!frame || !viCtx) {
    LOG_ERROR("Invalid input: frame or viCtx is null\n");
    return;
  }

  std::lock_guard<std::mutex> lock(mFrameCacheMutex);
  bool inserted = false;
  for (int i = 0; i < VI_MAX_INPUT_COUNT; ++i) {
    if (!mFrameCache[i].frame) {
      mFrameCache[i].frameId = frameId;
      mFrameCache[i].frame = frame;
      mFrameCache[i].viCtx = *viCtx;
      inserted = true;
      break;
    }
  }

  if (!inserted) {
    LOG_WARN("Frame cache full, drop frame id:%d\n", frameId);
    VIDEO_FRAME_INFO_S stViFrameInfo = {0};
    stViFrameInfo.stVFrame.pMbBlk = (MB_BLK)frame;
    RK_MPI_VI_ReleaseChnFrame(viCtx->pipeId, viCtx->channelId, &stViFrameInfo);
  }
}

void IFaceDetector::releaseFrameById(int frameId) {
  std::lock_guard<std::mutex> lock(mFrameCacheMutex);
  for (int i = 0; i < VI_MAX_INPUT_COUNT; ++i) {
    if (mFrameCache[i].frame && mFrameCache[i].frameId == frameId) {
      MB_BLK blk = (MB_BLK)mFrameCache[i].frame;
      if (blk) {
        VIDEO_FRAME_INFO_S stViFrameInfo = {0};
        stViFrameInfo.stVFrame.pMbBlk = blk;
        RK_S32 ret = RK_MPI_VI_ReleaseChnFrame(mFrameCache[i].viCtx.pipeId,
                                              mFrameCache[i].viCtx.channelId,
                                              &stViFrameInfo);
        if (ret != RK_SUCCESS) {
          LOG_ERROR("Failed to release frame id:%d ret:%x\n",
                    mFrameCache[i].frameId, ret);
        }
      }
      LOG_DEBUG("release i:%d id:%d frame:%p\n", i, frameId,
                mFrameCache[i].frame);
      mFrameCache[i].frame = nullptr;
      break;
    }
  }
}

void IFaceDetector::releaseAllFrames() {
  std::lock_guard<std::mutex> lock(mFrameCacheMutex);
  for (int i = 0; i < VI_MAX_INPUT_COUNT; ++i) {
    if (mFrameCache[i].frame) {
      MB_BLK blk = (MB_BLK)mFrameCache[i].frame;
      if (blk) {
        VIDEO_FRAME_INFO_S stViFrameInfo = {0};
        stViFrameInfo.stVFrame.pMbBlk = blk;
        RK_S32 ret = RK_MPI_VI_ReleaseChnFrame(mFrameCache[i].viCtx.pipeId,
                                  mFrameCache[i].viCtx.channelId,
                                  &stViFrameInfo);
        if (ret != RK_SUCCESS) {
          LOG_ERROR("Failed to release frame id:%d ret:%x\n",
                    mFrameCache[i].frameId, ret);
        }
      }
      LOG_DEBUG("release all i:%d id:%d frame:%p\n", i, mFrameCache[i].frameId,
                mFrameCache[i].frame);
      mFrameCache[i].frame = nullptr;
    }
  }
}