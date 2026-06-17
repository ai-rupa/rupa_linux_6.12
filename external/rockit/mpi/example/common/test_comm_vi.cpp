/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "test_comm_vi.h"
#include "test_comm_imgproc.h"

#include "rk_comm_vi.h"
#include "rk_common.h"
#include "rk_debug.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"

#define TEST_VI_PATH_LENGTH         128
#define TEST_VI_TIME_OUT_MS         100

typedef struct test_vi_getstream_s {
     RK_BOOL bThreadStart;
     FILE *fp;
     pthread_t ViPid;
     MB_POOL pool;
     COMMON_TEST_VI_CTX_S stViCtx;
} TEST_VI_THREAD_S;

// get stream thread info
static TEST_VI_THREAD_S gGSThread[VI_MAX_PIPE_NUM][VI_MAX_CHN_NUM];

static RK_S32 TEST_VI_StartGetStream(COMMON_TEST_VI_CTX_S *viCtx);
static RK_S32 TEST_VI_StopGetStream(TEST_VI_THREAD_S *viCtx);

RK_S32 TEST_VI_DEV_CREATE(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;

    s32Ret = RK_MPI_VI_GetDevAttr(viCtx->devId, &viCtx->stDevAttr);
    if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
        s32Ret = RK_MPI_VI_SetDevAttr(viCtx->devId, &viCtx->stDevAttr);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_SetDevAttr %x", s32Ret);
            return s32Ret;
        }
    } else {
        RK_LOGW("RK_MPI_VI_SetDevAttr already");
    }
    RK_LOGI("RK_MPI_VI_SetDevAttr success");

    s32Ret = RK_MPI_VI_GetDevIsEnable(viCtx->devId);
    if (s32Ret != RK_SUCCESS) {
        s32Ret = RK_MPI_VI_EnableDev(viCtx->devId);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_EnableDev %x", s32Ret);
            return s32Ret;
        }
    } else {
        RK_LOGW("RK_MPI_VI_EnableDev already");
    }
    RK_LOGI("RK_MPI_VI_EnableDev success");
    return s32Ret;
}

RK_S32 TEST_VI_DEV_DESTROY(RK_S32 s32DevId) {
    RK_S32 s32Ret = 0;
    s32Ret = RK_MPI_VI_DisableDev(s32DevId);
    if (s32Ret) {
        RK_LOGE("RK_MPI_VI_DisableDev dev:%d fail%x", s32Ret);
        return s32Ret;
    }
    RK_LOGI("RK_MPI_VI_DisableDev success");
    return s32Ret;
}

RK_S32 TEST_VI_PIPE_CREATE(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;
    s32Ret = RK_MPI_VI_CreatePipe(viCtx->pipeId, &viCtx->stPipeAttr);
    if (s32Ret) {
        RK_LOGE("pipe:%d create pipe fail:%#X", viCtx->pipeId, s32Ret);
        return s32Ret;
    }

    if (viCtx->bDevBindPipe) {
        s32Ret = RK_MPI_VI_SetDevBindPipe(viCtx->devId, &viCtx->stBindPipe);
        if (s32Ret) {
            RK_LOGE("dev:%d bind pipe fail:%#X", viCtx->devId, s32Ret);
            return s32Ret;
        }
    }

    /* start pipe */
    s32Ret = RK_MPI_VI_StartPipe(viCtx->pipeId);
    if (s32Ret) {
        RK_LOGE("pipe:%d start pipe fail%#X", viCtx->pipeId, s32Ret);
        return s32Ret;
    }
    return s32Ret;
}

RK_S32 TEST_VI_PIPE_DESTROY(RK_S32 s32PipeId) {
    RK_S32 s32Ret = 0;
    s32Ret = RK_MPI_VI_StopPipe(s32PipeId);
    RK_LOGI("pipe:%d stop ret:%#X", s32PipeId, s32Ret);

    s32Ret = RK_MPI_VI_DestroyPipe(s32PipeId);
    RK_LOGI("pipe:%d destroy ret:%#X", s32PipeId, s32Ret);
    return s32Ret;
}

RK_S32 TEST_VI_CHANNEL_CREATE(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;
    s32Ret = RK_MPI_VI_SetChnAttr(viCtx->pipeId, viCtx->channelId, &viCtx->stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_SetChnAttr %x", s32Ret);
        return s32Ret;
    }
    RK_LOGI("RK_MPI_VI_SetChnAttr success");

    s32Ret = RK_MPI_VI_EnableChn(viCtx->pipeId, viCtx->channelId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_EnableChn %x", s32Ret);
        return s32Ret;
    }

    if (viCtx->stDebugFile.bCfg) {
        s32Ret = RK_MPI_VI_ChnSaveFile(viCtx->pipeId, viCtx->channelId, &viCtx->stDebugFile);
        RK_LOGD("RK_MPI_VI_ChnSaveFile %x", s32Ret);
    }

    return s32Ret;
}

RK_S32 TEST_VI_CHANNEL_DESTROY(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;
    s32Ret = RK_MPI_VI_DisableChn(viCtx->pipeId, viCtx->channelId);
    if (s32Ret) {
        RK_LOGE("RK_MPI_VI_DisableChn %x", s32Ret);
        return s32Ret;
    }
    return s32Ret;
}

RK_S32 TEST_VI_Start(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;

    s32Ret = TEST_VI_DEV_CREATE(viCtx);
    if (s32Ret) {
        RK_LOGE("TEST_VI_DEV_CREATE fail:%#x", s32Ret);
        return s32Ret;
    }

    VI_DEV_BIND_PIPE_S stDevBindPipe;
    memset(&stDevBindPipe, 0, sizeof(stDevBindPipe));
    stDevBindPipe.u32Num = 1;
    stDevBindPipe.PipeId[0] = viCtx->pipeId;
    s32Ret = RK_MPI_VI_SetDevBindPipe(viCtx->devId, &stDevBindPipe);
    if (s32Ret) {
        RK_LOGE("dev:%d bind pipe fail:%#X", viCtx->devId, s32Ret);
        return s32Ret;
    }

    s32Ret = TEST_VI_CHANNEL_CREATE(viCtx);
    if (s32Ret) {
        RK_LOGE("TEST_VI_CHANNEL_CREATE fail:%x", s32Ret);
        return s32Ret;
    }

    s32Ret = TEST_VI_StartGetStream(viCtx);
    return s32Ret;
}

RK_S32 TEST_VI_Stop(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;

    TEST_VI_StopGetStream(&gGSThread[viCtx->pipeId][viCtx->channelId]);

    s32Ret = TEST_VI_CHANNEL_DESTROY(viCtx);
    if (s32Ret) {
        RK_LOGE("TEST_VI_CHANNEL_DESTROY fail %x", s32Ret);
        return s32Ret;
    }

    s32Ret = TEST_VI_DEV_DESTROY(viCtx->devId);
    if (s32Ret) {
         RK_LOGE("TEST_VI_DEV_DESTROY fail %x", s32Ret);
        return s32Ret;
    }
    return s32Ret;
}

static void* TEST_VI_GetViFrameProc(void *pArgs) {
    TEST_VI_THREAD_S *pstCtx = reinterpret_cast<TEST_VI_THREAD_S *>(pArgs);
    VIDEO_FRAME_INFO_S frameOut = {0};
    RK_S32 s32Ret = 0;
    RK_S32 testLoopCount = 0;
    RK_S32 fileSize = pstCtx->stViCtx.width * pstCtx->stViCtx.height * 3 / 2;

    while (pstCtx->bThreadStart) {
        s32Ret = RK_MPI_VI_GetChnFrame(pstCtx->stViCtx.pipeId, pstCtx->stViCtx.channelId,
                                        &frameOut, TEST_VI_TIME_OUT_MS);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGW("RK_MPI_VI_GetChnFrame fail :%#X", s32Ret);
            continue;
        }
        RK_LOGI("vi get frame:%d length %lld", frameOut.stVFrame.u32TimeRef,
                RK_MPI_MB_GetLength(frameOut.stVFrame.pMbBlk));
        if (pstCtx->fp != RK_NULL) {
            fwrite(RK_MPI_MB_Handle2VirAddr(frameOut.stVFrame.pMbBlk),
                    1, fileSize, pstCtx->fp);
            fflush(pstCtx->fp);
        }
        s32Ret = RK_MPI_VI_ReleaseChnFrame(pstCtx->stViCtx.pipeId, pstCtx->stViCtx.channelId,
                                            &frameOut);
        if (s32Ret)
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail :%#X", s32Ret);
        testLoopCount++;
        if (pstCtx->stViCtx.saveFrameLoop > 0 && pstCtx->stViCtx.saveFrameLoop <= testLoopCount)
            pstCtx->bThreadStart = RK_FALSE;
    }

    return NULL;
}

static RK_S32 TEST_VI_StartGetStream(COMMON_TEST_VI_CTX_S *viCtx) {
    RK_S32 s32Ret = 0;

    if (viCtx->viSaveFileName != RK_NULL) {
        gGSThread[viCtx->pipeId][viCtx->channelId].fp =
            fopen(viCtx->viSaveFileName, "wb");
        if (gGSThread[viCtx->pipeId][viCtx->channelId].fp == RK_NULL) {
            RK_LOGE("can't open file %s!", viCtx->viSaveFileName);
            return RK_FAILURE;
        }
    }

    gGSThread[viCtx->pipeId][viCtx->channelId].bThreadStart = RK_TRUE;
    memcpy(&gGSThread[viCtx->pipeId][viCtx->channelId].stViCtx, viCtx,
            sizeof(COMMON_TEST_VI_CTX_S));

    s32Ret = pthread_create(&(gGSThread[viCtx->pipeId][viCtx->channelId].ViPid), 0,
                            TEST_VI_GetViFrameProc,
                            (RK_VOID *)&(gGSThread[viCtx->pipeId][viCtx->channelId]));

    return s32Ret;
}

static RK_S32 TEST_VI_StopGetStream(TEST_VI_THREAD_S *viCtx) {
    RK_S32 s32Ret = 0;
    if (RK_TRUE == viCtx->bThreadStart) {
        viCtx->bThreadStart = RK_FALSE;
        pthread_join(viCtx->ViPid, 0);
        if (viCtx->fp != RK_NULL) {
            fclose(viCtx->fp);
            viCtx->fp = RK_NULL;
        }
    }
    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
