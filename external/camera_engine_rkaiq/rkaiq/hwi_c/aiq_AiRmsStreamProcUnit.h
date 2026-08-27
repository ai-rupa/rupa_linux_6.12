/*
 *  Copyright (c) 2024 Rockchip Corporation
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
 */

#ifndef _AIQ_AIRMS_STREAM_PROC_UNIT_H_
#define _AIQ_AIRMS_STREAM_PROC_UNIT_H_

#include "c_base/aiq_thpool.h"
#include "c_base/aiq_list.h"
#include "c_base/aiq_thread.h"
#include "hwi_c/aiq_rawStreamProcUnit.h"
#include "hwi_c/aiq_stream.h"
#include "include/common/rk_aiq_types.h"
#include "xcore_c/aiq_v4l2_buffer.h"

#define AIRMS_HELP_THREAD_POOL_NUM       (2)
#define AIRMS_HELP_THREAD_SCHED_POLICY   (SCHED_RR)
#define AIRMS_HELP_THREAD_SCHED_PRIORITY (80)
#define AIRMS_CONVERT_THREAD_MSG_MAX     (6)
#define AIRMS_INBUF_NUM                  (3)
#define AIRMS_OUTBUF_NUM                 (3)
#define AIRMS_DUMPBUF_DIR                "/data/airms_dump"
#define AIRMS_MGE_THREAD_POOL_NUM        (2)
#define AIRMS_MGE_THREAD_START_COREID    (2)
#define AIRMS_MAX_WIDTH                  (4096)
#define AIRMS_EXTEND_PIXEL               (16)

typedef enum AirmsBayerPattern_e {
    AIRMS_BAYER_RGGB = 0,
    AIRMS_BAYER_BGGR = 1,
    AIRMS_BAYER_GRBG = 2,
    AIRMS_BAYER_GBRG = 3,
} AirmsBayerPattern_t;

typedef struct _AiRmsAiispBuf {
    bool dump_flg;
    bool in_used;
    int buf_idx;
    int dma_fd;
    char *virt_addr;
    uint32_t buf_size;
    AiqV4l2Buffer_t aiqV4l2Buf;
    struct v4l2_plane planes;
} AiRmsAiispBuf;

typedef struct AiqAiRmsStreamProcUnit_s AiqAiRmsStreamProcUnit_t;

typedef struct _AirmsAwbGain {
    uint16_t r;
    uint16_t gr;
    uint16_t gb;
    uint16_t b;
} AirmsAwbGain;

typedef struct _AirmsBlc {
    uint16_t r;
    uint16_t gr;
    uint16_t gb;
    uint16_t b;
} AirmsBlc;

typedef struct _AirmsStreamParam {
    int hdlIdx;
    uint16_t* pSrc;
    AiRmsAiispBuf *aiispInBuf;
    AiqAiRmsStreamProcUnit_t* pProcUnit;
    bool awb_gain_en;
    AirmsBayerPattern_t bayer_pattern;
    AirmsAwbGain awb_gain;
    AirmsBlc blc;
} AirmsStreamParam;

typedef struct _AirmsMgeParam {
    int hdlIdx;
    AiRmsAiispBuf *aiispOutBuf;
    AiqAiRmsStreamProcUnit_t* pProcUnit;
} AirmsMgeParam;

typedef struct AirmsStreamHelperThd_s {
    AiqThread_t* _base;
    AiqAiRmsStreamProcUnit_t* mAiRmsStreamProc;
    AiqList_t* mMsgsQueue;
    AiqMutex_t _mutex;
    AiqCond_t _cond;
    AiqMutex_t result_mutex;
    AiqCond_t result_cond;
    bool bQuit;
} AirmsStreamHelperThd_t;

typedef struct _AirmsQuardConvertParam {
    AiqV4l2Buffer_t *vicapbuf;
} AirmsQuardConvertParam;

typedef struct AirmsQuardConvertThd_s {
    AiqThread_t* _base;
    AiqAiRmsStreamProcUnit_t* mAiRmsStreamProc;
    AiqList_t* mMsgsQueue;
    AiqMutex_t _mutex;
    AiqCond_t _cond;
    bool bQuit;
} AirmsQuardConvertThd_t;

typedef struct AiqAiRmsStreamProcUnit_s {
    struct AiqV4l2Device_s _device;
    AiqStream_t* _AiRmsStream;
    AiqV4l2Device_t* mAiIspDev;
    AiqV4l2SubDevice_t* mAiIspSubDev;
    bool mStartFlag;
    bool mStartStreamFlag;
    AiqMutex_t mStreamMutex;
    AiqPollCallback_t* _pcb;
    int hdlThNum;
    int dump_raw_num;

    struct v4l2_buffer v4l2_buf;
    struct v4l2_format v4l2_format;

    bool mBufPoolInit;
    struct rkaiisp_rmsbuf_info mRmsbufInfo;
    AiRmsAiispBuf inbuf_tbl[AIRMS_INBUF_NUM];
    AiRmsAiispBuf outbuf_tbl[AIRMS_OUTBUF_NUM];
    AiqMutex_t inbuf_mutex;
    struct rkaiisp_param_info mParamInfo;
    AiqRawStreamProcUnit_t* _proc_stream;
    rk_aiq_aiisp_info_t aiisp_info;

    bool mPrepareOK;
    bool mStartOK;
    uint32_t model_max_runcnt;
    uint32_t isp_acq_width;
    uint32_t isp_acq_height;
    char mModelFile[AIRMS_MODEL_FILENAME_LEN];
    char *mModelBuf;

    threadpool mHelpThPool;
    AirmsQuardConvertThd_t mQuardConvertThd;
    uint16_t mCompY[33];
    enum rkaiisp_model_mode mModelMode;

    AiqCamHwBase_t* pCamHw;
    bool is_parthdl;
    threadpool mMgeThPool;

    uint32_t inbuf_num;
    uint32_t outbuf_num;

    // for awb gain via cpu
    bool cpu_awb_gain_en;
    AirmsBayerPattern_t bayer_pattern;
    bool awb_gain_valid;
    AirmsAwbGain last_awb_gain;
    AirmsBlc last_blc;
    uint32_t cpu_awb_gain_frame_count;
    int64_t cpu_awb_gain_total_us;
} AiqAiRmsStreamProcUnit_t;

XCamReturn AiqAiRmsStreamProcUnit_init(AiqCamHwBase_t* pCamHw, AiqAiRmsStreamProcUnit_t* pProcUnit, rk_aiq_aiisp_info_t* aiisp_info);
XCamReturn AiqAiRmsStreamProcUnit_deinit(AiqAiRmsStreamProcUnit_t* pProcUnit);
XCamReturn AiqAiRmsStreamProcUnit_prepare(AiqAiRmsStreamProcUnit_t* pProcUnit,
                                          uint32_t isp_acq_width, uint32_t isp_acq_height,
                                          char* model_file, uint32_t inbuf_num,
                                          uint32_t outbuf_num);
void AiqAiRmsStreamProcUnit_start(AiqAiRmsStreamProcUnit_t* pProcUnit);
void AiqAiRmsStreamProcUnit_stop(AiqAiRmsStreamProcUnit_t* pProcUnit);
void AiqAiRmsStreamProcUnit_set_devices(AiqAiRmsStreamProcUnit_t* pProcUnit, AiqRawStreamProcUnit_t* proc);
void AiqAiRmsStreamProcUnit_setVicapBuf(AiqAiRmsStreamProcUnit_t* pProcUnit, AiqV4l2Buffer_t *vicapbuf);
int AiqAiRmsStreamProcUnit_dumpRaw(AiqAiRmsStreamProcUnit_t* pProcUnit, int dump_raw_num);
void AiqAiRmsStreamProcUnit_setCpuAwbGainEn(AiqAiRmsStreamProcUnit_t* pProcUnit, bool en);
void AiqAiRmsStreamProcUnit_setBayerPattern(AiqAiRmsStreamProcUnit_t* pProcUnit,
                                            AirmsBayerPattern_t pattern);
void AiqAiRmsStreamProcUnit_getBytesPerline(AiqAiRmsStreamProcUnit_t* pProcUnit, uint32_t width, uint32_t* bytes_perline);

#endif  // _AIQ_AIRMS_STREAM_PROC_UNIT_H_
