/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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
 */

#ifndef SRC_TESTS_RT_MPI_COMMON_TEST_COMM_VI_H_
#define SRC_TESTS_RT_MPI_COMMON_TEST_COMM_VI_H_

#include "rk_common.h"
#include "rk_comm_vi.h"
#include "test_common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef struct _COMMON_TEST_VI_CTX_S {
    RK_U32 width;
    RK_U32 height;
    RK_U32 maxWidth;
    RK_U32 maxHeight;
    RK_S32 devId;
    RK_S32 pipeId;
    RK_S32 channelId;
    RK_S32 saveFrameLoop;
    RK_BOOL bNoUseLibV4L2;
    RK_BOOL bMirror;
    RK_BOOL bFlip;
    RK_BOOL bOffLineMode;
    RK_BOOL bDevBindPipe;
    COMPRESS_MODE_E enCompressMode;
    VI_DEV_ATTR_S stDevAttr;
    VI_PIPE_ATTR_S stPipeAttr;
    VI_DEV_BIND_PIPE_S stBindPipe;
    VI_CHN_ATTR_S stChnAttr;
    VI_SAVE_FILE_INFO_S stDebugFile;
    const char *viSaveFileName;
} COMMON_TEST_VI_CTX_S;

RK_S32 TEST_VI_Start(COMMON_TEST_VI_CTX_S *viCtx);
RK_S32 TEST_VI_Stop(COMMON_TEST_VI_CTX_S *viCtx);
RK_S32 TEST_VI_DEV_CREATE(COMMON_TEST_VI_CTX_S *viCtx);
RK_S32 TEST_VI_DEV_DESTROY(RK_S32 s32DevId);
RK_S32 TEST_VI_PIPE_CREATE(COMMON_TEST_VI_CTX_S *viCtx);
RK_S32 TEST_VI_PIPE_DESTROY(RK_S32 s32PipeId);
RK_S32 TEST_VI_CHANNEL_CREATE(COMMON_TEST_VI_CTX_S *viCtx);
RK_S32 TEST_VI_CHANNEL_DESTROY(COMMON_TEST_VI_CTX_S *viCtx);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif  // SRC_TESTS_RT_MPI_COMMON_TEST_COMM_VI_H_
