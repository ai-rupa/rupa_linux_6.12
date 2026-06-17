// Copyright 2021 Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __FACE_POSE_H__
#define __FACE_POSE_H__

#include "lib_face_api.h"

#ifdef __cplusplus
#include <mutex>

#if defined(UVC_ENABLE_FACE_POSE)

class FacePoseEstimator {
public:
    FacePoseEstimator();
    ~FacePoseEstimator();

    bool init(const char *model_name);
    void deinit();

    // run face pose for each detected face
    // returns number of faces with pose data
    int run(int fd, void* virAddr, int width, int height, FaceImageFomat fmt,
            const int* x1, const int* y1, const int* x2, const int* y2,
            int faceCount, float* yaw, float* pitch, float* roll);

    bool isInitialized() const { return mContext != nullptr; }

private:
    void *mContext;
    std::mutex mMutex;
};

#else

// dummy class when FACE_POSE is disabled
class FacePoseEstimator {
public:
    FacePoseEstimator() {}
    ~FacePoseEstimator() {}
    bool init(const char* model_name) { (void)model_name; return false; }
    void deinit() {}
    int run(int fd, void* virAddr, int width, int height, FaceImageFomat fmt,
            const int* x1, const int* y1, const int* x2, const int* y2,
            int faceCount, float* yaw, float* pitch, float* roll) {
        (void)fd; (void)virAddr; (void)width; (void)height; (void)fmt;
        (void)x1; (void)y1; (void)x2; (void)y2;
        (void)faceCount; (void)yaw; (void)pitch; (void)roll;
        return 0;
    }
    bool isInitialized() const { return false; }
};

#endif // UVC_ENABLE_FACE_POSE

#endif // __cplusplus

#endif // __FACE_POSE_H__
