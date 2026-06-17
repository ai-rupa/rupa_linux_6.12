// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rockiva_detector.h"
#include "uvc_log.h"
#include <cstring>
#include <mutex>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rockiva_detector.cpp"
static std::mutex mtx;

RockivaDetector *RockivaDetector::create(int width, int height, MpiNnCfg *cfg) {
  std::lock_guard<std::mutex> lock(mtx);
  RockivaDetector *detector = new (std::nothrow) RockivaDetector();
  if (!detector) {
    LOG_ERROR("Failed to allocate RockivaDetector\n");
    return nullptr;
  }

  RockIvaDetTaskParams initParams;
  RockIvaInitParam globalParams;
  RockIvaRetCode ret;

  memset(&initParams, 0, sizeof(initParams));
  memset(&globalParams, 0, sizeof(globalParams));

  globalParams.coreMask = 0x04;
  globalParams.newModelInstance = 1; //force multi-instance
  globalParams.logLevel = ROCKIVA_LOG_ERROR;
  if (cfg->rockiva_model_name[0] == '\0') {
    snprintf(globalParams.modelPath, ROCKIVA_PATH_LENGTH, "/oem/usr/lib");
    LOG_INFO("Using default model path: %s\n", globalParams.modelPath);
  }else{
    globalParams.detModelName = cfg->rockiva_model_name;
    LOG_INFO("Using model name: %s\n", globalParams.detModelName);
  }

  globalParams.imageInfo.width = width;
  globalParams.imageInfo.height = height;

  LOG_INFO("iva init width %d, height %d\n", width, height);

  ret = ROCKIVA_Init(&detector->mHandle, ROCKIVA_MODE_VIDEO, &globalParams,
                     (void *)detector);
  if (ret != ROCKIVA_RET_SUCCESS) {
    LOG_ERROR("ROCKIVA_Init error %d\n", ret);
    delete detector;
    return nullptr;
  }

  initParams.detObjectType |=
      ROCKIVA_OBJECT_TYPE_BITMASK(ROCKIVA_OBJECT_TYPE_FACE);
  initParams.scores[0] = cfg->rockiva_threshold * 100;
  LOG_INFO("iva score %f\n", cfg->rockiva_threshold * 100);

  ret = ROCKIVA_DETECT_Init(detector->mHandle, &initParams,
                            RockivaDetector::staticCallback);
  if (ret != ROCKIVA_RET_SUCCESS) {
    LOG_ERROR("ROCKIVA_DETECT_Init error %d\n", ret);
    ROCKIVA_Release(detector->mHandle);
    delete detector;
    return nullptr;
  }

  LOG_INFO("RockivaDetector init success\n");
  return detector;
}

void RockivaDetector::destroy() {
  std::lock_guard<std::mutex> lock(mtx);
  if (mHandle) {
    ROCKIVA_DETECT_Release(mHandle);
    ROCKIVA_Release(mHandle);
  }
  LOG_INFO("RockivaDetector resources released\n");
}

RockivaDetector::RockivaDetector() : mHandle(nullptr) {}

RockivaDetector::~RockivaDetector() {
  mHandle = nullptr;
  LOG_INFO("RockivaDetector deinit\n");
}

void RockivaDetector::staticCallback(const RockIvaDetectResult *result,
                                     const RockIvaExecuteStatus status,
                                     void *userData) {
  if (!result) {
    LOG_ERROR("RockivaDetector staticCallback result is null\n");
    return;
  }

  RockivaDetector *detector = reinterpret_cast<RockivaDetector *>(userData);
  if (!detector) {
    LOG_ERROR("RockivaDetector staticCallback detector is null\n");
    return;
  }

  if (status == ROCKIVA_SUCCESS) {
    std::lock_guard<std::mutex> lock(detector->mFaceInfoMutex);
    detector->mFaceInfo.count = 0;

    for (uint32_t i = 0; i < result->objNum && i < MAX_DETECT_OBJECTS; i++) {
      if (result->objInfo[i].type == ROCKIVA_OBJECT_TYPE_FACE) {
        FaceBox &box = detector->mFaceInfo.boxes[detector->mFaceInfo.count];

        box.x = result->objInfo[i].rect.topLeft.x;
        box.y = result->objInfo[i].rect.topLeft.y;
        box.w = result->objInfo[i].rect.bottomRight.x -
                result->objInfo[i].rect.topLeft.x;
        box.h = result->objInfo[i].rect.bottomRight.y -
                result->objInfo[i].rect.topLeft.y;
        box.score = result->objInfo[i].score;
        box.id = result->objInfo[i].objId;
        box.yaw = 0;
        box.pitch = 0;
        box.roll = 0;

        detector->mFaceInfo.count++;
      }
    }
  }

  LOG_DEBUG("status is %d, frame %d, result->objNum is %d\n", status,
            result->frameId, result->objNum);

  for (int i = 0; i < result->objNum; i++) {
    LOG_DEBUG("topLeft:[%d,%d], bottomRight:[%d,%d],",
              result->objInfo[i].rect.topLeft.x,
              result->objInfo[i].rect.topLeft.y,
              result->objInfo[i].rect.bottomRight.x,
              result->objInfo[i].rect.bottomRight.y);

    LOG_DEBUG("objId is %d, frameId is %d, score is %d, type is %d\n",
              result->objInfo[i].objId, result->objInfo[i].frameId,
              result->objInfo[i].score, result->objInfo[i].type);
  }

  detector->releaseFrameById(result->frameId);
}

int RockivaDetector::writeRGB888Frame(uint16_t width, uint16_t height,
                                      uint32_t frame_id,
                                      unsigned char *buffer) {
  RockIvaImage image;
  memset(&image, 0, sizeof(RockIvaImage));

  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_BGR888;
  image.dataAddr = buffer;
  image.frameId = frame_id;

  int ret = ROCKIVA_PushFrame(mHandle, &image, nullptr);

  return ret;
}

int RockivaDetector::writeRGB888FrameByFd(uint16_t width, uint16_t height,
                                          uint32_t frame_id, int32_t fd) {
  RockIvaImage image;
  memset(&image, 0, sizeof(RockIvaImage));

  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_BGR888;
  image.frameId = frame_id;
  image.dataAddr = nullptr;
  image.dataPhyAddr = nullptr;
  image.dataFd = fd;

  int ret = ROCKIVA_PushFrame(mHandle, &image, nullptr);

  return ret;
}

int RockivaDetector::writeNV12FrameByFd(uint16_t width, uint16_t height,
                                        uint32_t frame_id, int32_t fd) {
  RockIvaImage image;
  memset(&image, 0, sizeof(RockIvaImage));

  image.info.width = width;
  image.info.height = height;
  image.info.format = ROCKIVA_IMAGE_FORMAT_YUV420SP_NV12;
  image.frameId = frame_id;
  image.dataAddr = nullptr;
  image.dataPhyAddr = nullptr;
  image.dataFd = fd;

  int ret = ROCKIVA_PushFrame(mHandle, &image, nullptr);

  return ret;
}

void RockivaDetector::getFaceInfo(FaceInfo *outInfo) {
  if (!outInfo) {
    return;
  }
  std::lock_guard<std::mutex> lock(mFaceInfoMutex);
  *outInfo = mFaceInfo;
}