// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "face_nn_detector.h"
#include "lib_face_api.h"
#include "uvc_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "face_nn_detector.cpp"

FaceNNDetector::FaceNNDetector() : mContext(nullptr) {}

FaceNNDetector *FaceNNDetector::create(MpiNnCfg *cfg) {
  FaceNNDetector *detector = new (std::nothrow) FaceNNDetector();
  if (!detector) {
    LOG_ERROR("Failed to allocate FaceNNDetector\n");
    return nullptr;
  }
  if (cfg->face_nn_model_name == nullptr) {
    LOG_ERROR("model_name is nullptr, please check the model name.\n");
    return nullptr;
  }
  if (cfg->face_nn_threshold <= 0.0f || cfg->face_nn_threshold >= 1.0f) {
    LOG_ERROR("threshold is out of range, threshold: %f.\n", cfg->face_nn_threshold);
    return nullptr;
  }

  detector->mContext = face_net_init(cfg->face_nn_model_name, cfg->face_nn_threshold);
  if (!detector->mContext) {
    LOG_ERROR("face nn init failed\n");
    delete detector;
    return nullptr;
  }

  LOG_INFO("FaceNNDetector created successfully\n");
  return detector;
}

FaceNNDetector::~FaceNNDetector() {
  LOG_INFO("FaceNNDetector destroyed\n");
  mContext = nullptr;
}

void FaceNNDetector::destroy() {
  if (mContext) {
    face_net_deinit(mContext);
  }
  mContext = nullptr;
}

int FaceNNDetector::writeRGB888Frame(uint16_t width, uint16_t height,
                                     uint32_t frame_id, unsigned char *buffer) {
  LOG_ERROR("RGB888 format not supported by FaceNN\n");
  return -1;
}

int FaceNNDetector::writeRGB888FrameByFd(uint16_t width, uint16_t height,
                                         uint32_t frame_id, int32_t fd) {
  LOG_ERROR("RGB888 format not supported by FaceNN\n");
  return -1;
}

int FaceNNDetector::writeNV12FrameByFd(uint16_t width, uint16_t height,
                                       uint32_t frame_id, int32_t fd) {
  int ret = -1;
  FaceImageFomat fmt = ISP_FACE_FORMAT_YUV420SP_NV12;
  FaceResult resultOrg;
  FaceTrackerResult result;

  if (!mContext) {
    LOG_ERROR("FaceNNDetector not initialized\n");
    releaseFrameById(frame_id);
    return -1;
  }

  ret = face_net_run(mContext, fd, nullptr, width, height, fmt, &resultOrg, &result);
  if (ret != 0) {
    LOG_ERROR("face_net_run failed, ret: %d\n", ret);
    releaseFrameById(frame_id);
    return ret;
  }

  // 直接更新 mFaceInfo
  std::lock_guard<std::mutex> lock(mFaceInfoMutex);
  mFaceInfo.count = 0;

  for (int i = 0; i < result.face_num && i < MAX_DETECT_OBJECTS; i++) {
    FaceBox &box = mFaceInfo.boxes[mFaceInfo.count];

    box.x = result.rect[i].topLeft.x;
    box.y = result.rect[i].topLeft.y;
    box.w = result.rect[i].bottomRight.x - result.rect[i].topLeft.x;
    box.h = result.rect[i].bottomRight.y - result.rect[i].topLeft.y;
    box.score = result.rect[i].score;
    box.id = i;

    box.yaw   = 0;
    box.pitch = 0;
    box.roll  = 0;

    mFaceInfo.count++;
  }

  releaseFrameById(frame_id);

  return ret;
}

void FaceNNDetector::getFaceInfo(FaceInfo *outInfo) {
  if (!outInfo) {
    return;
  }
  std::lock_guard<std::mutex> lock(mFaceInfoMutex);
  *outInfo = mFaceInfo;
}
