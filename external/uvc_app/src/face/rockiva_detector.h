// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __ROCKIVA_DETECTOR_H__
#define __ROCKIVA_DETECTOR_H__

#include "face_detector.h"
#include "rockiva/rockiva_det_api.h"
#include <mutex>

class RockivaDetector : public IFaceDetector {
public:
  static RockivaDetector *create(int width, int height, MpiNnCfg *cfg);

  void destroy() override;

  int writeRGB888Frame(uint16_t width, uint16_t height, uint32_t frame_id,
                       unsigned char *buffer) override;
  int writeRGB888FrameByFd(uint16_t width, uint16_t height, uint32_t frame_id,
                           int32_t fd) override;
  int writeNV12FrameByFd(uint16_t width, uint16_t height, uint32_t frame_id,
                         int32_t fd) override;
  void getFaceInfo(FaceInfo *outInfo) override;

  static void staticCallback(const RockIvaDetectResult *result,
                             const RockIvaExecuteStatus status, void *userData);

private:
  RockivaDetector();
  ~RockivaDetector();

  RockIvaHandle mHandle;
  FaceInfo mFaceInfo;
  std::mutex mFaceInfoMutex;
};

#endif // __ROCKIVA_DETECTOR_H__
