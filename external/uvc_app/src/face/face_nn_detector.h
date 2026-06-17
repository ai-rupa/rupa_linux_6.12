// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __FACE_NN_DETECTOR_H__
#define __FACE_NN_DETECTOR_H__

#include "face_detector.h"
#include "lib_face_api.h"
#include <mutex>

class FaceNNDetector : public IFaceDetector {
public:
  static FaceNNDetector *create(MpiNnCfg *cfg);

  void destroy() override;

  int writeRGB888Frame(uint16_t width, uint16_t height, uint32_t frame_id,
                       unsigned char *buffer) override;
  int writeRGB888FrameByFd(uint16_t width, uint16_t height, uint32_t frame_id,
                           int32_t fd) override;
  int writeNV12FrameByFd(uint16_t width, uint16_t height, uint32_t frame_id,
                         int32_t fd) override;
  void getFaceInfo(FaceInfo *outInfo) override;

private:
  FaceNNDetector();
  ~FaceNNDetector();

  FaceInfo mFaceInfo;
  void *mContext;
  std::mutex mFaceInfoMutex;
};

#endif // __FACE_NN_DETECTOR_H__
