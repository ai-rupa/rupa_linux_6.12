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

#ifndef __NN_PROCESS_H__
#define __NN_PROCESS_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "uvc_mpi_config.h"
#include "rk_comm_video.h"
#include "uvc_mpi_vi.h"
typedef struct NNProcess NNProcess;

NNProcess* createNNProcess(UVC_MPI_CFG *uvcCfg);
int destroyNNProcess(NNProcess *process);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include "face_detector.h"
#include "face_pose.h"

class NNProcess {
public:
    NNProcess();
    ~NNProcess();

    int configProcess(UVC_MPI_CFG *uvcCfg);
    
    int createDetector(UVC_MPI_CFG *uvcCfg);
    
    int destroyDetector();

    int startProcess();

    int stopProcess();

    int doProcess();

    FaceInfo getFaceInfo();

  private:
    int configVi();
    int startVi();
    int stopVi();

    void processLoop();

    void handleFrame(const VIDEO_FRAME_INFO_S& videoFrame);

    void updateAiqAndOsd();

private:
    UVC_MPI_CFG *m_uvcCfg = nullptr;
    UVC_VI_CHN_CTX_S m_viChnCtx = {0};

    std::atomic<bool> m_running{false};
    bool m_is_face_detected = false;
    std::thread m_thread;

    uint64_t m_doCount = 0;
    std::chrono::steady_clock::time_point m_lastLogTime;

    IFaceDetector *m_detector = nullptr;
    FacePoseEstimator *m_facePose = nullptr;

    // frame info for face pose calculation
    int m_frameFd = -1;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
};

#endif // __cplusplus

#endif // __NN_PROCESS_H__