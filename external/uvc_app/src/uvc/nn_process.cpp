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

#include "nn_process.h"
#include "camera_control.h"
#include "osd.h"
#include "uvc_mpi_config.h"
#include "uvc_mpi_vi.h"
#include "param.h"
#include "uvc_log.h"
#include "rk_common.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"

#if defined(UVC_ENABLE_IVA)
#include "rockiva_detector.h"
#endif

#if defined(UVC_ENABLE_FACE_NN)
#include "face_nn_detector.h"
#endif

#include <cstring>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <chrono>

#define NN_DYNAMIC_DEBUG_FPS "/tmp/nn_fps"
// use the apis when need to create/destroy nn process in C style
extern "C" {
    NNProcess* createNNProcess(UVC_MPI_CFG *uvcCfg) {
        LOG_INFO("enter");
        if (!uvcCfg) {
            return nullptr;
        }

        NNProcess *process = new (std::nothrow) NNProcess();
        if (!process) {
            LOG_ERROR("Failed to allocate NNProcess\n");
            return nullptr;
        }

        if (process->createDetector(uvcCfg) != 0) {
            LOG_ERROR("Failed to create NN Detector\n");
            delete process;
            return nullptr;
        }

        return process;
    }
    
    int destroyNNProcess(NNProcess *process) {
        if (!process) {
            LOG_ERROR("Invalid NNProcess pointer\n");
            return -1;
        }

        int ret = process->destroyDetector();
        if (ret != 0) {
          LOG_ERROR("Failed to destroy NN Detector\n");
          return ret;
        }
        delete process;
        process = nullptr;
        return 0;
    }
}

NNProcess::NNProcess() {
}

NNProcess::~NNProcess() {
}

int NNProcess::configProcess(UVC_MPI_CFG *uvcCfg) {
    if (!uvcCfg) return -1;
    m_uvcCfg = uvcCfg;
    return configVi();
}

int NNProcess::createDetector(UVC_MPI_CFG *uvcCfg) {
  if (m_detector) {
    LOG_WARN("NN Detector already created\n");
    return 0;
  }

#if defined(UVC_ENABLE_FACE_POSE)
  // init face pose estimator
  m_facePose = nullptr;
  if (uvcCfg->nn_cfg.enable_face_pose && uvcCfg->nn_cfg.face_nn_model_name[0] != '\0') {
    m_facePose = new FacePoseEstimator();
    if (!m_facePose->init(uvcCfg->nn_cfg.face_nn_model_name)) {
      LOG_WARN("face pose init failed, face pose disabled\n");
      delete m_facePose;
      m_facePose = nullptr;
    } else {
      LOG_INFO("face pose init success in nn_process\n");
    }
  }
#endif

#if defined(UVC_ENABLE_IVA)
  if (uvcCfg->nn_cfg.enable_rockiva) {
    m_detector = RockivaDetector::create(
        uvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_width,
        uvcCfg->vi_cfg[MPI_VI_CHANNEL_TYPE_NN].assign_height,
        &uvcCfg->nn_cfg);
  }
#endif

#if defined(UVC_ENABLE_FACE_NN)
  if (uvcCfg->nn_cfg.enable_face_nn && m_detector == nullptr) {
    m_detector = FaceNNDetector::create(&uvcCfg->nn_cfg);
  }
#endif

  if (!m_detector) {
    LOG_ERROR("create face detector failed\n");
    return -1;
  }
  return 0;
}

int NNProcess::destroyDetector() {
    if (m_running) {
        LOG_WARN("Cannot destroy detector while NN process is running\n");
        return -1;
    }

#if defined(UVC_ENABLE_FACE_POSE)
    if (m_facePose) {
        delete m_facePose;
        m_facePose = nullptr;
    }
#endif

    if (m_detector) {
        m_detector->destroy();
        delete m_detector;
        m_detector = nullptr;
    }
    return 0;
}

int NNProcess::startProcess() {
    if (m_running) {
        LOG_WARN("NN Process already running\n");
        return 0;
    }
    
    if(m_uvcCfg == nullptr){
        LOG_ERROR("UVC configuration is not set\n");
        return 0;
    }

    int ret = createDetector(m_uvcCfg);

    if (ret != 0) {
        LOG_ERROR("Failed to create NN Detector\n");
        return -1;
    }

    if (startVi() != 0) {
        LOG_ERROR("Failed to start VI\n");
        if (m_detector) {
            destroyDetector();
        }
        return -1;
    }

    m_running = true;
    m_doCount = 0;
    m_lastLogTime = std::chrono::steady_clock::now();
    m_thread = std::thread(&NNProcess::processLoop, this);

    LOG_INFO("NN Process started\n");
    return 0;
}

int NNProcess::stopProcess() {
    if (m_running) {
        LOG_INFO("NN process stopping thread...\n");
        m_running = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    if (m_detector) {
        LOG_INFO("NN process destroying detector...\n");
        destroyDetector();
    }

    stopVi();

    LOG_INFO("NN process exit ok\n");
    return 0;
}

int NNProcess::configVi() {
    if (!m_uvcCfg) return -1;
    uvc_vi_config(m_uvcCfg, MPI_VI_CHANNEL_TYPE_NN);
    m_viChnCtx = uvc_get_vi_chn_ctx(*m_uvcCfg, MPI_VI_CHANNEL_TYPE_NN);
    return 0;
}

int NNProcess::startVi() {
    if (!m_uvcCfg) return -1;
    uvc_vi_start(*m_uvcCfg, MPI_VI_CHANNEL_TYPE_NN);
    return 0;
}

int NNProcess::stopVi() {
    if (!m_uvcCfg) return -1;
    uvc_vi_stop(*m_uvcCfg, MPI_VI_CHANNEL_TYPE_NN);
    return 0;
}

void NNProcess::processLoop() {
    prctl(PR_SET_NAME, "nnProcessThread");
    
    VIDEO_FRAME_INFO_S videoFrame;
    RK_S32 s32Ret;

    while (m_running) {
        s32Ret = RK_MPI_VI_GetChnFrame(m_viChnCtx.pipeId, m_viChnCtx.channelId,
                                       &videoFrame, 1000);

        if (s32Ret == RK_SUCCESS) {
            handleFrame(videoFrame);
        } else {
            if (s32Ret != RK_ERR_VI_BUF_EMPTY) {
                 LOG_WARN("Get frame warning: %x pipe:%d chn:%d\n", 
                          s32Ret, m_viChnCtx.pipeId, m_viChnCtx.channelId);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void NNProcess::handleFrame(const VIDEO_FRAME_INFO_S& videoFrame) {
    RK_S32 fd = RK_MPI_MB_Handle2Fd(videoFrame.stVFrame.pMbBlk);
    RK_U32 width = videoFrame.stVFrame.u32Width;
    RK_U32 height = videoFrame.stVFrame.u32Height;
    RK_U32 seq = videoFrame.stVFrame.u32TimeRef;

    LOG_DEBUG("nn w:%d h:%d seq:%d fd:%d\n", width, height, seq, fd);

    // save frame info for face pose calculation
    m_frameFd = fd;
    m_frameWidth = width;
    m_frameHeight = height;

    m_doCount++;

    if (m_detector) {
        m_detector->insertFrame((int)m_doCount, (void *)videoFrame.stVFrame.pMbBlk, &m_viChnCtx);
        m_detector->writeNV12FrameByFd(width, height, (int)m_doCount, fd);

        if (m_uvcCfg->common_cfg.uvc_debug && !access(NN_DYNAMIC_DEBUG_FPS, 0)) {
            if (m_doCount % 100 == 0) {
                auto now = std::chrono::steady_clock::now();
                auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastLogTime).count();
                m_lastLogTime = now;
                
                if (durationMs > 0) {
                    LOG_INFO("nn process fps:%0.1f\n", (1000.0 * 100) / durationMs);
                }
            }
        }
        
        updateAiqAndOsd();
    } else {
        RK_MPI_VI_ReleaseChnFrame(m_viChnCtx.pipeId, m_viChnCtx.channelId, &videoFrame);
    }
}

void NNProcess::updateAiqAndOsd() {
  if (!m_detector || !m_uvcCfg)
    return;

  int dev_id = m_uvcCfg->vi_cfg->dev_id;
  FaceInfo faceInfo;

  rk_isp_asd_get_face_win(dev_id, &faceInfo);
  if (faceInfo.count > 0)
    osd_draw_nn_osd_asd(m_uvcCfg, &faceInfo);

  faceInfo = getFaceInfo();

#if defined(UVC_ENABLE_FACE_POSE)
  // run face pose estimation if enabled
  LOG_DEBUG("face pose check: m_facePose=%p, isInit=%d, faceCount=%d, m_frameFd=%d, m_frameWidth=%d, m_frameHeight=%d\n",
           m_facePose, m_facePose ? m_facePose->isInitialized() : 0,
           faceInfo.count, m_frameFd, m_frameWidth, m_frameHeight);

  if (m_facePose && m_facePose->isInitialized() && faceInfo.count > 0 &&
      m_frameFd >= 0 && m_frameWidth > 0 && m_frameHeight > 0) {
    // prepare arrays for face pose
    int x1[MAX_DETECT_OBJECTS], y1[MAX_DETECT_OBJECTS];
    int x2[MAX_DETECT_OBJECTS], y2[MAX_DETECT_OBJECTS];
    float yaw[MAX_DETECT_OBJECTS], pitch[MAX_DETECT_OBJECTS], roll[MAX_DETECT_OBJECTS];

    for (int i = 0; i < faceInfo.count && i < MAX_DETECT_OBJECTS; i++) {
      x1[i] = faceInfo.boxes[i].x;
      y1[i] = faceInfo.boxes[i].y;
      x2[i] = faceInfo.boxes[i].x + faceInfo.boxes[i].w;
      y2[i] = faceInfo.boxes[i].y + faceInfo.boxes[i].h;
    }

    // run face pose
    LOG_DEBUG("face pose run: fd=%d, w=%d, h=%d, fmt=%d, faceCount=%d\n",
              m_frameFd, m_frameWidth, m_frameHeight,
              ISP_FACE_FORMAT_YUV420SP_NV12, faceInfo.count);
    for (int i = 0; i < faceInfo.count && i < MAX_DETECT_OBJECTS; i++) {
      LOG_DEBUG("face %d: x1=%d, y1=%d, x2=%d, y2=%d\n",
                i, x1[i], y1[i], x2[i], y2[i]);
    }

    m_facePose->run(m_frameFd, nullptr, m_frameWidth, m_frameHeight,
                    ISP_FACE_FORMAT_YUV420SP_NV12,
                    x1, y1, x2, y2, faceInfo.count, yaw, pitch, roll);

    // assign pose to face boxes
    for (int i = 0; i < faceInfo.count && i < MAX_DETECT_OBJECTS; i++) {
      faceInfo.boxes[i].yaw = (int)yaw[i];
      faceInfo.boxes[i].pitch = (int)pitch[i];
      faceInfo.boxes[i].roll = (int)roll[i];
      LOG_DEBUG("yaw %f, picth %f, roll: %f\n", yaw[i], pitch[i], roll[i]);
    }
  }
#endif

  if (faceInfo.count > 0) {
    rk_isp_asd_set_face_win(dev_id, &faceInfo);
    osd_draw_nn_osd(m_uvcCfg, &faceInfo);
    m_is_face_detected = true;
  } else {
    m_is_face_detected = false;
  }
  if (!m_is_face_detected) {
    osd_clean_nn_osd(m_uvcCfg);
    osd_clean_nn_osd_asd(m_uvcCfg);
  }
}

FaceInfo NNProcess::getFaceInfo() {
  FaceInfo faceInfo{};
  if (m_detector) {
    m_detector->getFaceInfo(&faceInfo);
  }
    return faceInfo;
}

int NNProcess::doProcess() {
    return 0;
}