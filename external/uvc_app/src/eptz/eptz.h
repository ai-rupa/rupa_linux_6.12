/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * author: kevin.lin@rock-chips.com
 *   date: 2020-09-18
 * module: eptz
 */

#ifndef EPTZ_H_
#define EPTZ_H_

#include "../face/face_detector.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "eptz_algorithm.h"
#include "uvc_mpi_config.h"
#include "uvc_process_unit.h"

#define MAX_OBJECT_NUM 40

typedef struct _eptzCtx {
  EptzInitInfo mInitInfo;
  EptzAiData mAiData;
  UVC_MPI_CFG *mUvcCfg;
  int mSetInfo[UVC_EPTZ_MAX];
  int mCurrentInfo[UVC_EPTZ_MAX];
  int mPosition[4];
  bool mInit;
  float mNnResultRatio[UVC_RATIO_MAX];
} EPTZ_CTX_S;

int eptz_init(EPTZ_CTX_S *ctx);
int eptz_deinit(EPTZ_CTX_S *ctx);
void eptz_config(EPTZ_CTX_S *ctx, UVC_CTRL_INFO uvcCtrlInfo);
void eptz_process(EPTZ_CTX_S *ctx, FaceInfo *faceinfo);

#ifdef __cplusplus
}
#endif

#endif // EPTZ_H_
