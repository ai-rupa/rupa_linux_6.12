/*
 * Copyright (c) 2022 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "rkadk_muxer.h"
#include "rkadk_thumb_comm.h"
#include "linux_list.h"
#include "rkadk_log.h"
#include "rkadk_signal.h"
#include "rkadk_thread.h"
#include "rkadk_msg.h"
#include "rkadk_param.h"
#include "rkadk_thumb_comm.h"
#include "rkmuxer.h"
#include <sys/time.h>

//#define RKADK_MUXER_TEST

#ifdef FILE_CACHE
#include "file_msg.h"
#include "file_cache.h"
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]))
#endif // ARRAY_SIZE

/** Stream Count Check */
#define RKADK_CHECK_STREAM_CNT(cnt)                                            \
  do {                                                                         \
    if ((cnt > RKADK_MUXER_STREAM_MAX_CNT) || (cnt == 0)) {                    \
      RKADK_LOGE("stream count[%d] > RKADK_MUXER_STREAM_MAX_CNT or == 0",      \
                 cnt);                                                         \
      return -1;                                                               \
    }                                                                          \
  } while (0)

typedef struct {
  struct list_head mark;
  MB_BLK pMbBlk;
  RKADK_MUXER_RELEASE_CALLBACK pfnCellReleaseBuf;
  unsigned char *buf;
  RKADK_U32 size;
  int isKeyFrame;
  int64_t pts;
  bool bIsPool;
  struct list_head *pool;
  RKADK_U32 seq;
} MUXER_BUF_CELL_S;

typedef struct {
  bool bEnableSplit;
  bool bSplitRecord;
  RKADK_U32 u32SplitDurationSec;
} MANUAL_SPLIT_ATTR;

typedef struct {
  bool bGetThumb;
  bool bRequestThumb;

  //only for fs/flv
  pthread_mutex_t mutex;
  char thumbPath[RKADK_MAX_FILE_PATH_LEN];
  MB_BLK pMbBlk;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U8 *pData;
  RKADK_U32 u32Size;
  RKADK_MUXER_RELEASE_CALLBACK pfnReleaseMbBlk;
} MUXER_THUMB_PARAM;

typedef struct {
  struct list_head stVList;
  struct list_head stAList;
  RKADK_MUXER_PRE_RECORD_ATTR_S stAttr;
  pthread_mutex_t mutex;
} MUXER_PRE_RECORD_PARAM;

#ifdef ENABLE_AOV
#define RKADK_IPCMSG_KEY 1030

typedef struct {
  bool bIsSleep;
} AOV_PARAM_S;

typedef struct {
  int initCnt;
  int msgid;
  pthread_t tid;
  pthread_mutex_t mutex;
} AOV_HANDLE_S;

static AOV_HANDLE_S stAovHandle = {.initCnt = 0, .msgid = -1, .tid = 0};
#endif

#ifdef FILE_CACHE
typedef struct {
  bool bUsed;
  RKADK_MW_PTR ptr;
  int32_t realDuration;
  bool bSplitRecord;
  const char *cOutputFmt;
  bool bBuildInThm;
  char cFileName[RKADK_MAX_FILE_PATH_LEN];
  RKADK_MUXER_EVENT_CALLBACK_FN pfnEventCallback;
  MUXER_THUMB_PARAM stThumbParam;
} FILE_CACHE_HANDLE_S;
#endif

typedef struct {
  RKADK_MW_PTR ptr;
  RKADK_U32 u32CamId;
  RKADK_S32 s32ViChn; // vi channel id
  RKADK_S32 s32VencChn; // venc channel id
  RKADK_S32 s32ThumbVencChn; // thumb venc channel id
  bool bUseVpss;
  int muxerId;
  char cFileName[RKADK_MAX_FILE_PATH_LEN];
  const char *cOutputFmt;
  VideoParam stVideo;
  AudioParam stAudio;
  int32_t gop;
  int32_t duration;     // s
  int32_t realDuration; // ms
  int64_t startTime;    // us
  int64_t lapseTimeStamp;
  RKADK_U32 keyFrameCnt;
  RKADK_U32 frameCnt;
  bool bEnableStream;
  bool bReseting;
  bool bMuxering;
  bool bFirstFile;
  bool bFirstKeyFrame;
  bool bWriteFirstFrame;
  RKADK_U32 u32FirstSeq;
  bool bIOError;
  pthread_mutex_t paramMutex;
  RKADK_MUXER_REQUEST_FILE_NAME_CB pcbRequestFileNames;
  RKADK_MUXER_EVENT_CALLBACK_FN pfnEventCallback;
  RKADK_U32 u32GetThumbTime; //ms

  void *pThread;
  pthread_mutex_t mutex;
  void *pSignal;

  // list param
  MUXER_BUF_CELL_S stVCell[RKADK_MUXER_CELL_MAX_CNT]; // video list cache size
  MUXER_BUF_CELL_S stACell[RKADK_MUXER_CELL_MAX_CNT]; // audio list cache size
  struct list_head stVFree;     // video list remain size
  struct list_head stAFree;     // audio list remain size
  struct list_head stProcList;  // process list

  struct timeval checkWriteTime;

  MUXER_THUMB_PARAM stThumbParam;
  MANUAL_SPLIT_ATTR stManualSplit;
  MUXER_PRE_RECORD_PARAM stPreRecParam;

#ifdef ENABLE_AOV
  AOV_PARAM_S stAovParam;
#endif

#ifdef RKADK_MUXER_TEST
  RKADK_S64 S64DiffPts;
  RKADK_U64 u32PrePts;
  RKADK_U32 u32PreSeq;
  RKADK_U32 u32DiffSeq;
#endif
} MUXER_HANDLE_S;

#if defined(FILE_CACHE) || defined(ENABLE_AOV)
static RKADK_MW_PTR g_pRecorder[RKADK_MAX_SENSOR_CNT] = {NULL};
#endif

#ifdef FILE_CACHE
FILE_CACHE_HANDLE_S g_stFileCachehandle[RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT];
#endif

static MUXER_HANDLE_S *RKADK_MUXER_FindHandle(RKADK_MUXER_HANDLE_S *pstMuxer,
                                              RKADK_S32 s32MuxerId, RKADK_S32 s32VencChn) {
  bool bFindHandle = false;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (pstMuxerHandle) {
      if (s32VencChn != -1 && pstMuxerHandle->s32VencChn == s32VencChn) {
        bFindHandle = true;
        break;
      }

      if (s32MuxerId != -1 && pstMuxerHandle->muxerId == s32MuxerId) {
        bFindHandle = true;
        break;
      }
    }
  }

  if (!bFindHandle)
    pstMuxerHandle = NULL;

  return pstMuxerHandle;
}

#ifdef ENABLE_AOV
#if 0
static void *RKADK_MUXER_AovProc(void *arg) {
  int i, j, ret;
  RKADK_MSG stAovMsg;
  bool bIsSleep;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  AOV_HANDLE_S *pstAovHandle = &stAovHandle;

  prctl(PR_SET_NAME, "rkadk_aov_proc", 0, 0, 0);
  while(1) {
    bIsSleep = true;

    ret = RKADK_MSG_Recv(pstAovHandle->msgid, &stAovMsg);
    if (ret < 0) {
      RKADK_LOGE("rec msg failed");
      continue;
    }

    RKADK_LOGD("u32CamId: %d, u32VencChn: %d, cmd: %d", stAovMsg.u32CamId, stAovMsg.u32VencChn, stAovMsg.command);

    if (stAovMsg.command == RKADK_CMD_EXIT) {
      RKADK_LOGI("aov proc pthread will exit");
      break;
    }

    if (stAovMsg.u32CamId >= RKADK_MAX_SENSOR_CNT) {
      RKADK_LOGE("Invalid camId[%d]", stAovMsg.u32CamId);
      continue;
    }

    pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[stAovMsg.u32CamId];
    pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, -1, stAovMsg.u32VencChn);
    if (!pstMuxerHandle) {
      RKADK_LOGE("Not find pstMuxerHandle[%d, %d]", stAovMsg.u32CamId, stAovMsg.u32VencChn);
      continue;
    }

    if (pstMuxerHandle->bReseting && stAovMsg.command == RKADK_CMD_SLEEP) {
      RKADK_LOGI("reseting, don't sleep");
      continue;
    }

    if (stAovMsg.command == RKADK_CMD_SLEEP)
      pstMuxerHandle->stAovParam.bIsSleep = true;

    for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
      if (!g_pRecorder[i])
        continue;

      pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
      for (j = 0; j < (int)pstMuxer->u32StreamCnt; j++) {
        pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[j];
        if (pstMuxerHandle) {
          if (!pstMuxerHandle->stAovParam.bIsSleep)
            bIsSleep = false;
        }
      }
    }

    if (stAovMsg.command == RKADK_CMD_SLEEP) {
      if (bIsSleep) {
        RKADK_AOV_Notify(RKADK_AOV_ENTER_SLEEP, NULL);
        for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
          if (!g_pRecorder[i])
            continue;

          pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
          for (j = 0; j < (int)pstMuxer->u32StreamCnt; j++) {
            pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[j];
            if (pstMuxerHandle)
              pstMuxerHandle->stAovParam.bIsSleep = false;
          }
        }
      }
    }
  }

  return NULL;
}
#endif

static int RKADK_MUXER_AovInit(RKADK_AOV_ATTR_S *pstAovAttr) {
  int ret = 0;
  AOV_HANDLE_S *pstAovHandle = &stAovHandle;

  if (pstAovHandle->initCnt == 0) {
    if (pthread_mutex_init(&pstAovHandle->mutex, NULL) != 0) {
      RKADK_LOGE("msg lock init failed");
      return -1;
    }

#if 0
    ret = RKADK_MSG_Init(RKADK_IPCMSG_KEY, &pstAovHandle->msgid);
    if (ret != 0) {
      RKADK_LOGE("RKADK_MSG_Init failed");
      pthread_mutex_destroy(&pstAovHandle->mutex);
      return -1;
    }

    ret = pthread_create(&pstAovHandle->tid, NULL, RKADK_MUXER_AovProc, NULL);
    if (ret) {
      RKADK_LOGE("create aov proc thread failed[%d]", ret);
      pthread_mutex_destroy(&pstAovHandle->mutex);
      RKADK_MSG_Deinit(RKADK_IPCMSG_KEY, &pstAovHandle->msgid);
      return -1;
    }
#endif

    RKADK_LOGD("Aov init done");
  }

  pstAovHandle->initCnt++;
  return 0;
}

static int RKADK_MUXER_AovDeinit() {
  int ret;
  RKADK_MSG stAovMsg;
  AOV_HANDLE_S *pstAovHandle = &stAovHandle;

  if (pstAovHandle->initCnt == 0) {
    RKADK_LOGD("Aov has been deinit");
    return 0;
  }

  if (pstAovHandle->initCnt == 1) {
#if 0
    if (pstAovHandle->tid) {
      stAovMsg.u32CamId = 0;
      stAovMsg.u32VencChn = 0;
      stAovMsg.command = RKADK_CMD_EXIT;
      ret = RKADK_MSG_Send(pstAovHandle->msgid, &stAovMsg);
      if (ret)
        RKADK_LOGE("Send exit msg failed");

      ret = pthread_join(pstAovHandle->tid, NULL);
      if (ret)
        RKADK_LOGE("Exit aov proc thread failed!");
      else
        RKADK_LOGE("Exit aov proc thread ok");

      pstAovHandle->tid = 0;
    }

    RKADK_MSG_Deinit(RKADK_IPCMSG_KEY, &pstAovHandle->msgid);
#endif

    pthread_mutex_destroy(&pstAovHandle->mutex);
    RKADK_LOGD("Aov deinit done");
  }

  pstAovHandle->initCnt--;
  return 0;
}

static int RKADK_MUXER_EnterSleep(RKADK_MW_PTR pHandle, RKADK_S32 s32VencChnId) {
  int i, j;
  bool bAllSleep = false, bIsSleep = true;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_CAMERAID(pstMuxer->u32CamId, RKADK_FAILURE);
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  if (s32VencChnId == -1)
    bAllSleep = true;

  for (i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    if (bAllSleep)
      pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    else
      pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, -1, s32VencChnId);

    if (!pstMuxerHandle)
      continue;

    pstMuxerHandle->stAovParam.bIsSleep = true;
    RKADK_LOGD("set u32CamId: %d, s32VencChnId: %d bIsSleep", pstMuxer->u32CamId, pstMuxerHandle->s32VencChn);

    if (!bAllSleep)
      break;
  }

  for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
    if (!g_pRecorder[i])
      continue;

    pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
    for (j = 0; j < (int)pstMuxer->u32StreamCnt; j++) {
      pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[j];
      if (pstMuxerHandle) {
        if (!pstMuxerHandle->stAovParam.bIsSleep)
          bIsSleep = false;
      }
    }
  }

  if (bIsSleep) {
    RKADK_AOV_Notify(RKADK_AOV_ENTER_SLEEP, NULL);
    for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
      if (!g_pRecorder[i])
        continue;

      pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
      for (j = 0; j < (int)pstMuxer->u32StreamCnt; j++) {
        pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[j];
        if (pstMuxerHandle)
          pstMuxerHandle->stAovParam.bIsSleep = false;
      }
    }
  }

  return 0;
}

static void RKADK_MUXER_AovDropFrame(RKADK_MUXER_HANDLE_S *pstMuxer) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  VENC_STREAM_S stFrame;
  VENC_PACK_S stPack;
  RKADK_U32 u32LoopCount = 0;
  int ret;

  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle)
      continue;

    u32LoopCount = 0;
    while (1) {
      ret = RK_MPI_VENC_GetStream(pstMuxerHandle->s32VencChn, &stFrame, 1000);
      if (ret == RK_SUCCESS) {
        RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32VencChn, &stFrame);
        u32LoopCount++;
      } else {
        RKADK_LOGD("CamId[%d] venc[%d] mode[%d, %d], drop frame[%d]!!!", pstMuxer->u32CamId,
                    pstMuxerHandle->s32VencChn, pstMuxer->enFrameMode, pstMuxer->enRecType, u32LoopCount);
        break;
      }
    }

    RK_MPI_VENC_RequestIDR(pstMuxerHandle->s32VencChn, RK_FALSE);
  }
}

static bool RKADK_MUXER_IsEnableAiisp() {
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  for (int i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
    if (g_pRecorder[i]) {
      pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
      if (pstMuxer->bEnableAiisp)
        return true;
    }
  }
  return false;
}

static int RKADK_MUXER_AovSwichMode(RKADK_MUXER_HANDLE_S *pstMuxer) {
  if (pstMuxer->enFrameMode == MULTI_FRAME_MODE && pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
    if (!pstMuxer->stAovAttr.pfnSingleFrame) {
      RKADK_LOGE("CamId[%d] unregistered pfnSingleFrame", pstMuxer->u32CamId);
      return -1;
    }

    pstMuxer->stAovAttr.pfnSingleFrame(pstMuxer->u32CamId);
    if (!RKADK_MUXER_IsEnableAiisp())
      RKADK_AOV_DisableNonBootCPUs();
    RKADK_MUXER_AovDropFrame(pstMuxer);
    pstMuxer->enFrameMode = SINGLE_FRAME_MODE;
    RKADK_MUXER_EnterSleep((RKADK_MW_PTR)pstMuxer, -1);
    return 0;
  } else if (pstMuxer->enFrameMode == SINGLE_FRAME_MODE && pstMuxer->enRecType != RKADK_REC_TYPE_AOV_LAPSE) {
    if (!pstMuxer->stAovAttr.pfnMultiFrame) {
      RKADK_LOGE("CamId[%d] unregistered pfnMultiFrame", pstMuxer->u32CamId);
      return -1;
    }

    RKADK_MUXER_AovDropFrame(pstMuxer);
    pstMuxer->stAovAttr.pfnMultiFrame(pstMuxer->u32CamId);
    RKADK_AOV_EnableNonBootCPUs();
    pstMuxer->enFrameMode = MULTI_FRAME_MODE;
    return 0;
  }

  return -1;
}
#endif

static void RKADK_MUXER_ListInit(MUXER_HANDLE_S *pstMuxerHandle) {
  INIT_LIST_HEAD(&pstMuxerHandle->stVFree);
  INIT_LIST_HEAD(&pstMuxerHandle->stAFree);
  INIT_LIST_HEAD(&pstMuxerHandle->stProcList);
  INIT_LIST_HEAD(&pstMuxerHandle->stPreRecParam.stAList);
  INIT_LIST_HEAD(&pstMuxerHandle->stPreRecParam.stVList);

  for (unsigned int i = 0; i < ARRAY_SIZE(pstMuxerHandle->stVCell); i++) {
    memset(&pstMuxerHandle->stVCell[i], 0, sizeof(MUXER_BUF_CELL_S));
    INIT_LIST_HEAD(&pstMuxerHandle->stVCell[i].mark);
    pstMuxerHandle->stVCell[i].pool = &pstMuxerHandle->stVFree;
    list_add_tail(&pstMuxerHandle->stVCell[i].mark, &pstMuxerHandle->stVFree);
  }

  for (unsigned int i = 0; i < ARRAY_SIZE(pstMuxerHandle->stACell); i++) {
    memset(&pstMuxerHandle->stACell[i], 0, sizeof(MUXER_BUF_CELL_S));
    INIT_LIST_HEAD(&pstMuxerHandle->stACell[i].mark);
    pstMuxerHandle->stACell[i].pool = &pstMuxerHandle->stAFree;
    list_add_tail(&pstMuxerHandle->stACell[i].mark, &pstMuxerHandle->stAFree);
  }
}

static void RKADK_MUXER_CellFree(MUXER_HANDLE_S *pstMuxerHandle,
                                 MUXER_BUF_CELL_S *cell) {
  bool bIsPool = cell->bIsPool;
  struct list_head *pool = cell->pool;

  list_del_init(&cell->mark);
  if (cell->buf)
    cell->buf = NULL;

  if (cell->pfnCellReleaseBuf) {
    if (cell->pMbBlk)
      cell->pfnCellReleaseBuf(cell->pMbBlk);
  }

  if (bIsPool) {
    memset(cell, 0, sizeof(MUXER_BUF_CELL_S));
    INIT_LIST_HEAD(&cell->mark);
    cell->pool = pool;
    RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
    list_add_tail(&cell->mark, cell->pool);
    RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  } else {
    free(cell);
    cell = NULL;
  }
}

static int RKADK_MUXER_CellReleaseBuf(MB_BLK pMbBlk) {
  int ret;

  if (pMbBlk == NULL)
    return -1;

  ret = RK_MPI_MB_ReleaseMB(pMbBlk);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);
    return -1;
  }

  return 0;
}

static void RKADK_MUXER_ListRelease(MUXER_HANDLE_S *pstMuxerHandle,
                                    struct list_head *head) {
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  list_for_each_entry_safe(cell, cell_n, head, mark) {
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
  }
}

static MUXER_BUF_CELL_S *RKADK_MUXER_CellGet(MUXER_HANDLE_S *pstMuxerHandle,
                                             struct list_head *head) {
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  list_for_each_entry_safe(cell, cell_n, head, mark) {
    list_del_init(&cell->mark);
    RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
    return cell;
  }
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  return NULL;
}

static int RKADK_MUXER_GetListSize(struct list_head *head) {
  int size = 0;
  MUXER_BUF_CELL_S *cell = NULL;

  list_for_each_entry(cell, head, mark) {
    size++;
  }

  return size;
}

static void RKADK_MUXER_CellPush(MUXER_HANDLE_S *pstMuxerHandle,
                                 struct list_head *head,
                                 MUXER_BUF_CELL_S *one) {
  int ret = -1;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, head, mark) {
      if ((cell->pts > one->pts) && (cell->pool == one->pool)) {
        list_add_tail(&one->mark, &cell->mark);
        ret = 0;
        break;
      }
    }
    if (ret) {
      list_add_tail(&one->mark, head);
    }
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
}

static MUXER_BUF_CELL_S *RKADK_MUXER_CellPop(MUXER_HANDLE_S *pstMuxerHandle,
                                             struct list_head *head) {
  MUXER_BUF_CELL_S *rst = NULL;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, head, mark) {
      list_del_init(&cell->mark);
      rst = cell;
      break;
    }
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  return rst;
}

void RKADK_MUXER_ListDropPFrame(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *rst = NULL;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stProcList, mark) {
    if (cell->isKeyFrame == 0 && cell->pool == &pstMuxerHandle->stVFree) {
      rst = cell;
    }
  }
  if (rst) {
    list_del_init(&rst->mark);
  }
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);

  if (rst)
    RKADK_MUXER_CellFree(pstMuxerHandle, rst);
  else
    RKADK_LOGE("drop pframe fail");
}

static void RKADK_MUXER_PreRecPush(MUXER_HANDLE_S *pstMuxerHandle,
                                struct list_head *pstList, MUXER_BUF_CELL_S *one) {
  int ret = -1;
  int64_t cacheDuration = 0;
  MUXER_BUF_CELL_S *cell = NULL, *cell_n = NULL;
  MUXER_BUF_CELL_S *pstPreRecCell = NULL;
  MUXER_PRE_RECORD_PARAM *pstPreRecParam;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  //lapse record unsupport prerecord
  if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
    RKADK_MUTEX_LOCK(pstMuxerHandle->stPreRecParam.mutex);
    if (RKADK_MUXER_GetListSize(pstList) > 0)
      RKADK_MUXER_ListRelease(pstMuxerHandle, pstList);
    RKADK_MUTEX_UNLOCK(pstMuxerHandle->stPreRecParam.mutex);

    return;
  }

  pstPreRecParam = &pstMuxerHandle->stPreRecParam;
  if (pstPreRecParam->stAttr.u32PreRecTimeSec <= 0)
      return;

  pstPreRecCell = (MUXER_BUF_CELL_S *)malloc(sizeof(MUXER_BUF_CELL_S));
  if (NULL == pstPreRecCell) {
    RKADK_LOGE("Muxer[%d] venc[%d]: malloc pre_record cell failed", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn);
    return;
  }
  memset(pstPreRecCell, 0, sizeof(MUXER_BUF_CELL_S));

  INIT_LIST_HEAD(&pstPreRecCell->mark);
  pstPreRecCell->buf = one->buf;
  pstPreRecCell->isKeyFrame = one->isKeyFrame;
  pstPreRecCell->pts = one->pts;
  pstPreRecCell->size = one->size;
  pstPreRecCell->bIsPool = false;
  pstPreRecCell->pool = one->pool;
  pstPreRecCell->pMbBlk = one->pMbBlk;
  pstPreRecCell->seq = one->seq;
  pstPreRecCell->pfnCellReleaseBuf = RKADK_MUXER_CellReleaseBuf;
  RK_MPI_MB_AddUserCnt(one->pMbBlk);

  RKADK_MUTEX_LOCK(pstMuxerHandle->stPreRecParam.mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, pstList, mark) {
      if (cell->pts > pstPreRecCell->pts) {
        list_add_tail(&pstPreRecCell->mark, &cell->mark);
        ret = 0;
        break;
      }
    }

    if (ret) {
      list_add_tail(&pstPreRecCell->mark, pstList);
    }
  } while (0);

  if (!list_empty(pstList)) {
    cell = list_first_entry(pstList, MUXER_BUF_CELL_S, mark);
    if (cell) {
      cell_n = list_entry(pstList->prev, MUXER_BUF_CELL_S, mark);
      if (cell_n)
        cacheDuration = cell_n->pts - cell->pts;

      if (cacheDuration >= pstPreRecParam->stAttr.u32PreRecCacheTime * 1000000)
        RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    }
  }

  RKADK_MUTEX_UNLOCK(pstMuxerHandle->stPreRecParam.mutex);
}

void RKADK_MUXER_ProcessEvent(MUXER_HANDLE_S *pstMuxerHandle,
                              RKADK_MUXER_EVENT_E enEventType, int64_t value) {
  RKADK_MUXER_EVENT_INFO_S stEventInfo;

  if (!pstMuxerHandle->pfnEventCallback) {
    RKADK_LOGD("Unregistered event callback");
    return;
  }

  memset(&stEventInfo, 0, sizeof(RKADK_MUXER_EVENT_INFO_S));
  stEventInfo.enEvent = enEventType;
  switch (enEventType) {
    case RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL:
    case RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL:
    case RKADK_MUXER_EVENT_ERR_CARD_NONEXIST:
      stEventInfo.unEventInfo.stErrorInfo.s32ErrorCode = value;
      strncpy(stEventInfo.unEventInfo.stErrorInfo.asFileName,
              pstMuxerHandle->cFileName, strlen(pstMuxerHandle->cFileName));
      break;
    case RKADK_MUXER_EVENT_REQUEST_THUMB:
    case RKADK_MUXER_EVENT_BUILD_IN_THUMB:
      stEventInfo.unEventInfo.stThumbInfo.s32MuxerId = pstMuxerHandle->muxerId;
      strncpy(stEventInfo.unEventInfo.stThumbInfo.asFileName, pstMuxerHandle->cFileName, strlen(pstMuxerHandle->cFileName));
      break;
    default:
      stEventInfo.unEventInfo.stFileInfo.u32Duration = value;
      strncpy(stEventInfo.unEventInfo.stFileInfo.asFileName, pstMuxerHandle->cFileName, strlen(pstMuxerHandle->cFileName));
      break;
  }

  pstMuxerHandle->pfnEventCallback(pstMuxerHandle->ptr, &stEventInfo);
}

static void RKADK_MUXER_CheckWriteSpeed(MUXER_HANDLE_S *pstMuxerHandle) {
  int size = 0;
  struct timeval curTime;
  float diff = 0;
  long diffMs = 0;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (pstMuxer->enableFileCache)
    return;

  gettimeofday(&curTime, NULL);
  size = RKADK_MUXER_GetListSize(&pstMuxerHandle->stVFree);
  if(size <= 5) {
    if (pstMuxerHandle->checkWriteTime.tv_sec == 0 && pstMuxerHandle->checkWriteTime.tv_usec == 0) {
      pstMuxerHandle->checkWriteTime.tv_sec = curTime.tv_sec;
      pstMuxerHandle->checkWriteTime.tv_usec = curTime.tv_usec;
    } else {
      diff = (curTime.tv_sec - pstMuxerHandle->checkWriteTime.tv_sec) * 1000000
              + (curTime.tv_usec - pstMuxerHandle->checkWriteTime.tv_usec);
      diffMs = diff / 1000;
      if (diffMs >= 1000) {
        pstMuxerHandle->checkWriteTime.tv_sec = 0;
        pstMuxerHandle->checkWriteTime.tv_usec = 0;
        RKADK_LOGW("Muxer[%d] venc[%d] write file slow", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn);
        RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_WRITING_SLOW, 0);
      }
    }
  } else {
    pstMuxerHandle->checkWriteTime.tv_sec = 0;
    pstMuxerHandle->checkWriteTime.tv_usec = 0;
  }
}

int RKADK_MUXER_WriteVideoFrame(RKADK_MUXER_DATA_S stData, void *handle) {
  int cnt = 0;
  MUXER_BUF_CELL_S cell;
  MUXER_BUF_CELL_S *pstCell;
  RKADK_U32 framerate = 0;
  RKADK_U64 u64Pts = 0;
  RKADK_ISP_FRAME_MODE enFrameMode = MULTI_FRAME_MODE;

  RKADK_CHECK_POINTER(handle, RKADK_FAILURE);

  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)handle;
  enFrameMode = pstMuxer->enFrameMode;

  MUXER_HANDLE_S *pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, stData.s32MuxerId, stData.s32VencChn);
  if (!pstMuxerHandle) {
    RKADK_LOGE("muxer[%d] venc[%d] not find pstMuxerHandle", stData.s32MuxerId, stData.s32VencChn);
    return -1;
  }

  if (pstMuxerHandle->bReseting)
    return 0;

#ifdef ENABLE_AOV
  RKADK_MUTEX_LOCK(stAovHandle.mutex);
  if (!RKADK_MUXER_AovSwichMode(pstMuxer)) {
    RKADK_MUTEX_UNLOCK(stAovHandle.mutex);
    return 0;
  }
  RKADK_MUTEX_UNLOCK(stAovHandle.mutex);

  if ((enFrameMode == MULTI_FRAME_MODE && pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
      || ((enFrameMode == SINGLE_FRAME_MODE && pstMuxer->enRecType != RKADK_REC_TYPE_AOV_LAPSE))) {
    RKADK_LOGI("CamId[%d] muxer[%d] venc[%d] mode[%d, %d] no match, drop frame", pstMuxer->u32CamId,
                pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, enFrameMode, pstMuxer->enRecType);
    return 0;
  }
#endif

  u64Pts = stData.u64Pts;
  if (pstMuxerHandle->bFirstKeyFrame && stData.isKeyFrame) {
    RKADK_KLOG("Muxer[%d] venc[%d] first key frame pts: %lld",
               pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, stData.u64Pts);
    pstMuxerHandle->bFirstKeyFrame = false;

    if (u64Pts == 0)
      u64Pts = 1;
  }

  cell.buf = (unsigned char *)RK_MPI_MB_Handle2VirAddr(stData.pMbBlk);
  if (NULL == cell.buf) {
    RKADK_LOGE("Muxer[%d] venc[%d]: RK_MPI_MB_Handle2VirAddr failed", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn);
    return -1;
  }

  cell.isKeyFrame = stData.isKeyFrame;
  cell.pts = u64Pts;
  cell.size = stData.u32Len;
  cell.bIsPool = true;
  cell.pool = &pstMuxerHandle->stVFree;
  cell.pMbBlk = stData.pMbBlk;
  cell.seq = stData.u32Seq;
  cell.pfnCellReleaseBuf = RKADK_MUXER_CellReleaseBuf;
  RKADK_MUXER_PreRecPush(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stVList, &cell);

  if (!pstMuxerHandle->bEnableStream)
    return 0;

#ifdef RKADK_MUXER_TEST
  if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
#if defined(RV1106_1103) || defined(RV1103B) || defined(RV1126B)|| defined(RK3572)
    system("cat /proc/rkisp-vir0 | grep frame");

#if defined(RV1126B)|| defined(RK3572)
    system("cat /proc/rkvpss-vir0");
#endif
#else
    system("cat /proc/rkispp0 | grep frame");
#endif
    printf("\n\n");
  }

  pstMuxerHandle->S64DiffPts = stData.u64Pts - pstMuxerHandle->u32PrePts;
  pstMuxerHandle->u32PrePts = stData.u64Pts;
  pstMuxerHandle->u32DiffSeq = cell.seq - pstMuxerHandle->u32PreSeq;
  pstMuxerHandle->u32PreSeq = cell.seq;
  printf("Muxer[%d]: pts: %lld, S64DiffPts: %lld, seq: %d, u32DiffSeq: %d\n", pstMuxerHandle->muxerId, stData.u64Pts, pstMuxerHandle->S64DiffPts, cell.seq, pstMuxerHandle->u32DiffSeq);
#endif

  RKADK_MUXER_CheckWriteSpeed(pstMuxerHandle);
  framerate = pstMuxerHandle->stVideo.frame_rate_num / pstMuxerHandle->stVideo.frame_rate_den;
  while ((pstCell = RKADK_MUXER_CellGet(pstMuxerHandle, &pstMuxerHandle->stVFree)) == NULL) {
      if (cnt % framerate == 0)
        RKADK_LOGW("Muxer[%d] venc[%d]: get video cell fail, retry, cnt = %d", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, cnt);

      cnt++;
      RKADK_MUXER_CheckWriteSpeed(pstMuxerHandle);
      usleep(10000);
  }

  if (cnt % framerate != 0)
    RKADK_LOGW("Muxer[%d] venc[%d]: get video cell fail, retry, cnt = %d", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, cnt);

  pstCell->buf = cell.buf;
  pstCell->isKeyFrame = cell.isKeyFrame;
  pstCell->pts = cell.pts;
  pstCell->size = cell.size;
  pstCell->bIsPool = true;
  pstCell->pMbBlk = cell.pMbBlk;
  pstCell->seq = cell.seq;
  pstCell->pfnCellReleaseBuf = RKADK_MUXER_CellReleaseBuf;
  RK_MPI_MB_AddUserCnt(stData.pMbBlk);

  RKADK_MUXER_CellPush(pstMuxerHandle, &pstMuxerHandle->stProcList, pstCell);
  RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);
  return 0;
}

int RKADK_MUXER_WriteAudioFrame(RKADK_MUXER_DATA_S stData, void *handle) {
  int cnt = 0;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  int headerSize = 0; // aenc header size
  MUXER_BUF_CELL_S cell;
  MUXER_BUF_CELL_S *pstCell;
  bool bRawData = true;

  RKADK_CHECK_POINTER(handle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)handle;
  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    bRawData = true;

    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle)
      continue;

    if (pstMuxerHandle->bReseting)
      continue;

    headerSize = 0;
    unsigned char *base = (unsigned char *)RK_MPI_MB_Handle2VirAddr(stData.pMbBlk);
    if (NULL == base) {
      RKADK_LOGE("Muxer[%d]: RK_MPI_MB_Handle2VirAddr failed", pstMuxerHandle->muxerId);
      return -1;
    }

#ifdef RKMUXER_MEDIA_SERVER
    //media-server flv/ts support header audio data
    if (strcmp(pstMuxerHandle->cOutputFmt, "mp4")) {
      bRawData = false;
    }
#endif

    if (!strcmp(pstMuxerHandle->stAudio.codec, "ACC") && bRawData) {
      if (stData.u32Len >= 2 && base[0] == 0xFF && ((base[1] & 0xF0) == 0xF0)) {
        headerSize = (base[1] & 0x01) ? 7 : 9;
        if (stData.u32Len < (RKADK_U32)headerSize)
          headerSize = 0;
      }
    }
    cell.buf = base + headerSize;

    cell.size = stData.u32Len - headerSize;
    cell.isKeyFrame = 0;
    cell.pts = stData.u64Pts;
    cell.bIsPool = true;
    cell.pool = &pstMuxerHandle->stAFree;
    cell.pMbBlk = stData.pMbBlk;
    cell.pfnCellReleaseBuf = RKADK_MUXER_CellReleaseBuf;
    RKADK_MUXER_PreRecPush(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stAList, &cell);

    if (!pstMuxerHandle->bEnableStream)
      continue;

    while ((pstCell = RKADK_MUXER_CellGet(pstMuxerHandle, &pstMuxerHandle->stAFree)) == NULL) {
      if (cnt % 100 == 0) {
        RKADK_LOGW("Muxer[%d] get audio cell fail, retry, cnt = %d",pstMuxerHandle->muxerId, cnt);
        if (!pstMuxer->enableFileCache && cnt != 0)
          RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_WRITING_SLOW, 0);
      }
      cnt++;
      usleep(10000);
    }

    pstCell->buf = cell.buf;
    pstCell->size = cell.size;
    pstCell->isKeyFrame = 0;
    pstCell->pts = cell.pts;
    pstCell->bIsPool = true;
    pstCell->pMbBlk = cell.pMbBlk;
    pstCell->pfnCellReleaseBuf = RKADK_MUXER_CellReleaseBuf;
    RK_MPI_MB_AddUserCnt(pstCell->pMbBlk);
    RKADK_MUXER_CellPush(pstMuxerHandle, &pstMuxerHandle->stProcList, pstCell);
    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);
  }

  return 0;
}

static bool RKADK_MUXER_GetThumbOther(MUXER_HANDLE_S *pstMuxerHandle) {
  int ret;
  VENC_PACK_S stPack;
  VENC_STREAM_S stFrame;
  bool bSaveThumb = false;
  MUXER_THUMB_PARAM *pstThumbParam = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (pstMuxerHandle->s32ThumbVencChn < 0)
    return false;

  if (pstMuxer->enableFileCache) {
#ifdef FILE_CACHE
    pstThumbParam = &g_stFileCachehandle[pstMuxerHandle->muxerId].stThumbParam;
#endif
  } else {
    pstThumbParam = &pstMuxerHandle->stThumbParam;
  }

  if (!pstThumbParam) {
    RKADK_LOGE("pstThumbParam is null");
    return true;
  }

  if (pstMuxer->enableFileCache) {
    RKADK_MUTEX_LOCK(pstThumbParam->mutex);
    if (pstThumbParam->pData || pstThumbParam->pMbBlk) {
      RKADK_LOGD("thumb pMbBlk not null, waiting for previous file build-in thumbnail");
      RKADK_MUTEX_UNLOCK(pstThumbParam->mutex);
      return true;
    }
    RKADK_MUTEX_UNLOCK(pstThumbParam->mutex);
  }

  if (strlen(pstMuxer->cThumbDir) > 0) {
    if (access(pstMuxer->cThumbDir, F_OK)) {
      if (RKADK_PARAM_Mkdirs(pstMuxer->cThumbDir)) {
        RKADK_LOGE("create %s failed", pstMuxer->cThumbDir);
        return true;
      }

      bSaveThumb = true;
    } else {
      bSaveThumb = true;
    }
  }

  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;
  ret = RK_MPI_VENC_GetStream(pstMuxerHandle->s32ThumbVencChn, &stFrame, 1);
  if (ret == RK_SUCCESS) {
    RKADK_U8 *pData = (RKADK_U8 *)RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);

    RKADK_MUTEX_LOCK(pstThumbParam->mutex);
    if (pstMuxer->bBuildInThm) {
      pstThumbParam->pData = pData;
      pstThumbParam->u32Size = stFrame.pstPack->u32Len;
      pstThumbParam->pMbBlk = stFrame.pstPack->pMbBlk;
      pstThumbParam->pfnReleaseMbBlk = RKADK_MUXER_CellReleaseBuf;
      RK_MPI_MB_AddUserCnt(stFrame.pstPack->pMbBlk);
    }

    if (bSaveThumb) {
      ret = RKADK_THUMB_GetFilePath(pstMuxerHandle->cFileName, pstMuxer->cThumbDir,
                                    pstThumbParam->thumbPath, RKADK_MAX_FILE_PATH_LEN);
      if (ret) {
        RKADK_LOGE("cFileName[%s] get thumb path faild", pstMuxerHandle->cFileName);
        bSaveThumb = false;
      }
    }
    RKADK_MUTEX_UNLOCK(pstThumbParam->mutex);

    if (bSaveThumb) {
#ifdef ENABLE_AOV
    if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
      RKADK_AOV_WakeupLock();
      if (pstMuxer->pfnMountSdcard) {
        if (pstMuxer->pfnMountSdcard())
          RKADK_LOGE("sdcard mount failed");
      } else {
          RKADK_LOGD("Unregistered pfnMountSdcard");
      }
    }
#endif

      FILE *fp = fopen(pstThumbParam->thumbPath, "w");
      if (!fp) {
#ifdef ENABLE_AOV
        if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
          RKADK_AOV_WakeupUnlock();
#endif
        RKADK_LOGE("Open %s file failed, errno = %d", pstThumbParam->thumbPath, errno);
        RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32ThumbVencChn, &stFrame);
        return false;
      }

      fwrite(pData, 1, stFrame.pstPack->u32Len, fp);
      fclose(fp);
      RKADK_LOGI("Muxer[%d] thumbnail[seq:%d, len:%d] save file[%s] done!",
                  pstMuxerHandle->muxerId, stFrame.u32Seq, stFrame.pstPack->u32Len, pstThumbParam->thumbPath);

#ifdef ENABLE_AOV
      if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
        RKADK_AOV_WakeupUnlock();
#endif
    }

    ret = RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32ThumbVencChn, &stFrame);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", ret);
    }

    RK_MPI_VENC_StopRecvFrame(pstMuxerHandle->s32ThumbVencChn);
    RK_MPI_VENC_ResetChn(pstMuxerHandle->s32ThumbVencChn);
    return false;
  } else {
    RKADK_LOGD("Muxer[%d] get thumb failed[%x]", pstMuxerHandle->muxerId, ret);
    return true;
  }
}

static bool RKADK_MUXER_GetThumbMp4(MUXER_HANDLE_S *pstMuxerHandle) {
  int ret;
  int position = 0;
  VENC_PACK_S stPack;
  VENC_STREAM_S stFrame;
  bool bGetThumb = false;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (pstMuxerHandle->s32ThumbVencChn < 0)
    return false;

  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;

  if (pstMuxer->stThirdMuxerAttr.pfnGetThumbPos)
    position = pstMuxer->stThirdMuxerAttr.pfnGetThumbPos(pstMuxerHandle->muxerId);
  else
    position = rkmuxer_get_thumb_pos(pstMuxerHandle->muxerId);
  if (pstMuxerHandle->bIOError || position > 0)
    bGetThumb = true;

  if (bGetThumb) {
    ret = RK_MPI_VENC_GetStream(pstMuxerHandle->s32ThumbVencChn, &stFrame, 1);
    if (ret == RK_SUCCESS) {
      RKADK_CHAR *pData = (RKADK_CHAR *)RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);

#ifdef ENABLE_AOV
      if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
        RKADK_AOV_WakeupLock();
        if (pstMuxer->pfnMountSdcard) {
          if (pstMuxer->pfnMountSdcard())
            RKADK_LOGE("sdcard mount failed");
        } else {
            RKADK_LOGD("Unregistered pfnMountSdcard");
        }
      }
#endif

      FILE *fp = fopen(pstMuxerHandle->cFileName, "r+");
      if (!fp) {
#ifdef ENABLE_AOV
        if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
          RKADK_AOV_WakeupUnlock();
#endif
        RKADK_LOGE("Open %s file failed, error[%d, %s]", pstMuxerHandle->cFileName, errno, strerror(errno));
        RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32ThumbVencChn, &stFrame);
        return false;
      }

      if (!fseek(fp, position, SEEK_SET)) {
        fwrite(pData, 1, stFrame.pstPack->u32Len, fp);
        RKADK_LOGI("Muxer[%d] thumbnail[seq:%d, len:%d] build in file[%s] pos[%d] done!",
                    pstMuxerHandle->muxerId, stFrame.u32Seq, stFrame.pstPack->u32Len,
                    pstMuxerHandle->cFileName, position);
      } else {
        RKADK_LOGE("Seek error[%d, %s]", errno, strerror(errno));
      }

      fclose(fp);

#ifdef ENABLE_AOV
      if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
        RKADK_AOV_WakeupUnlock();
#endif

      ret = RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32ThumbVencChn, &stFrame);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", ret);
      }

      RK_MPI_VENC_StopRecvFrame(pstMuxerHandle->s32ThumbVencChn);
      RK_MPI_VENC_ResetChn(pstMuxerHandle->s32ThumbVencChn);
      return false;
    } else {
      RKADK_LOGD("Muxer[%d] get thumb failed[%x]", pstMuxerHandle->muxerId, ret);
      return true;
    }
  } else {
    RKADK_LOGW("Muxer[%d] position[%d] invalid value!", pstMuxerHandle->muxerId, position);
    return true;
  }
}

static bool RKADK_MUXER_GetThumb(MUXER_HANDLE_S *pstMuxerHandle) {
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (pstMuxer->enMuxerType == RKADK_MUXER_TYPE_THIRD_STREAM) {
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_BUILD_IN_THUMB, 0);
    return false;
  }

  if (!strcmp(pstMuxerHandle->cOutputFmt, "mp4"))
    return RKADK_MUXER_GetThumbMp4(pstMuxerHandle);
  else
    return RKADK_MUXER_GetThumbOther(pstMuxerHandle);
}

// only for ts/flv
static int RKADK_MUXER_SaveThumb(MUXER_THUMB_PARAM *pstThumbParam, char *pFileName) {
  int ret = -1;
  FILE *fd = NULL;
  RKADK_THUMB_ATTR_S stThumbAttr;

  RKADK_MUTEX_LOCK(pstThumbParam->mutex);
  if (!pstThumbParam->pData || !pstThumbParam->pMbBlk) {
    RKADK_LOGE("pFileName[%s] invalid thumb data", pFileName);
    goto exit;
  }

  fd = fopen(pFileName, "r+");
  if (!fd) {
    RKADK_LOGE("Open %s failed, error[%d, %s]", pFileName, errno, strerror(errno));
    goto exit;
  }

  memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stThumbAttr.u32Width = pstThumbParam->u32Width;
  stThumbAttr.u32Height = pstThumbParam->u32Height;
  stThumbAttr.u32VirWidth = pstThumbParam->u32Width;
  stThumbAttr.u32VirHeight = pstThumbParam->u32Height;
  stThumbAttr.pu8Buf = pstThumbParam->pData;
  stThumbAttr.u32BufSize = pstThumbParam->u32Size;
  ret = BuildInThmToFile(fd, &stThumbAttr);
  if (ret) {
    RKADK_LOGE("pFileName[%s] build-in thumb failed", pFileName);
  } else {
    RKADK_LOGI("pFileName[%s] build-in thumb done!", pFileName);
    ret = 0;
  }

exit:
  if (pstThumbParam->pfnReleaseMbBlk) {
    pstThumbParam->pfnReleaseMbBlk(pstThumbParam->pMbBlk);
  } else {
    RKADK_LOGE("pFileName[%s] unregistered release thumb function", pFileName);
  }

  pstThumbParam->pData = NULL;
  pstThumbParam->pMbBlk = NULL;

  if (fd) {
    fflush(fd);
    fclose(fd);
  }

  RKADK_MUTEX_UNLOCK(pstThumbParam->mutex);

  // if build-in done, remove thumb file
  if (!ret && strlen(pstThumbParam->thumbPath) > 0) {
    if (remove(pstThumbParam->thumbPath) == 0)
      RKADK_LOGD("remove thumb file[%s] done", pstThumbParam->thumbPath);
    else
      RKADK_LOGE("remove thumb file[%s] fail, errno[%d, %s]", pstThumbParam->thumbPath, errno, strerror(errno));
  }

  return ret;
}

static void RKADK_MUXER_ForceRequestThumb(MUXER_HANDLE_S *pstMuxerHandle) {
  int ret;
  VENC_PACK_S stPack;
  VENC_STREAM_S stFrame;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (!pstMuxer)
    return;

  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;

  if (pstMuxer->enMuxerType == RKADK_MUXER_TYPE_THIRD_STREAM) {
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_REQUEST_THUMB, 0);
  } else {
#ifndef THUMB_NORMAL
    RK_MPI_VENC_ThumbnailRequest(pstMuxerHandle->s32VencChn);
#else
    ret = RK_MPI_VENC_GetStream(pstMuxerHandle->s32ThumbVencChn, &stFrame, 1);
    if (ret == RK_SUCCESS) {
      ret = RK_MPI_VENC_ReleaseStream(pstMuxerHandle->s32ThumbVencChn, &stFrame);
      if (ret != RK_SUCCESS)
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);
    }

    RK_MPI_VENC_StopRecvFrame(pstMuxerHandle->s32ThumbVencChn);
    RK_MPI_VENC_ResetChn(pstMuxerHandle->s32ThumbVencChn);
    ThumbnailRequest(pstMuxerHandle->s32ThumbVencChn);
    RKADK_LOGI("muxerId[%d] request venc[%d] thumbnail", pstMuxerHandle->muxerId, pstMuxerHandle->s32ThumbVencChn);
#endif
  }

  pstMuxerHandle->stThumbParam.bRequestThumb = false;
}

static void RKADK_MUXER_Close(MUXER_HANDLE_S *pstMuxerHandle) {
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (!pstMuxerHandle->bMuxering)
    return;

  RKADK_LOGI("File end: muxer[%d] venc[%d], duration[%d], frameCnt[%d], keyFrameCnt[%d]",
      pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, pstMuxerHandle->realDuration,
      pstMuxerHandle->frameCnt, pstMuxerHandle->keyFrameCnt);

  if (pstMuxerHandle->realDuration <= 0) {
    pstMuxerHandle->realDuration =
      pstMuxerHandle->frameCnt * (1000 / pstMuxerHandle->stVideo.frame_rate_num);
    RKADK_LOGI("The revised Duration = %d, frameCnt = %d",
      pstMuxerHandle->realDuration, pstMuxerHandle->frameCnt);
  }

#ifdef FILE_CACHE
  if (pstMuxer->enableFileCache) {
    FILE_CACHE_HANDLE_S *pstFileCachehandle = &g_stFileCachehandle[pstMuxerHandle->muxerId];
    pstFileCachehandle->bSplitRecord = pstMuxerHandle->stManualSplit.bSplitRecord;
    pstFileCachehandle->realDuration = pstMuxerHandle->realDuration;
    strncpy(pstFileCachehandle->cFileName, pstMuxerHandle->cFileName, RKADK_MAX_FILE_PATH_LEN);
  }
#endif

  // Stop muxer
  if (pstMuxer->stThirdMuxerAttr.pfnMuxerDeinit)
    pstMuxer->stThirdMuxerAttr.pfnMuxerDeinit(pstMuxerHandle->muxerId);
  else
    rkmuxer_deinit(pstMuxerHandle->muxerId);

  if (!strcmp(pstMuxerHandle->cOutputFmt, "mp4") && pstMuxerHandle->stThumbParam.bGetThumb) {
    RKADK_LOGI("muxerId[%d] cFileName[%s] not get thumb, retry!", pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);
    pstMuxerHandle->stThumbParam.bGetThumb = RKADK_MUXER_GetThumb(pstMuxerHandle);
    if (pstMuxerHandle->stThumbParam.bGetThumb)
      RKADK_LOGE("muxerId[%d] cFileName[%s] get thumb fail", pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);

    RKADK_MUXER_ForceRequestThumb(pstMuxerHandle);
  }

  if (!pstMuxer->enableFileCache) {
    if (pstMuxer->bBuildInThm && strcmp(pstMuxerHandle->cOutputFmt, "mp4")) {
      if (RKADK_MUXER_SaveThumb(&pstMuxerHandle->stThumbParam, pstMuxerHandle->cFileName))
        RKADK_LOGE("muxerId[%d] cFileName[%s] save thumb failed", pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);
    }

    if (pstMuxerHandle->stManualSplit.bSplitRecord) {
      RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_MANUAL_SPLIT_END,
                               pstMuxerHandle->realDuration);
      pstMuxerHandle->stManualSplit.bSplitRecord = false;
    } else {
      RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_END,
                               pstMuxerHandle->realDuration);
    }
  } else {
    pstMuxerHandle->stManualSplit.bSplitRecord = false;
  }

  if (pstMuxerHandle->stThumbParam.bRequestThumb && pstMuxer->enRecType != RKADK_REC_TYPE_SINGLE)
    RKADK_MUXER_ForceRequestThumb(pstMuxerHandle);

  // Reset muxer
  pstMuxerHandle->realDuration = 0;
  pstMuxerHandle->startTime = 0;
  pstMuxerHandle->lapseTimeStamp = 0;
  pstMuxerHandle->frameCnt = 0;
  pstMuxerHandle->bMuxering = false;
  pstMuxerHandle->bFirstFile = false;
  pstMuxerHandle->bIOError = false;
  pstMuxerHandle->stThumbParam.bGetThumb = false;
  pstMuxerHandle->stManualSplit.bEnableSplit = false;
  pstMuxerHandle->keyFrameCnt = 0;
}

static void RKADK_MUXER_RequestThumb(MUXER_HANDLE_S *pstMuxerHandle,
                                    MUXER_BUF_CELL_S *cell) {
  RKADK_U32 u32Duration;
  bool bThumbPts, bThumbFrame;
  bool bRequestThumb;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (!pstMuxer)
    return;

  if (pstMuxerHandle->duration <= 0)
    return;

  if (pstMuxerHandle->stManualSplit.bSplitRecord)
    u32Duration = pstMuxerHandle->stManualSplit.u32SplitDurationSec;
  else
    u32Duration = pstMuxerHandle->duration;

#ifndef THUMB_NORMAL
  int ret;
  bRequestThumb = cell->isKeyFrame && pstMuxerHandle->stThumbParam.bRequestThumb;

  bThumbFrame = pstMuxerHandle->frameCnt > ((u32Duration - pstMuxerHandle->gop /
                pstMuxerHandle->stVideo.frame_rate_num) * pstMuxerHandle->stVideo.frame_rate_num)
                ? true : false;

  bThumbPts = cell->pts - pstMuxerHandle->startTime >=
              (u32Duration - pstMuxerHandle->gop / pstMuxerHandle->stVideo.frame_rate_num) * 1000000
              ? true : false;

  if (bRequestThumb && (bThumbFrame || bThumbPts)) {
    RKADK_LOGI("Muxer[%d] venc[%d] request thumbnail frameCnt = %d, realDuration = %d",
                pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, pstMuxerHandle->frameCnt, pstMuxerHandle->realDuration);
    if (pstMuxer->enMuxerType == RKADK_MUXER_TYPE_THIRD_STREAM) {
      RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_REQUEST_THUMB, 0);
    } else {
      ret = RK_MPI_VENC_ThumbnailRequest(pstMuxerHandle->s32VencChn);
      if (ret)
        RKADK_LOGE("RK_MPI_VENC_ThumbnailRequest fail %x", ret);
    }

    pstMuxerHandle->stThumbParam.bRequestThumb = false;
  }
#else
  bRequestThumb = pstMuxerHandle->stThumbParam.bRequestThumb;

  bThumbFrame = pstMuxerHandle->frameCnt > (u32Duration * pstMuxerHandle->stVideo.frame_rate_num - 1)
                ? true : false;

  bThumbPts = cell->pts - pstMuxerHandle->startTime >=
              (u32Duration * 1000000 - 1000000 / pstMuxerHandle->stVideo.frame_rate_num * 2)
              ? true : false;

  if (bRequestThumb && (bThumbFrame || bThumbPts) ) {
    if (pstMuxerHandle->stThumbParam.bGetThumb) {
      RKADK_LOGI("muxerId[%d] cFileName[%s] not get thumb, retry!", pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);
      pstMuxerHandle->stThumbParam.bGetThumb = RKADK_MUXER_GetThumb(pstMuxerHandle);
      if (pstMuxerHandle->stThumbParam.bGetThumb)
        RKADK_LOGE("muxerId[%d] cFileName[%s] get thumb fail", pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);
    }

    RKADK_LOGI("muxerId[%d] venc[%d] request thumbnail frameCnt = %d, realDuration = %d",
        pstMuxerHandle->muxerId, pstMuxerHandle->s32ThumbVencChn, pstMuxerHandle->frameCnt, pstMuxerHandle->realDuration);
    if (pstMuxer->enMuxerType == RKADK_MUXER_TYPE_THIRD_STREAM)
      RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_REQUEST_THUMB, 0);
    else
      ThumbnailRequest(pstMuxerHandle->s32ThumbVencChn);

    pstMuxerHandle->stThumbParam.bRequestThumb = false;
  }
#endif
}

static bool RKADK_MUXER_CheckEnd(MUXER_HANDLE_S *pstMuxerHandle,
                                    MUXER_BUF_CELL_S *cell) {
  bool bFileSwitch;
  RKADK_U32 u32Duration;

  if (!cell->isKeyFrame || !pstMuxerHandle->startTime)
    return false;

  if (pstMuxerHandle->duration <= 0) {
    RKADK_LOGE("muxerId[%d] cFileName[%s] duration[%d]", pstMuxerHandle->muxerId,
        pstMuxerHandle->cFileName, pstMuxerHandle->duration);

    return false;
  }

  if (pstMuxerHandle->stManualSplit.bSplitRecord)
    u32Duration = pstMuxerHandle->stManualSplit.u32SplitDurationSec;
  else
    u32Duration = pstMuxerHandle->duration;

  if (pstMuxerHandle->stManualSplit.bEnableSplit) {
    RKADK_LOGI("File switch: muxer[%d], duration: %d",
        pstMuxerHandle->muxerId, pstMuxerHandle->realDuration);
    pstMuxerHandle->stManualSplit.bEnableSplit = false;
    RKADK_MUXER_Close(pstMuxerHandle);
    pstMuxerHandle->stManualSplit.bSplitRecord = true;
    return true;
  }

  bFileSwitch = cell->pts - pstMuxerHandle->startTime >=
                (u32Duration * 1000000 - 1000000 / pstMuxerHandle->stVideo.frame_rate_num);
  if (bFileSwitch) {
    RKADK_MUXER_Close(pstMuxerHandle);
    return true;
  }

  return false;
}

static int RKADK_MUXER_PreRecProc(MUXER_HANDLE_S *pstMuxerHandle) {
  int size;
  int64_t s64FirstTime = 0, s64SeekTime = 0;
  bool bPreRecord = false, bFindKeyFrame = false;
  MUXER_BUF_CELL_S *cell = NULL;
  RKADK_MUXER_PRE_RECORD_ATTR_S *pstAttr;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;

  if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
    return 0;

  RKADK_MUTEX_LOCK(pstMuxerHandle->stPreRecParam.mutex);
  size = RKADK_MUXER_GetListSize(&pstMuxerHandle->stPreRecParam.stVList);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->stPreRecParam.mutex);
  if (size < pstMuxerHandle->stVideo.frame_rate_num)
    return 0;

  pstAttr = &pstMuxerHandle->stPreRecParam.stAttr;
  switch (pstAttr->enPreRecordMode) {
  case RKADK_MUXER_PRE_RECORD_MANUAL_SPLIT:
    bPreRecord = pstMuxerHandle->stManualSplit.bSplitRecord && (pstAttr->u32PreRecTimeSec > 0);
    break;
  case RKADK_MUXER_PRE_RECORD_SINGLE:
    bPreRecord = pstMuxerHandle->bFirstFile && (pstAttr->u32PreRecTimeSec > 0);
    break;
  default:
    return 0;
  }

  if (!bPreRecord)
    return 0;

  RKADK_MUTEX_LOCK(pstMuxerHandle->stPreRecParam.mutex);
  RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stProcList);
  while (!list_empty(&pstMuxerHandle->stPreRecParam.stVList)) {
    cell = list_first_entry(&pstMuxerHandle->stPreRecParam.stVList, MUXER_BUF_CELL_S, mark);
    if (!s64FirstTime)
      s64FirstTime = cell->pts;

    if (!bFindKeyFrame) {
      if (cell->isKeyFrame) {
        bFindKeyFrame = true;
        s64SeekTime = cell->pts - s64FirstTime;
      } else {
        RKADK_MUXER_CellFree(pstMuxerHandle, cell);
        continue;
      }
    }

    list_del_init(&cell->mark);
    list_add_tail(&cell->mark, &pstMuxerHandle->stProcList);
  }

  if (!bFindKeyFrame) {
    RKADK_LOGD("don't find KeyFrame, pre_record fialed");
    RKADK_MUTEX_UNLOCK(pstMuxerHandle->stPreRecParam.mutex);
    return 0;
  }

  s64FirstTime = 0;
  while (!list_empty(&pstMuxerHandle->stPreRecParam.stAList)) {
    cell = list_first_entry(&pstMuxerHandle->stPreRecParam.stAList, MUXER_BUF_CELL_S, mark);
    if (!s64FirstTime)
      s64FirstTime = cell->pts;

    if ((cell->pts - s64FirstTime) < s64SeekTime) {
      RKADK_MUXER_CellFree(pstMuxerHandle, cell);
      continue;
    }

    list_del_init(&cell->mark);
    list_add_tail(&cell->mark, &pstMuxerHandle->stProcList);
  }

  RKADK_MUTEX_UNLOCK(pstMuxerHandle->stPreRecParam.mutex);
  return 1;
}

static bool RKADK_MUXER_Proc(void *params) {
  int ret = 0;
  RKADK_U32 u32Duration;
  MUXER_BUF_CELL_S *cell = NULL;
  RKADK_U32 u32LapseFrameInterval = 0;
  int64_t pts;
  RKADK_MUXER_PTS_INFO_S stPtsInfo;

  if (!params) {
    RKADK_LOGE("Invalid param");
    return false;
  }

  MUXER_HANDLE_S *pstMuxerHandle = (MUXER_HANDLE_S *)params;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pstMuxerHandle->ptr;
  RKADK_SIGNAL_Wait(pstMuxerHandle->pSignal, pstMuxerHandle->duration * 1000);

  cell = RKADK_MUXER_CellPop(pstMuxerHandle, &pstMuxerHandle->stProcList);
  while (cell) {
    // Create muxer
    if (pstMuxerHandle->bEnableStream) {
      pts = cell->pts;
      if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
        if (pstMuxerHandle->startTime != 0 && cell->pool == &pstMuxerHandle->stVFree) {
          u32LapseFrameInterval = 1000000 / pstMuxerHandle->stVideo.frame_rate_num; // us
          pstMuxerHandle->lapseTimeStamp += u32LapseFrameInterval;
          cell->pts = pstMuxerHandle->lapseTimeStamp;
        }
      }

      if (RKADK_MUXER_CheckEnd(pstMuxerHandle, cell)) {
        if (pstMuxer->enRecType == RKADK_REC_TYPE_SINGLE) {
          pstMuxerHandle->bEnableStream = false;
          RKADK_MUXER_CellFree(pstMuxerHandle, cell);
          RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stProcList);
          break;
        }
      }

      if (pstMuxerHandle->stManualSplit.bSplitRecord)
        u32Duration = pstMuxerHandle->stManualSplit.u32SplitDurationSec;
      else
        u32Duration = pstMuxerHandle->duration;

      if (!pstMuxerHandle->bMuxering && cell->isKeyFrame) {
        ret = pstMuxerHandle->pcbRequestFileNames(pstMuxerHandle->ptr,
                                                  pstMuxerHandle->cFileName,
                                                  pstMuxerHandle->muxerId);
        if (ret) {
          RKADK_LOGE("muxer[%d] request file name failed", pstMuxerHandle->muxerId);
          RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_ERR_GET_FILENAME, 0);
        } else {
          RKADK_LOGI("Ready to record new file path:[%s]", pstMuxerHandle->cFileName);

          RKADK_MUTEX_LOCK(pstMuxerHandle->paramMutex);
          if (pstMuxer->stThirdMuxerAttr.pfnMuxerInit) {
            ret = pstMuxer->stThirdMuxerAttr.pfnMuxerInit(pstMuxerHandle->muxerId,
                              (char *)pstMuxerHandle->cOutputFmt,
                              pstMuxerHandle->cFileName, &pstMuxerHandle->stVideo,
                              &pstMuxerHandle->stAudio);
          } else {
            ret = rkmuxer_init(pstMuxerHandle->muxerId,
                              (char *)pstMuxerHandle->cOutputFmt,
                              pstMuxerHandle->cFileName, &pstMuxerHandle->stVideo,
                              &pstMuxerHandle->stAudio);
          }
          RKADK_MUTEX_UNLOCK(pstMuxerHandle->paramMutex);
          if (ret) {
            RKADK_LOGE("rkmuxer init[%d] failed[%d]", pstMuxerHandle->muxerId, ret);
            if (!pstMuxer->enableFileCache)
              RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL, 0);
          } else {
            if (!pstMuxer->enableFileCache)
              RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_BEGIN, u32Duration);
            if (RKADK_MUXER_PreRecProc(pstMuxerHandle)) {
              MUXER_BUF_CELL_S *firstCell = RKADK_MUXER_CellPop(pstMuxerHandle, &pstMuxerHandle->stProcList);
              if (firstCell) {
                RKADK_MUXER_CellFree(pstMuxerHandle, cell);
                cell = firstCell;
              } else {
                RKADK_LOGE("pre_record proc ok, but cell pop fialed");
                RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stProcList);
              }
            }

            pstMuxerHandle->bMuxering = true;
            pstMuxerHandle->startTime = cell->pts;
            pstMuxerHandle->lapseTimeStamp = pstMuxerHandle->startTime;
            pstMuxerHandle->stThumbParam.bGetThumb = true;
            pstMuxerHandle->stThumbParam.bRequestThumb = true;
            pstMuxerHandle->u32FirstSeq = cell->seq;
            pts = cell->pts;
          }
        }
      } else if (!pstMuxerHandle->bMuxering) {
        if(cell->pool == &pstMuxerHandle->stVFree && pstMuxer->enMuxerType != RKADK_MUXER_TYPE_THIRD_STREAM) {
          RKADK_LOGI("muxer[%d] venc[%d] request idr!", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn);
          RK_MPI_VENC_RequestIDR(pstMuxerHandle->s32VencChn, RK_FALSE);
        }
      }

      // Process
      if (pstMuxerHandle->bMuxering) {
        // Write
        if (cell->pool == &pstMuxerHandle->stVFree) {
          if (pstMuxer->stThirdMuxerAttr.pfnWriteVideo) {
            ret = pstMuxer->stThirdMuxerAttr.pfnWriteVideo(pstMuxerHandle->muxerId,
                                      cell->buf, cell->size, cell->pts, cell->isKeyFrame);
          } else {
            ret = rkmuxer_write_video_frame(pstMuxerHandle->muxerId, cell->buf,
                                      cell->size, cell->pts, cell->isKeyFrame);
          }

          if (ret) {
            RKADK_LOGE("Muxer[%d] write video frame failed", pstMuxerHandle->muxerId);
            RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL, 0);
            pstMuxerHandle->bIOError = true;
          } else {
            pstMuxerHandle->bIOError = false;

            if (pstMuxer->pfnPtsCallback) {
              stPtsInfo.u32CamId = pstMuxer->u32CamId;
              stPtsInfo.u32ChnId = pstMuxerHandle->s32VencChn;
              stPtsInfo.s32MuxerId = pstMuxerHandle->muxerId;
              stPtsInfo.u64PTS = pts;
              stPtsInfo.u32Seq = cell->seq - pstMuxerHandle->u32FirstSeq;
              stPtsInfo.pFileName = pstMuxerHandle->cFileName;
              pstMuxer->pfnPtsCallback(&stPtsInfo);
            }

            if (pstMuxerHandle->bWriteFirstFrame) {
              RKADK_KLOG("Muxer[%d] venc[%d] write first frame pts: %lld",
                        pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, cell->pts);
              pstMuxerHandle->bWriteFirstFrame = false;
            }

            if (cell->pts < pstMuxerHandle->startTime)
              RKADK_LOGE("Muxer[%d] venc[%d] pts err pts[%lld], startTime[%lld]",
                          pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn, cell->pts, pstMuxerHandle->startTime);
            pstMuxerHandle->realDuration = (cell->pts - pstMuxerHandle->startTime) / 1000;
            pstMuxerHandle->frameCnt++;

            if(cell->isKeyFrame)
              pstMuxerHandle->keyFrameCnt++;

            if (pstMuxer->enRecType != RKADK_REC_TYPE_SINGLE)
              RKADK_MUXER_RequestThumb(pstMuxerHandle, cell);

            if (pstMuxerHandle->stThumbParam.bGetThumb) {
              if (pstMuxer->enableFileCache) {
                if (pstMuxerHandle->realDuration >= pstMuxerHandle->u32GetThumbTime)
                  pstMuxerHandle->stThumbParam.bGetThumb = RKADK_MUXER_GetThumb(pstMuxerHandle);
              } else {
                pstMuxerHandle->stThumbParam.bGetThumb = RKADK_MUXER_GetThumb(pstMuxerHandle);
              }
            }
          }
        } else if (cell->pool == &pstMuxerHandle->stAFree) {
          if (pstMuxer->stThirdMuxerAttr.pfnWriteAudio) {
            ret = pstMuxer->stThirdMuxerAttr.pfnWriteAudio(pstMuxerHandle->muxerId, cell->buf, cell->size, cell->pts);
          } else {
            ret = rkmuxer_write_audio_frame(pstMuxerHandle->muxerId, cell->buf, cell->size, cell->pts);
          }

          if (ret) {
            RKADK_LOGE("Muxer[%d] write audio frame failed", pstMuxerHandle->muxerId);
            RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL, 0);
            pstMuxerHandle->bIOError = true;
          } else {
            pstMuxerHandle->bIOError = false;
          }
        } else {
          RKADK_LOGE("unknow pool");
        }

#ifdef ENABLE_AOV
        if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE) {
          RKADK_MUTEX_LOCK(stAovHandle.mutex);
          if (!pstMuxerHandle->stAovParam.bIsSleep)
            RKADK_MUXER_EnterSleep(pstMuxer, pstMuxerHandle->s32VencChn);
          RKADK_MUTEX_UNLOCK(stAovHandle.mutex);
        }
#endif

      }
    }

    // free and next
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    cell = RKADK_MUXER_CellPop(pstMuxerHandle, &pstMuxerHandle->stProcList);
  }

  // Check exit
  if (!pstMuxerHandle->bEnableStream)
    RKADK_MUXER_Close(pstMuxerHandle);

  return pstMuxerHandle->bEnableStream;
}

static RKADK_S32 RKADK_MUXER_SetAVParam(MUXER_HANDLE_S *pMuxerHandle,
                                        RKADK_MUXER_STREAM_ATTR_S *pstSrcStreamAttr,
                                        RKADK_U32 u32FragKeyFrame, RKADK_MUXER_MP4_ATTR_S stMp4Attr)
{
  int i, ret = 0;
  RKADK_MUXER_TRACK_SOURCE_S *pstTrackSource = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)pMuxerHandle->ptr;

  RKADK_MUTEX_LOCK(pMuxerHandle->paramMutex);
  for (i = 0; i < (int)pstSrcStreamAttr->u32TrackCnt; i++) {
    pstTrackSource = &(pstSrcStreamAttr->aHTrackSrcHandle[i]);
    if (pstTrackSource->enTrackType == RKADK_TRACK_SOURCE_TYPE_VIDEO) {
      RKADK_TRACK_VIDEO_SOURCE_INFO_S *pstVideoInfo =
          &(pstTrackSource->unTrackSourceAttr.stVideoInfo);

      pMuxerHandle->gop = pstVideoInfo->u32Gop;
      pMuxerHandle->stVideo.width = pstVideoInfo->u32Width;
      pMuxerHandle->stVideo.height = pstVideoInfo->u32Height;
      pMuxerHandle->stVideo.bit_rate = pstVideoInfo->u32BitRate;
      pMuxerHandle->stVideo.frame_rate_den = 1;
      pMuxerHandle->stVideo.frame_rate_num = pstVideoInfo->u32FrameRate;
      pMuxerHandle->stVideo.profile = pstVideoInfo->u16Profile;
      pMuxerHandle->stVideo.level = pstVideoInfo->u16Level;
      pMuxerHandle->stVideo.frag_keyframe = u32FragKeyFrame;
      pMuxerHandle->stVideo.moov_front = stMp4Attr.u32MoovFront;
      pMuxerHandle->stVideo.reserved_moov_size = stMp4Attr.u32ReservedMoovSize;

      switch (pstVideoInfo->enCodecType) {
      case RKADK_CODEC_TYPE_H264:
        memcpy(pMuxerHandle->stVideo.codec, "H.264", strlen("H.264"));
        break;
      case RKADK_CODEC_TYPE_H265:
        memcpy(pMuxerHandle->stVideo.codec, "H.265", strlen("H.265"));
        break;
      default:
        RKADK_LOGE("not support enCodecType: %d", pstVideoInfo->enCodecType);
        ret = -1;
        goto exit;
      }

      if (pstMuxer->enableFileCache) {
#ifdef FILE_CACHE
        FILE_CACHE_HANDLE_S *pstFileCachehandle = &g_stFileCachehandle[pMuxerHandle->muxerId];
        pstFileCachehandle->stThumbParam.u32Width = pstVideoInfo->u32ThumbWidth;
        pstFileCachehandle->stThumbParam.u32Height = pstVideoInfo->u32ThumbHeight;
#endif
      } else {
        pMuxerHandle->stThumbParam.u32Width = pstVideoInfo->u32ThumbWidth;
        pMuxerHandle->stThumbParam.u32Height = pstVideoInfo->u32ThumbHeight;
      }

      //thumbnail infomation
      if (pstSrcStreamAttr->enType == RKADK_MUXER_TYPE_MP4) {
        if (!pMuxerHandle->stVideo.thumb.data && pstVideoInfo->u32ThumbWidth != 0 && pstVideoInfo->u32ThumbHeight != 0) {
          pMuxerHandle->stVideo.thumb.width = pstVideoInfo->u32ThumbWidth;
          pMuxerHandle->stVideo.thumb.height = pstVideoInfo->u32ThumbHeight;
          pMuxerHandle->stVideo.thumb.vir_width = pstVideoInfo->u32ThumbWidth;
          pMuxerHandle->stVideo.thumb.vir_height = pstVideoInfo->u32ThumbHeight;
          pMuxerHandle->stVideo.thumb.data_size = pstVideoInfo->u32ThumbWidth *
                                                  pstVideoInfo->u32ThumbHeight * 3 / 2;
          pMuxerHandle->stVideo.thumb.data = (unsigned char *)malloc(pMuxerHandle->stVideo.thumb.data_size);
          if (pMuxerHandle->stVideo.thumb.data)
            memset(pMuxerHandle->stVideo.thumb.data, 0, pMuxerHandle->stVideo.thumb.data_size);
        }
      }
    } else if (pstTrackSource->enTrackType == RKADK_TRACK_SOURCE_TYPE_AUDIO) {
      RKADK_TRACK_AUDIO_SOURCE_INFO_S *pstAudioInfo =
          &(pstTrackSource->unTrackSourceAttr.stAudioInfo);

      pMuxerHandle->stAudio.channels = pstAudioInfo->u32ChnCnt;
      pMuxerHandle->stAudio.frame_size = pstAudioInfo->u32SamplesPerFrame;
      pMuxerHandle->stAudio.sample_rate = pstAudioInfo->u32SampleRate;

      switch (pstAudioInfo->u32BitWidth) {
      case 16:
        memcpy(pMuxerHandle->stAudio.format, "S16", strlen("S16"));
        break;
      case 32:
        memcpy(pMuxerHandle->stAudio.format, "S32", strlen("S32"));
        break;
      default:
        RKADK_LOGE("not support u32BitWidth: %d", pstAudioInfo->u32BitWidth);
        ret = -1;
        goto exit;
      }

      switch (pstAudioInfo->enCodecType) {
      case RKADK_CODEC_TYPE_MP3:
#ifdef RKMUXER_MEDIA_SERVER
        memcpy(pMuxerHandle->stAudio.codec, "MP3", strlen("MP3"));
#else
        memcpy(pMuxerHandle->stAudio.codec, "MP2", strlen("MP2"));
#endif
        break;
      case RKADK_CODEC_TYPE_MP2:
        memcpy(pMuxerHandle->stAudio.codec, "MP2", strlen("MP2"));
        break;
      case RKADK_CODEC_TYPE_ACC:
        memcpy(pMuxerHandle->stAudio.codec, "ACC", strlen("ACC"));
        break;
      default:
        RKADK_LOGE("not support enCodecType: %d", pstAudioInfo->enCodecType);
        ret = -1;
        goto exit;
      }
    }
  }

exit:
  RKADK_MUTEX_UNLOCK(pMuxerHandle->paramMutex);
  return ret;
}

RKADK_S32 RKADK_MUXER_Enable(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                                    RKADK_MW_PTR pHandle) {
  int i, ret;
  char name[RKADK_THREAD_NAME_LEN];
  MUXER_HANDLE_S *pMuxerHandle = NULL;
  RKADK_MUXER_STREAM_ATTR_S *pstSrcStreamAttr = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  RKADK_LOGI("Enable Muxer[%d] Start...", pstMuxer->u32CamId);

  for (i = 0; i < (int)pstMuxerAttr->u32StreamCnt; i++) {
    pMuxerHandle = NULL;
    pstSrcStreamAttr = &(pstMuxerAttr->astStreamAttr[i]);
    if (pstSrcStreamAttr->s32MuxerId >= (RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT)) {
      RKADK_LOGE("Invalid muxerId[%d] >= max[%d]", pstSrcStreamAttr->s32MuxerId,
                 RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT);
      goto disable;
    }

    if (pstMuxerAttr->enFileType != pstSrcStreamAttr->enType) {
      RKADK_LOGE("enFileType[%d] != stream enType[%d]", pstMuxerAttr->enFileType, pstSrcStreamAttr->enType);
      goto disable;
    }

    pMuxerHandle = (MUXER_HANDLE_S *)malloc(sizeof(MUXER_HANDLE_S));
    if (!pMuxerHandle) {
      RKADK_LOGE("malloc muxer handle failed");
      goto disable;
    }
    memset(pMuxerHandle, 0, sizeof(MUXER_HANDLE_S));

    switch (pstSrcStreamAttr->enType) {
      case RKADK_MUXER_TYPE_MP4:
        pMuxerHandle->cOutputFmt = "mp4";
        break;
      case RKADK_MUXER_TYPE_MPEGTS:
        pMuxerHandle->cOutputFmt = "mpegts";
        break;
      case RKADK_MUXER_TYPE_FLV:
        pMuxerHandle->cOutputFmt = "flv";
        break;
      default:
        RKADK_LOGE("not support type: %d", pstSrcStreamAttr->enType);
        free(pMuxerHandle);
        goto disable;
    }

    pMuxerHandle->u32CamId = pstMuxerAttr->u32CamId;
    pMuxerHandle->pcbRequestFileNames = pstMuxerAttr->pcbRequestFileNames;
    pMuxerHandle->pfnEventCallback = pstMuxerAttr->pfnEventCallback;
    pMuxerHandle->bReseting = false;
    pMuxerHandle->duration = pstSrcStreamAttr->u32TimeLenSec;
    pMuxerHandle->bFirstFile = true;
    pMuxerHandle->bFirstKeyFrame = true;
    pMuxerHandle->bWriteFirstFrame = true;
    memcpy(&pstMuxer->stPipAttr[i], &pstMuxerAttr->astStreamAttr[i].stPipAttr, sizeof(RKADK_PIP_ATTR_S));

    if (pstMuxerAttr->astStreamAttr[i].u32GetThumbTime > 0)
      pMuxerHandle->u32GetThumbTime = pstMuxerAttr->astStreamAttr[i].u32GetThumbTime;
    else
      pMuxerHandle->u32GetThumbTime = 3000;

    memcpy(&pMuxerHandle->stPreRecParam.stAttr, &pstMuxerAttr->stPreRecordAttr,
            sizeof(RKADK_MUXER_PRE_RECORD_ATTR_S));
    ret = pthread_mutex_init(&pMuxerHandle->stPreRecParam.mutex, NULL);
    if (ret) {
      RKADK_LOGE("preRecord mutex init failed[%d]", ret);
      free(pMuxerHandle);
      goto disable;
    }

    ret = pthread_mutex_init(&pMuxerHandle->mutex, NULL);
    if (ret) {
      RKADK_LOGE("handle mutex init failed[%d]", ret);
      pthread_mutex_destroy(&pMuxerHandle->stPreRecParam.mutex);
      goto fail_1;
    }

    ret = pthread_mutex_init(&pMuxerHandle->paramMutex, NULL);
    if (ret) {
      RKADK_LOGE("param mutex init failed[%d]", ret);
      pthread_mutex_destroy(&pMuxerHandle->mutex);
      pthread_mutex_destroy(&pMuxerHandle->stPreRecParam.mutex);
      goto fail_1;
    }

    pMuxerHandle->ptr = (RKADK_MW_PTR)pstMuxer;
    pMuxerHandle->muxerId = pstSrcStreamAttr->s32MuxerId;
    pMuxerHandle->s32ViChn = pstSrcStreamAttr->s32ViChn;
    pMuxerHandle->s32VencChn = pstSrcStreamAttr->s32VencChn;
    pMuxerHandle->s32ThumbVencChn = pstSrcStreamAttr->s32ThumbVencChn;
    pMuxerHandle->bUseVpss = pstSrcStreamAttr->bUseVpss;
    if (RKADK_MUXER_SetAVParam(pMuxerHandle, pstSrcStreamAttr, pstMuxer->u32FragKeyFrame, pstMuxer->stMp4Attr)) {
      RKADK_LOGE("RKADK_MUXER_SetAVParam failed");
      goto fail;
    }

    // Init List
    RKADK_MUXER_ListInit(pMuxerHandle);

    // Create signal
    pMuxerHandle->pSignal = RKADK_SIGNAL_Create(0, 1);
    if (!pMuxerHandle->pSignal) {
      RKADK_LOGE("RKADK_SIGNAL_Create failed");
      goto fail;
    }
    snprintf(name, sizeof(name), "Muxer_%d", pMuxerHandle->muxerId);

    pMuxerHandle->pThread = RKADK_THREAD_Create(RKADK_MUXER_Proc, pMuxerHandle, name);
    if (!pMuxerHandle->pThread) {
      RKADK_LOGE("RKADK_THREAD_Create failed");
      goto fail;
    }

    if (pstMuxer->enableFileCache) {
#ifdef FILE_CACHE
      FILE_CACHE_HANDLE_S *pstFileCachehandle = &g_stFileCachehandle[pMuxerHandle->muxerId];
      memset(pstFileCachehandle, 0, sizeof(FILE_CACHE_HANDLE_S));
      pstFileCachehandle->bUsed = true;
      pstFileCachehandle->ptr = (RKADK_MW_PTR)pstMuxer;
      pstFileCachehandle->pfnEventCallback = pstMuxerAttr->pfnEventCallback;
      pstFileCachehandle->cOutputFmt = pMuxerHandle->cOutputFmt;
      pstFileCachehandle->bBuildInThm = pstMuxer->bBuildInThm;

      ret = pthread_mutex_init(&pstFileCachehandle->stThumbParam.mutex, NULL);
      if (ret) {
        RKADK_LOGE("thumb mutex init failed[%d]", ret);
        goto fail;
      }
#endif
    } else {
      ret = pthread_mutex_init(&pMuxerHandle->stThumbParam.mutex, NULL);
      if (ret) {
        RKADK_LOGE("thumb mutex init failed[%d]", ret);
        goto fail;
      }
    }

    pstMuxer->pMuxerHandle[i] = (RKADK_MW_PTR)pMuxerHandle;
  }

  RKADK_LOGI("Enable Muxer[%d] End...", pstMuxer->u32CamId);
  return 0;

  // Clean up the current muxer resources
fail:
  pthread_mutex_destroy(&pMuxerHandle->paramMutex);
  pthread_mutex_destroy(&pMuxerHandle->mutex);
  pthread_mutex_destroy(&pMuxerHandle->stPreRecParam.mutex);

fail_1:
  if (pMuxerHandle->pSignal)
    RKADK_SIGNAL_Destroy(pMuxerHandle->pSignal);

  if (pMuxerHandle->pThread)
    RKADK_THREAD_Destory(pMuxerHandle->pThread);

  if (pMuxerHandle->stVideo.thumb.data)
    free(pMuxerHandle->stVideo.thumb.data);

  if (pMuxerHandle)
    free(pMuxerHandle);

disable:
  // Clean up other muxer resources
  RKADK_MUXER_Disable(pHandle);
  RKADK_LOGI("Enable Muxer[%d] failed", pstMuxer->u32CamId);
  return -1;
}

RKADK_S32 RKADK_MUXER_Create(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                             RKADK_MW_PTR *ppHandle) {
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pstMuxerAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstMuxerAttr->u32CamId, RKADK_FAILURE);

  RKADK_LOGI("Create Muxer[%d] Start...", pstMuxerAttr->u32CamId);

  if (*ppHandle) {
    RKADK_LOGE("Muxer Handle has been created");
    return -1;
  }

  pstMuxer = (RKADK_MUXER_HANDLE_S *)malloc(sizeof(RKADK_MUXER_HANDLE_S));
  if (!pstMuxer) {
    RKADK_LOGE("malloc pstMuxer failed");
    return -1;
  }
  memset(pstMuxer, 0, sizeof(RKADK_MUXER_HANDLE_S));

  pstMuxer->u32CamId = pstMuxerAttr->u32CamId;
  pstMuxer->u32StreamCnt = pstMuxerAttr->u32StreamCnt;
  pstMuxer->enRecType = pstMuxerAttr->enRecType;
  pstMuxer->enMuxerType = pstMuxerAttr->enMuxerType;
  pstMuxer->u32FragKeyFrame = pstMuxerAttr->u32FragKeyFrame;
  pstMuxer->stMp4Attr.u32MoovFront = pstMuxerAttr->stMp4Attr.u32MoovFront;
  pstMuxer->stMp4Attr.u32ReservedMoovSize = pstMuxerAttr->stMp4Attr.u32ReservedMoovSize;
  pstMuxer->enFrameMode = MULTI_FRAME_MODE;
  pstMuxer->pfnPtsCallback = pstMuxerAttr->pfnPtsCallback;
  pstMuxer->pfnMountSdcard = pstMuxerAttr->pfnMountSdcard;
  pstMuxer->bEnableAiisp = pstMuxerAttr->bEnableAiisp;
  pstMuxer->bBuildInThm = true; // default build-in thumbnail;
  memcpy(&pstMuxer->stAovAttr, &pstMuxerAttr->stAovAttr, sizeof(RKADK_AOV_ATTR_S));

  if (pstMuxerAttr->pstThumbParam) {
    if (pstMuxerAttr->pstThumbParam->pThumbDir) {
      if (strlen(pstMuxerAttr->pstThumbParam->pThumbDir) >= RKADK_MAX_FILE_PATH_LEN) {
        RKADK_LOGE("thumb path len[%d] > RKADK_MAX_FILE_PATH_LEN[%d]",
                    strlen(pstMuxerAttr->pstThumbParam->pThumbDir), RKADK_MAX_FILE_PATH_LEN);
      } else {
        memcpy(pstMuxer->cThumbDir, pstMuxerAttr->pstThumbParam->pThumbDir, strlen(pstMuxerAttr->pstThumbParam->pThumbDir));
      }
    }

    pstMuxer->bBuildInThm = pstMuxerAttr->pstThumbParam->bBuildInThm;
    RKADK_LOGD("bBuildInThm: %d, cThumbDir: %s", pstMuxer->bBuildInThm, pstMuxer->cThumbDir);
  }

  if (pstMuxerAttr->pstThirdMuxerAttr)
    memcpy(&pstMuxer->stThirdMuxerAttr, pstMuxerAttr->pstThirdMuxerAttr, sizeof(RKADK_THIRD_MUXER_ATTR_S));

#ifdef ENABLE_AOV
  if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
    RKADK_MUXER_AovInit(&pstMuxerAttr->stAovAttr);
#endif

#ifdef FILE_CACHE
  pstMuxer->enableFileCache = getenv("file_cache_env") && atoi(getenv("file_cache_env"));

  if (pstMuxer->enableFileCache) {
#ifdef RKMUXER_MEDIA_SERVER
    if (pstMuxer->stMp4Attr.u32MoovFront) {
      RKADK_LOGW("mp4 moov front unsupported file cache");
      pstMuxer->enableFileCache = false;
    }

    if (pstMuxerAttr->enFileType == RKADK_MUXER_TYPE_FLV) {
      RKADK_LOGW("flv unsupported file cache");
      pstMuxer->enableFileCache = false;
    }
#endif

    if (pstMuxer->u32FragKeyFrame) {
      RKADK_LOGW("file cache unsupported key frame fragment");
      pstMuxer->u32FragKeyFrame = 0;
    }

    if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
      file_cache_set_mode(AOV_MODE);
    else
      file_cache_set_mode(NORMAL_MODE);
  }
#endif

#if defined(FILE_CACHE) || defined(ENABLE_AOV)
  g_pRecorder[pstMuxer->u32CamId] = (RKADK_MW_PTR)pstMuxer;
#endif

  *ppHandle = (RKADK_MW_PTR)pstMuxer;

  RKADK_LOGI("Create Muxer[%d] End...", pstMuxerAttr->u32CamId);
  return 0;
}

RKADK_S32 RKADK_MUXER_Disable(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  RKADK_LOGI("Disable Muxer[%d] Start...", pstMuxer->u32CamId);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

#ifdef FILE_CACHE
    if (pstMuxer->enableFileCache)
      file_cache_process_exit(pstMuxerHandle->cFileName);
#endif

    // Set flag off
    pstMuxerHandle->bEnableStream = false;

    // exit thread
    RKADK_THREAD_SetExit(pstMuxerHandle->pThread);

    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);

    // Destroy thread
    RKADK_THREAD_Destory(pstMuxerHandle->pThread);
    pstMuxerHandle->pThread = NULL;

    // Destroy signal
    RKADK_SIGNAL_Destroy(pstMuxerHandle->pSignal);

    if (pstMuxerHandle->stThumbParam.pMbBlk) {
      if (pstMuxerHandle->stThumbParam.pfnReleaseMbBlk) {
        pstMuxerHandle->stThumbParam.pfnReleaseMbBlk(pstMuxerHandle->stThumbParam.pMbBlk);
      } else {
        RKADK_LOGE("muxerId[%d] cFileName[%s] unregistered release thumb function",
                  pstMuxerHandle->muxerId, pstMuxerHandle->cFileName);
      }
      pstMuxerHandle->stThumbParam.pData = NULL;
      pstMuxerHandle->stThumbParam.pMbBlk = NULL;
    }

    // Release list
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stProcList);
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stAList);
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stVList);

    // Destory mutex
    pthread_mutex_destroy(&pstMuxerHandle->mutex);
    pthread_mutex_destroy(&pstMuxerHandle->paramMutex);
    pthread_mutex_destroy(&pstMuxerHandle->stPreRecParam.mutex);

    if (pstMuxer->enableFileCache) {
#ifdef FILE_CACHE
      pthread_mutex_destroy(&g_stFileCachehandle[pstMuxerHandle->muxerId].stThumbParam.mutex);
#endif
    } else {
      pthread_mutex_destroy(&pstMuxerHandle->stThumbParam.mutex);
    }

    if (pstMuxerHandle->stVideo.thumb.data) {
      free(pstMuxerHandle->stVideo.thumb.data);
      pstMuxerHandle->stVideo.thumb.data = NULL;
    }

    free(pstMuxer->pMuxerHandle[i]);
    pstMuxer->pMuxerHandle[i] = NULL;
  }

#ifdef ENABLE_AOV
  RKADK_MUTEX_LOCK(stAovHandle.mutex);
  if (pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE && pstMuxer->stAovAttr.pfnMultiFrame) {
    pstMuxer->stAovAttr.pfnMultiFrame(pstMuxer->u32CamId);
    RKADK_AOV_EnableNonBootCPUs();
  }
  RKADK_MUTEX_UNLOCK(stAovHandle.mutex);
#endif

  RKADK_LOGI("Disable Muxer[%d] End...", pstMuxer->u32CamId);
  return 0;
}

RKADK_S32 RKADK_MUXER_Destroy(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  RKADK_LOGI("Destory Muxer[%d] Start...", pstMuxer->u32CamId);

#ifdef ENABLE_AOV
  RKADK_MUXER_AovDeinit();
#endif

#if defined(FILE_CACHE) || defined(ENABLE_AOV)
  g_pRecorder[pstMuxer->u32CamId] = NULL;
#endif

  RKADK_LOGI("Destory Muxer[%d] End...", pstMuxer->u32CamId);

  free(pstMuxer);
  pstMuxer = NULL;

  return 0;
}

RKADK_S32 RKADK_MUXER_Start(RKADK_MW_PTR pHandle) {
  int ret;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    if ((pstMuxer->enMuxerType != RKADK_MUXER_TYPE_THIRD_STREAM) && (pstMuxerHandle->s32ThumbVencChn >= 0))
      RK_MPI_VENC_RequestIDR(pstMuxerHandle->s32VencChn, RK_FALSE);

    RKADK_MUXER_ForceRequestThumb(pstMuxerHandle);
    pstMuxerHandle->bEnableStream = true;
    pstMuxerHandle->bFirstFile = true;
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_START, 0);
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Stop(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    pstMuxerHandle->bEnableStream = false;
    pstMuxerHandle->stManualSplit.bEnableSplit = false;
    pstMuxerHandle->stManualSplit.bSplitRecord = false;
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_STOP, 0);
    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Single_Start(RKADK_MW_PTR pHandle, int s32MuxerId, int s32VencChn) {
  int ret;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, s32MuxerId, s32VencChn);
  if (!pstMuxerHandle) {
    RKADK_LOGE("Not find muxer[%d] venc[%d] handle", s32MuxerId, s32VencChn);
    return -1;
  }

  if ((pstMuxer->enMuxerType != RKADK_MUXER_TYPE_THIRD_STREAM) && (pstMuxerHandle->s32ThumbVencChn >= 0))
    RK_MPI_VENC_RequestIDR(pstMuxerHandle->s32VencChn, RK_FALSE);

  RKADK_MUXER_ForceRequestThumb(pstMuxerHandle);
  pstMuxerHandle->bEnableStream = true;
  pstMuxerHandle->bFirstFile = true;
  RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_START, 0);

  return 0;
}

RKADK_S32 RKADK_MUXER_Single_Stop(RKADK_MW_PTR pHandle, int s32MuxerId, int s32VencChn) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, s32MuxerId, s32VencChn);
  if (!pstMuxerHandle) {
    RKADK_LOGE("Not find muxer[%d] venc[%d] handle", s32MuxerId, s32VencChn);
    return -1;
  }

  pstMuxerHandle->bEnableStream = false;
  pstMuxerHandle->stManualSplit.bEnableSplit = false;
  pstMuxerHandle->stManualSplit.bSplitRecord = false;
  RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_STOP, 0);
  RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);

  return 0;
}

RKADK_S32 RKADK_MUXER_SetFrameRate(RKADK_MW_PTR pHandle,
                                   RKADK_MUXER_FPS_ATTR_S stFpsAttr) {
  return 0;
}

RKADK_S32 RKADK_MUXER_ManualSplit(RKADK_MW_PTR pHandle,
                        RKADK_MUXER_MANUAL_SPLIT_ATTR_S *pstSplitAttr) {
  int ret;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstSplitAttr, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  if (!pstSplitAttr->u32DurationSec) {
    RKADK_LOGE("invalid manual split duration[%d]", pstSplitAttr->u32DurationSec);
    return -1;
  }

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    if (!pstMuxerHandle->bEnableStream)
      return -1;

    if ((pstMuxer->enMuxerType != RKADK_MUXER_TYPE_THIRD_STREAM) && (pstMuxerHandle->s32ThumbVencChn >= 0))
      RK_MPI_VENC_RequestIDR(pstMuxerHandle->s32VencChn, RK_FALSE);

    if (pstSplitAttr->enManualType == MUXER_PRE_MANUAL_SPLIT)
      pstMuxerHandle->stPreRecParam.stAttr.enPreRecordMode = RKADK_MUXER_PRE_RECORD_MANUAL_SPLIT;

    pstMuxerHandle->stManualSplit.bEnableSplit = true;
    pstMuxerHandle->stManualSplit.u32SplitDurationSec = pstSplitAttr->u32DurationSec;
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_BuildInThumb(RKADK_MW_PTR pHandle, RKADK_MUXER_THUMB_INFO_S stThumbInfo) {
  int ret, position = 0;
  FILE *fp = NULL;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, stThumbInfo.s32MuxerId, -1);
  if (!pstMuxerHandle) {
    RKADK_LOGE("muxer[%d] venc[%d] not find pstMuxerHandle", stThumbInfo.s32MuxerId);
    goto ERR;
  }

  if (strcmp(pstMuxerHandle->cOutputFmt, "mp4")) {
    RKADK_LOGW("Format[%s] don't support built-in thumbnails", pstMuxerHandle->cOutputFmt);
    goto ERR;
  }

  if (pstMuxer->stThirdMuxerAttr.pfnGetThumbPos)
    position = pstMuxer->stThirdMuxerAttr.pfnGetThumbPos(pstMuxerHandle->muxerId);
  else
    position = rkmuxer_get_thumb_pos(pstMuxerHandle->muxerId);

  if (position > 0) {
    fp = fopen(pstMuxerHandle->cFileName, "r+");
    if (!fp) {
      RKADK_LOGE("Open %s file failed, errno[%d]", pstMuxerHandle->cFileName, errno, strerror(errno));
      goto ERR;
    }

    if (!fseek(fp, position, SEEK_SET)) {
      ret = fwrite(stThumbInfo.pBuf, stThumbInfo.u32Len, 1, fp);
      if (ret != 1) {
        RKADK_LOGI("Muxer[%d] file[%s] pos[%d] build in thumb error[%d, %s]",
                    pstMuxerHandle->muxerId, pstMuxerHandle->cFileName, position, errno, strerror(errno));
        goto ERR;
      }

      RKADK_LOGI("Muxer[%d] file[%s] pos[%d] build in thumb done!",
                  pstMuxerHandle->muxerId, pstMuxerHandle->cFileName, position);
      fclose(fp);
      return 0;
    } else {
      RKADK_LOGE("Seek error[%d, %s]", errno, strerror(errno));
      goto ERR;
    }
  } else {
    RKADK_LOGW("Muxer[%d] position[%d] invalid value!", pstMuxerHandle->muxerId, position);
  }

ERR:
  if (fp)
    fclose(fp);

  pstMuxerHandle->stThumbParam.bGetThumb = true;
  return -1;
}

RKADK_S32 RKADK_MUXER_ResetParam(RKADK_MUXER_ATTR_S *pstMuxerAttr, RKADK_MW_PTR pHandle) {
  int ret;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstMuxerAttr, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    pstMuxer->enRecType = pstMuxerAttr->enRecType;
    pstMuxerHandle->duration = pstMuxerAttr->astStreamAttr[i].u32TimeLenSec;
    if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
      memset(&pstMuxerHandle->stAudio, 0, sizeof(AudioParam));

    ret = RKADK_MUXER_SetAVParam(pstMuxerHandle, &pstMuxerAttr->astStreamAttr[i],
                                 pstMuxer->u32FragKeyFrame, pstMuxer->stMp4Attr);
    if (ret) {
      RKADK_LOGE("RKADK_MUXER_SetAVParam failed");
      return -1;
    }
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Single_ResetParam(RKADK_U32 chnId, RKADK_MW_PTR pHandle,
                             RKADK_MUXER_ATTR_S *pstMuxerAttr, int index) {
  int ret;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstMuxerAttr, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, -1, chnId);
  if (!pstMuxerHandle) {
    RKADK_LOGE("don't find muxer handle");
    return -1;
  }

  pstMuxer->enRecType = pstMuxerAttr->enRecType;
  pstMuxerHandle->duration = pstMuxerAttr->astStreamAttr[index].u32TimeLenSec;
  if (pstMuxer->enRecType == RKADK_REC_TYPE_LAPSE || pstMuxer->enRecType == RKADK_REC_TYPE_AOV_LAPSE)
    memset(&pstMuxerHandle->stAudio, 0, sizeof(AudioParam));

  ret = RKADK_MUXER_SetAVParam(pstMuxerHandle, &pstMuxerAttr->astStreamAttr[index],
                               pstMuxer->u32FragKeyFrame, pstMuxer->stMp4Attr);
  if (ret) {
    RKADK_LOGE("RKADK_MUXER_SetAVParam failed");
    return -1;
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Reset(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  char name[RKADK_THREAD_NAME_LEN];

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    RKADK_LOGI("Reset Muxer[%d] Start...", pstMuxerHandle->muxerId);

    RKADK_THREAD_SetExit(pstMuxerHandle->pThread);
    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);

    // Destroy thread
    RKADK_THREAD_Destory(pstMuxerHandle->pThread);
    pstMuxerHandle->pThread = NULL;

    snprintf(name, sizeof(name), "Muxer_%d_%d", pstMuxerHandle->muxerId, pstMuxerHandle->s32VencChn);
    pstMuxerHandle->pThread = RKADK_THREAD_Create(RKADK_MUXER_Proc, pstMuxerHandle, name);
    if (!pstMuxerHandle->pThread) {
      RKADK_LOGE("RKADK_THREAD_Create failed");
      return -1;
    }

    // Release list
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stProcList);
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stAList);
    RKADK_MUXER_ListRelease(pstMuxerHandle, &pstMuxerHandle->stPreRecParam.stVList);

#ifdef ENABLE_AOV
    pstMuxerHandle->stAovParam.bIsSleep = false;
#endif

    RKADK_LOGI("Reset Muxer[%d] End...", pstMuxerHandle->muxerId);
  }

#ifdef ENABLE_AOV
  if (stAovHandle.initCnt == 0)
    RKADK_MUXER_AovInit(&pstMuxer->stAovAttr);
#endif

  return 0;
}

void RKADK_MUXER_SetResetState(RKADK_MW_PTR pHandle, bool state) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER_N(pHandle);
  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  if (pstMuxer->u32StreamCnt > RKADK_MUXER_STREAM_MAX_CNT || !pstMuxer->u32StreamCnt)
    return;

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    pstMuxerHandle->bReseting = state;
    if (pstMuxer->enMuxerType != RKADK_MUXER_TYPE_THIRD_STREAM)
      RKADK_MEDIA_SetVencState(pstMuxerHandle->u32CamId, pstMuxerHandle->s32VencChn, state);
  }
}

int RKADK_MUXER_GetViChn(RKADK_MW_PTR pHandle, RKADK_S32 s32VencChn) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstRecorder = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstRecorder = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstRecorder->u32StreamCnt);

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstRecorder, -1, s32VencChn);
  if (!pstMuxerHandle) {
    RKADK_LOGD("Muxer Handle is NULL");
    return -1;
  }

  return pstMuxerHandle->s32ViChn;
}

bool RKADK_MUXER_IsUseVpss(RKADK_MW_PTR pHandle, RKADK_S32 s32VencChn) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstRecorder = NULL;

  if (!pHandle) {
    RKADK_LOGD("Recorder Handle is NULL");
    return false;
  }

  pstRecorder = (RKADK_MUXER_HANDLE_S *)pHandle;
  pstMuxerHandle = RKADK_MUXER_FindHandle(pstRecorder, -1, s32VencChn);
  if (!pstMuxerHandle) {
    RKADK_LOGD("Muxer Handle is NULL");
    return false;
  }

  return pstMuxerHandle->bUseVpss;
}

RKADK_S32 RKADK_MUXER_UpdateRes(RKADK_MW_PTR pHandle, RKADK_U32 chnId,
                              RKADK_U32 u32Wdith, RKADK_U32 u32Hieght) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstRecorder = NULL;

  if (!pHandle) {
    RKADK_LOGD("Recorder Handle is NULL");
    return -1;
  }

  pstRecorder = (RKADK_MUXER_HANDLE_S *)pHandle;
  pstMuxerHandle = RKADK_MUXER_FindHandle(pstRecorder, -1, chnId);
  if (!pstMuxerHandle) {
    RKADK_LOGD("Muxer Handle is NULL");
    return -1;
  }

  RKADK_MUTEX_LOCK(pstMuxerHandle->paramMutex);
  pstMuxerHandle->stVideo.width = u32Wdith;
  pstMuxerHandle->stVideo.height = u32Hieght;
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->paramMutex);
  return 0;
}

#ifdef FILE_CACHE
static void RKADK_MUXER_NotifyCallback(int cmd, void *msg0, void *msg1) {
  int i = 0, j = 0;
  RKADK_U32 value = 0;
  FILE_NOTIFY enNotify = cmd;
  char *filename = (char *)msg0;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_EVENT_E enEventType = RKADK_MUXER_EVENT_BUTT;
  FILE_CACHE_HANDLE_S *pstFileCachehandle = NULL;

  if (!filename) {
    RKADK_LOGW("filename is null");
    return;
  }

  RKADK_LOGD("filename[%s] enNotify: %d", filename, enNotify);

  if (enNotify != FILE_END) {
    for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
      if (g_pRecorder[i]) {
        pstMuxer = (RKADK_MUXER_HANDLE_S *)g_pRecorder[i];
        for (j = 0; j < (int)pstMuxer->u32StreamCnt; j++) {
          pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[j];
          if (!pstMuxerHandle) {
            RKADK_LOGD("Muxer Handle[%d] is NULL", j);
            continue;
          }

          if (!strcmp(pstMuxerHandle->cFileName, filename))
            break;

          pstMuxerHandle = NULL;
        }
      }

      if (pstMuxerHandle)
        break;
    }

    if (!pstMuxerHandle) {
      RKADK_LOGE("file[%s] not find pstMuxerHandle", filename);
      return;
    }
  } else {
    for(i = 0; i < (RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT); i++) {
      pstFileCachehandle = &g_stFileCachehandle[i];

      if (!pstFileCachehandle->bUsed)
        continue;

      if (!strcmp(pstFileCachehandle->cFileName, filename))
        break;

      pstFileCachehandle = NULL;
    }

    if (!pstFileCachehandle) {
      RKADK_LOGE("file[%s] not find pstFileCachehandle", filename);
      return;
    } else {
      if (pstFileCachehandle->bBuildInThm && strcmp(pstFileCachehandle->cOutputFmt, "mp4")) {
        if (RKADK_MUXER_SaveThumb(&pstFileCachehandle->stThumbParam, pstFileCachehandle->cFileName))
          RKADK_LOGE("cFileName[%s] save thumb failed", pstFileCachehandle->cFileName);
      }

      if (!pstFileCachehandle->pfnEventCallback) {
        RKADK_LOGE("Unregistered event callback");
        return;
      }
    }
  }

  switch (enNotify) {
    case FILE_NEW:
      enEventType = RKADK_MUXER_EVENT_FILE_BEGIN;
      if (pstMuxerHandle->stManualSplit.bSplitRecord)
        value = pstMuxerHandle->stManualSplit.u32SplitDurationSec;
      else
        value = pstMuxerHandle->duration;
      break;
    case FILE_END:
      if (pstFileCachehandle->bSplitRecord)
        enEventType = RKADK_MUXER_EVENT_MANUAL_SPLIT_END;
      else
        enEventType = RKADK_MUXER_EVENT_FILE_END;
      value = pstFileCachehandle->realDuration;
      break;
    case OPEN_FAILED:
      enEventType = RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL;
      value = *(int *)msg1;
      break;
    case WRITE_FAILED:
      enEventType = RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL;
      value = *(int *)msg1;
      break;
    case WRITE_SLOW:
      enEventType = RKADK_MUXER_EVENT_FILE_WRITING_SLOW;
      break;
    case CARD_NONEXIST:
      enEventType = RKADK_MUXER_EVENT_ERR_CARD_NONEXIST;
      break;
    default:
      RKADK_LOGW("Nosupport notify cmd: %d", enNotify);
      break;
  }

  if (enEventType == RKADK_MUXER_EVENT_BUTT)
    return;

  if (enNotify != FILE_END) {
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, enEventType, value);
  } else {
    RKADK_MUXER_EVENT_INFO_S stEventInfo;

    memset(&stEventInfo, 0, sizeof(RKADK_MUXER_EVENT_INFO_S));
    stEventInfo.enEvent = enEventType;
    stEventInfo.unEventInfo.stFileInfo.u32Duration = value;
    strncpy(stEventInfo.unEventInfo.stFileInfo.asFileName,
            pstFileCachehandle->cFileName, strlen(pstFileCachehandle->cFileName));
    pstFileCachehandle->pfnEventCallback(pstFileCachehandle->ptr, &stEventInfo);
  }
}

static void RKADK_MUXER_FsCacheNotify() {
  file_msg_reg_callback(RKADK_MUXER_NotifyCallback);
}
#endif

int RKADK_MUXER_FileCacheInit(FILE_CACHE_ARG *pstFileCacheAttr) {
#ifdef FILE_CACHE
  int ret = 0;

  RKADK_CHECK_POINTER(pstFileCacheAttr, RKADK_FAILURE);
  ret = file_cache_init(pstFileCacheAttr);
  memset(g_stFileCachehandle, 0, sizeof(FILE_CACHE_HANDLE_S) * RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT);
  RKADK_MUXER_FsCacheNotify();
  return ret;
#else
  RKADK_LOGE("Not define FILE_CACHE");
  return -1;
#endif
}

RKADK_S32 RKADK_MUXER_FileCacheDeInit() {
#ifdef FILE_CACHE
  FILE_CACHE_HANDLE_S *pstFileCachehandle = NULL;

  for (int i = 0; i < RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT; i++) {
    pstFileCachehandle = &g_stFileCachehandle[i];

    if (!pstFileCachehandle->bUsed)
      continue;

    if (pstFileCachehandle->stThumbParam.pMbBlk) {
      if (pstFileCachehandle->stThumbParam.pfnReleaseMbBlk) {
        pstFileCachehandle->stThumbParam.pfnReleaseMbBlk(pstFileCachehandle->stThumbParam.pMbBlk);
      } else {
        RKADK_LOGE("unregistered release thumb function");
      }
      pstFileCachehandle->stThumbParam.pData = NULL;
      pstFileCachehandle->stThumbParam.pMbBlk = NULL;
    }
  }


  return file_cache_deinit();
#else
  RKADK_LOGE("Not define FILE_CACHE");
  return -1;
#endif
}