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

#ifndef __RKADK_MUXER_H__
#define __RKADK_MUXER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rkadk_common.h"
#include "rkadk_aov.h"
#include "rkadk_media_comm.h"

#ifndef OS_RTT
#include "file_common.h"
#include "rkmuxer.h"
#else
#include "../../common_algorithm/misc/include/file_common.h"
#include "../../common_algorithm/misc/include/rkmuxer.h"
#endif

#define RKADK_MUXER_FILE_NAME_LEN RKADK_MAX_FILE_PATH_LEN
#define RKADK_MUXER_STREAM_MAX_CNT RECORD_FILE_NUM_MAX
#define RKADK_MUXER_TRACK_MAX_CNT 2 /* a video track and a audio track */
#define RKADK_MUXER_CELL_MAX_CNT 40

typedef enum {
  RKADK_MUXER_EVENT_STREAM_START = 0,
  RKADK_MUXER_EVENT_STREAM_STOP,
  RKADK_MUXER_EVENT_FILE_BEGIN,
  RKADK_MUXER_EVENT_FILE_END,
  RKADK_MUXER_EVENT_MANUAL_SPLIT_END,
  RKADK_MUXER_EVENT_ERR_GET_FILENAME,
  RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL,
  RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL,
  RKADK_MUXER_EVENT_FILE_WRITING_SLOW,
  RKADK_MUXER_EVENT_ERR_CARD_NONEXIST,
  RKADK_MUXER_EVENT_REQUEST_THUMB,
  RKADK_MUXER_EVENT_BUILD_IN_THUMB,
  RKADK_MUXER_EVENT_BUTT
} RKADK_MUXER_EVENT_E;

typedef struct {
  RKADK_S32 s32MuxerId;
  unsigned char* pBuf;
  RKADK_U32 u32Len;
} RKADK_MUXER_THUMB_INFO_S;

typedef struct {
  RKADK_CHAR asFileName[RKADK_MUXER_FILE_NAME_LEN];
  RKADK_U32 u32Duration; // ms
} RKADK_MUXER_FILE_EVENT_INFO_S;

typedef struct {
  RKADK_CHAR asFileName[RKADK_MUXER_FILE_NAME_LEN];
  RKADK_S32 s32ErrorCode;
} RKADK_MUXER_ERROR_EVENT_INFO_S;

typedef struct {
  RKADK_S32 s32MuxerId;
  RKADK_CHAR asFileName[RKADK_MUXER_FILE_NAME_LEN];
} RKADK_MUXER_THUMB_EVENT_INFO_S;

typedef struct {
  RKADK_MUXER_EVENT_E enEvent;
  union {
    RKADK_MUXER_FILE_EVENT_INFO_S stFileInfo;
    RKADK_MUXER_ERROR_EVENT_INFO_S stErrorInfo;
    RKADK_MUXER_THUMB_EVENT_INFO_S stThumbInfo;
  } unEventInfo;
} RKADK_MUXER_EVENT_INFO_S;

typedef struct {
  RKADK_U32 u32CamId;
  RKADK_S32 s32MuxerId;
  RKADK_U32 u32ChnId;
  RKADK_U64 u64PTS;
  RKADK_U32 u32Seq;
  const char *pFileName;
} RKADK_MUXER_PTS_INFO_S;

/* muxer pts callback function */
typedef RKADK_VOID (*RKADK_MUXER_PTS_CALLBACK_FN)(const RKADK_MUXER_PTS_INFO_S *pstPtsInfo);

/* muxer event callback function */
typedef RKADK_VOID (*RKADK_MUXER_EVENT_CALLBACK_FN)(
    RKADK_MW_PTR pHandle, const RKADK_MUXER_EVENT_INFO_S *pstEventInfo);

/* Muxer request file name callback function */
typedef int (*RKADK_MUXER_REQUEST_FILE_NAME_CB)(RKADK_VOID *pHandle, RKADK_CHAR *pcFileName,
                                          RKADK_U32 u32MuxerId);

/* cell release buf callback */
typedef int (*RKADK_MUXER_RELEASE_CALLBACK)(void *pMbBlk);

/* Third party muxer registration function */
typedef int (*RKADK_THIRD_MUXER_INIT_FN)(int muxerId, char *format, const char *filePath,
                                          VideoParam *videoParam, AudioParam *audioParam);
typedef int (*RKADK_THIRD_MUXER_DEINIT_FN)(int muxerId);
typedef int (*RKADK_THIRD_MUXER_WRITE_VIDEO_FN)(int muxerId, unsigned char *buffer,
                                                unsigned int bufferSize, int64_t pts, int keyFrame);
typedef int (*RKADK_THIRD_MUXER_WRITE_AUDIO_FN)(int muxerId, unsigned char *buffer,
                                                unsigned int bufferSize, int64_t pts);
typedef long long int (*RKADK_THIRD_MUXER_GET_THUMB_POS_FN)(int muxerId);

/* muxer manual split type enum */
typedef enum {
  MUXER_PRE_MANUAL_SPLIT,      /* pre manual split type */
  MUXER_NORMAL_MANUAL_SPLIT,   /* normal manual split type */
} RKADK_MUXER_MANUAL_SPLIT_TYPE_E;

/* pre manual split attribute */
typedef struct {
  RKADK_MUXER_MANUAL_SPLIT_TYPE_E enManualType; /* maual split type */
  RKADK_U32 u32DurationSec; /* file duration of manual split file */
} RKADK_MUXER_MANUAL_SPLIT_ATTR_S;

typedef enum {
  RKADK_MUXER_TYPE_MP4 = 0,
  RKADK_MUXER_TYPE_MPEGTS,
  RKADK_MUXER_TYPE_FLV,
  RKADK_MUXER_TYPE_BUTT
} RKADK_MUXER_FILE_TYPE_E;

typedef enum {
  RKADK_TRACK_SOURCE_TYPE_VIDEO = 0,
  RKADK_TRACK_SOURCE_TYPE_AUDIO,
  RKADK_TRACK_SOURCE_TYPE_BUTT
} RKADK_TRACK_SOURCE_TYPE_E;

typedef struct {
  RKADK_CODEC_TYPE_E enCodecType;
  RKADK_CHAR cPixFmt[RKADK_PIX_FMT_LEN];
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U32 u32BitRate;
  RKADK_U32 u32FrameRate;
  RKADK_U32 u32Gop;
  RKADK_U16 u16Profile;
  RKADK_U16 u16Level;
  RKADK_U32 u32ThumbWidth;
  RKADK_U32 u32ThumbHeight;
} RKADK_TRACK_VIDEO_SOURCE_INFO_S;

typedef struct {
  RKADK_CODEC_TYPE_E enCodecType;
  RKADK_U16 u32BitWidth;
  RKADK_U32 u32ChnCnt;
  RKADK_U32 u32SampleRate;
  RKADK_U32 u32SamplesPerFrame;
  RKADK_U32 u32Bitrate;
} RKADK_TRACK_AUDIO_SOURCE_INFO_S;

typedef struct {
  RKADK_TRACK_SOURCE_TYPE_E enTrackType;
  union {
    RKADK_TRACK_VIDEO_SOURCE_INFO_S stVideoInfo; /* <video track info */
    RKADK_TRACK_AUDIO_SOURCE_INFO_S stAudioInfo; /* <audio track info */
  } unTrackSourceAttr;
} RKADK_MUXER_TRACK_SOURCE_S;

typedef struct {
  RKADK_BOOL bEnablePip;                      /* enable picture-in-picture */
  RKADK_U32 u32AvsGrpId;                      /* avs group id [0, AVS_MAX_GRP_NUM)] */
  RKADK_U32 u32AvsBufCnt;                     /* default 2, min value 2 */
  RKADK_U32 u32SubCamId;                      /* subwindow camera id */
  RKADK_STREAM_TYPE_E enSubStreamType;        /* subwindow stream type */
  RKADK_RECT_S stSubRect;                     /* The subwindow is based on the display area of the main window */
} RKADK_PIP_ATTR_S;

/* muxer stream attribute */
typedef struct {
  RKADK_S32 s32MuxerId;
  /* If it is a third-party stream that is not Rockit(such as a network stream),
    * s32ViChn/s32VencChn must set to -1.
  */
  RKADK_S32 s32ViChn;
  RKADK_S32 s32VencChn;
  RKADK_S32 s32ThumbVencChn;
  bool bUseVpss;
  RKADK_U32 u32TimeLenSec; /* record time */
  RKADK_PIP_ATTR_S stPipAttr;
  RKADK_U32 u32GetThumbTime;
  RKADK_U32 u32TrackCnt;   /* track cnt*/
  RKADK_MUXER_TRACK_SOURCE_S
  aHTrackSrcHandle[RKADK_MUXER_TRACK_MAX_CNT]; /* array of track source cnt */
  RKADK_MUXER_FILE_TYPE_E enType;
} RKADK_MUXER_STREAM_ATTR_S;

typedef enum {
  RKADK_MUXER_PRE_RECORD_NONE = 0,
  RKADK_MUXER_PRE_RECORD_MANUAL_SPLIT, /* manual split file prerecord */
  RKADK_MUXER_PRE_RECORD_SINGLE        /* first file prerecord */
} RKADK_MUXER_PRE_RECORD_MODE_E;

typedef struct {
  RKADK_U32 u32PreRecTimeSec; /* pre record time, unit in second(s)*/
  RKADK_U32 u32PreRecCacheTime;
  RKADK_MUXER_PRE_RECORD_MODE_E enPreRecordMode;
} RKADK_MUXER_PRE_RECORD_ATTR_S;

/** muxer fps attribute */
typedef struct {
  RKADK_U32 u32Fps;      /* framerate */
  RKADK_BOOL bSplitFile; /* stop current file, record the next file immediately
                            at the new framerate */
  RKADK_STREAM_TYPE_E enStreamType; /* stream type */
} RKADK_MUXER_FPS_ATTR_S;

/* record type enum */
typedef enum {
  RKADK_REC_TYPE_NORMAL = 0, /* normal record */
  RKADK_REC_TYPE_LAPSE, /* time lapse record, record a frame by an fixed time
                           interval */
  RKADK_REC_TYPE_AOV_LAPSE, /* low power time lapse record */
  RKADK_REC_TYPE_SINGLE, /* single record: record one file of record_time then stop */
  RKADK_REC_TYPE_BUTT,
} RKADK_MUXER_REC_TYPE_E;

typedef void (*RKADK_ISP_WAKE_UP_PAUSE_FN)(RKADK_U32 u32CamId);
typedef void (*RKADK_ISP_WAKE_UP_RESUME_FN)(RKADK_U32 u32CamId);
typedef int (*RKADK_ISP_SET_FRAME_RATE_FN)(RKADK_U32 u32CamId, unsigned int uFps);
typedef int (*RKADK_MOUMNT_SDCARD_FN)(void);

typedef struct {
  RKADK_ISP_WAKE_UP_PAUSE_FN pfnSingleFrame;
  RKADK_ISP_WAKE_UP_RESUME_FN pfnMultiFrame;
} RKADK_AOV_ATTR_S;

typedef enum {
  RKADK_MUXER_TYPE_RECORD = 0,
  RKADK_MUXER_TYPE_THIRD_STREAM,
} RKADK_MUXER_TYPE_E;

/* Third party muxer attribute */
typedef struct {
  RKADK_THIRD_MUXER_INIT_FN pfnMuxerInit;
  RKADK_THIRD_MUXER_DEINIT_FN pfnMuxerDeinit;
  RKADK_THIRD_MUXER_WRITE_VIDEO_FN pfnWriteVideo;
  RKADK_THIRD_MUXER_WRITE_AUDIO_FN pfnWriteAudio;
  RKADK_THIRD_MUXER_GET_THUMB_POS_FN pfnGetThumbPos;
} RKADK_THIRD_MUXER_ATTR_S;

typedef struct {
  RKADK_U32 u32MoovFront;        /* moov front, default moov back */
  RKADK_U32 u32ReservedMoovSize; /* reserved moov size(Bytes), default 0, suggest aligning 1024B*/
} RKADK_MUXER_MP4_ATTR_S;

/* muxer attribute param */
typedef struct {
  RKADK_U32 u32CamId;
  RKADK_MUXER_FILE_TYPE_E enFileType;
  RKADK_MUXER_REC_TYPE_E enRecType;
  RKADK_MUXER_TYPE_E enMuxerType;
  RKADK_U32 u32StreamCnt;                       /* stream cnt */
  RKADK_U32 u32FragKeyFrame;
  RKADK_MUXER_STREAM_ATTR_S
  astStreamAttr[RKADK_MUXER_STREAM_MAX_CNT];    /* array of stream attr */
  RKADK_MUXER_PRE_RECORD_ATTR_S stPreRecordAttr;
  RKADK_MUXER_REQUEST_FILE_NAME_CB pcbRequestFileNames;
  RKADK_MUXER_EVENT_CALLBACK_FN pfnEventCallback;
  RKADK_MUXER_PTS_CALLBACK_FN pfnPtsCallback;
  RKADK_AOV_ATTR_S stAovAttr;
  RKADK_MOUMNT_SDCARD_FN pfnMountSdcard;
  bool bEnableAiisp;
  RKADK_THIRD_MUXER_ATTR_S *pstThirdMuxerAttr;  /* third party muxer attr */
  RKADK_THUMB_PARAM_S *pstThumbParam;           /* only for ts/flv */
  RKADK_MUXER_MP4_ATTR_S stMp4Attr;
} RKADK_MUXER_ATTR_S;

typedef enum {
  MULTI_FRAME_MODE,
  SINGLE_FRAME_MODE,
} RKADK_ISP_FRAME_MODE;

typedef struct {
  RKADK_S32 s32MuxerId;
  RKADK_S32 s32VencChn;
  int isKeyFrame;
  RKADK_U64 u64Pts;
  RKADK_U32 u32Seq;
  RKADK_U32 u32Len;
  MB_BLK pMbBlk;
} RKADK_MUXER_DATA_S;

typedef struct {
  RKADK_U32 u32CamId;
  RKADK_MUXER_REC_TYPE_E enRecType;
  RKADK_MUXER_TYPE_E enMuxerType;
  RKADK_U32 u32StreamCnt;
  RKADK_MW_PTR pMuxerHandle[RKADK_MUXER_STREAM_MAX_CNT];
  RKADK_U64 u64AudioPts;
  RKADK_U32 u32FragKeyFrame;
  int enableFileCache;
  RKADK_AOV_ATTR_S stAovAttr;
  RKADK_ISP_FRAME_MODE enFrameMode;
  RKADK_MUXER_PTS_CALLBACK_FN pfnPtsCallback;
  RKADK_MOUMNT_SDCARD_FN pfnMountSdcard;
  RKADK_PIP_ATTR_S stPipAttr[RECORD_FILE_NUM_MAX];
  bool bEnableAiisp;
  RKADK_THIRD_MUXER_ATTR_S stThirdMuxerAttr; /* third party muxer attr */
  char cThumbDir[RKADK_MAX_FILE_PATH_LEN];
  bool bBuildInThm;
  RKADK_MUXER_MP4_ATTR_S stMp4Attr;
} RKADK_MUXER_HANDLE_S;

/**
 * @brief create a new muxer
 * @param[in]pstRecAttr : the attribute of muxer
 * @param[out]ppRecorder : pointer of muxer
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_Create(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                           RKADK_MW_PTR *ppHandle);

RKADK_S32 RKADK_MUXER_Enable(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                           RKADK_MW_PTR pHandle);

RKADK_S32 RKADK_MUXER_Disable(RKADK_MW_PTR pHandle);

/**
 * @brief destory a muxer.
 * @param[in]pRecorder : pointer of muxer
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_Destroy(RKADK_MW_PTR pHandle);

/**
 * @brief start muxer
 * @param[in]pRecorder : pointer of muxer
 * @return 0 success
 * @return -1 failure
 */
RKADK_S32 RKADK_MUXER_Start(RKADK_MW_PTR pHandle);

/**
 * @brief stop muxer
 * @param[in]pRecorder : pointer of muxer
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_Stop(RKADK_MW_PTR pHandle);

/**
 * @brief start muxer
 * @param[in]pRecorder : pointer of muxer
 * @param[in]enStrmType : stream type, mainStream or subStream
 * @return 0 success
 * @return -1 failure
 */
RKADK_S32 RKADK_MUXER_Single_Start(RKADK_MW_PTR pHandle, int s32MuxerId, int s32VencChn);

/**
 * @brief stop muxer
 * @param[in]pRecorder : pointer of muxer
 * @param[in]enStrmType : stream type, mainStream or subStream
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_Single_Stop(RKADK_MW_PTR pHandle, int s32MuxerId, int s32VencChn);

/**
 * @brief set muxer framerate
 * @param[in]pRecorder : pointer of muxer
 * @param[in]stFpsAttr : fps attribute
 * the new framerate
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_SetFrameRate(RKADK_MW_PTR pHandle,
                                 RKADK_MUXER_FPS_ATTR_S stFpsAttr);

/**
 * @brief manual splite file.
 * @param[in]pRecorder : pointer of muxer
 * @param[in]pstSplitAttr : manual split attr.
 * @return 0 success
 * @return others failure
 */
RKADK_S32 RKADK_MUXER_ManualSplit(RKADK_MW_PTR pHandle,
                                RKADK_MUXER_MANUAL_SPLIT_ATTR_S *pstSplitAttr);

/**
 * @brief write video frame
 */
int RKADK_MUXER_WriteVideoFrame(RKADK_MUXER_DATA_S stData, void *handle);

/**
 * @brief write audio frame
 */
int RKADK_MUXER_WriteAudioFrame(RKADK_MUXER_DATA_S stData, void *handle);

RKADK_S32 RKADK_MUXER_BuildInThumb(RKADK_MW_PTR pHandle, RKADK_MUXER_THUMB_INFO_S stThumbInfo);

RKADK_S32 RKADK_MUXER_ResetParam(RKADK_MUXER_ATTR_S *pstMuxerAttr, RKADK_MW_PTR pHandle);

RKADK_S32 RKADK_MUXER_Single_ResetParam(RKADK_U32 chnId, RKADK_MW_PTR pHandle,
                              RKADK_MUXER_ATTR_S *pstMuxerAttr, int index);

RKADK_S32 RKADK_MUXER_Reset(RKADK_MW_PTR pHandle);

void RKADK_MUXER_SetResetState(RKADK_MW_PTR pHandle, bool state);

int RKADK_MUXER_GetViChn(RKADK_MW_PTR pHandle, RKADK_S32 s32VencChn);

bool RKADK_MUXER_IsUseVpss(RKADK_MW_PTR pHandle, RKADK_S32 s32VencChn);

RKADK_S32 RKADK_MUXER_UpdateRes(RKADK_MW_PTR pHandle, RKADK_U32 chnId,
                              RKADK_U32 u32Wdith, RKADK_U32 u32Hieght);

int RKADK_MUXER_FileCacheInit(FILE_CACHE_ARG *pstFileCacheAttr);
RKADK_S32 RKADK_MUXER_FileCacheDeInit();

#ifdef __cplusplus
}
#endif
#endif
