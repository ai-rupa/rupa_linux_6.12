// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "face_pose.h"
#include "lib_face_api.h"
#include "uvc_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "face_pose.cpp"

#if defined(UVC_ENABLE_FACE_POSE)

FacePoseEstimator::FacePoseEstimator() : mContext(nullptr) {
}

FacePoseEstimator::~FacePoseEstimator() {
    deinit();
}

bool FacePoseEstimator::init(const char *model_name) {
    if (!model_name || model_name[0] == '\0') {
        LOG_ERROR("model_name is null or empty\n");
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);

    if (mContext) {
        LOG_WARN("FacePoseEstimator already initialized\n");
        return true;
    }

    mContext = facePose_init(model_name);
    if (!mContext) {
        LOG_ERROR("facePose_init failed\n");
        return false;
    }

    LOG_INFO("FacePoseEstimator initialized with model: %s\n", model_name);
    return true;
}

void FacePoseEstimator::deinit() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mContext) {
        facePose_deinit(mContext);
        mContext = nullptr;
        LOG_INFO("FacePoseEstimator deinitialized\n");
    }
}

int FacePoseEstimator::run(int fd, void* virAddr, int width, int height, FaceImageFomat fmt,
                  const int* x1, const int* y1, const int* x2, const int* y2,
                  int faceCount, float* yaw, float* pitch, float* roll) {
    if (!mContext) {
        return -1;
    }

    if (faceCount <= 0 || !x1 || !y1 || !x2 || !y2 || !yaw || !pitch || !roll) {
        return -1;
    }

    LOG_DEBUG("face pose run: fd=%d, w=%d, h=%d, fmt=%d, faceCount=%d\n",
              fd, width, height, fmt, faceCount);

    int validCount = 0;

    for (int i = 0; i < faceCount; i++) {
        float y, p, r;
        int ret = facePose_run(mContext, fd, virAddr, width, height, fmt,
                               x1[i], y1[i], x2[i], y2[i],
                               &y, &p, &r);
        if (ret == 0) {
            yaw[i] = y;
            pitch[i] = p;
            roll[i] = r;
            validCount++;
            LOG_DEBUG("face %d: x1=%d, y1=%d, x2=%d, y2=%d, yaw=%.1f, pitch=%.1f, roll=%.1f\n",
                      i, x1[i], y1[i], x2[i], y2[i], y, p, r);
        } else {
            yaw[i] = 0;
            pitch[i] = 0;
            roll[i] = 0;
            LOG_DEBUG("face %d: pose estimation failed ret=%d\n", i, ret);
        }
    }

    LOG_DEBUG("face pose done: validCount=%d\n", validCount);
    return validCount;
}

#endif // UVC_ENABLE_FACE_POSE
