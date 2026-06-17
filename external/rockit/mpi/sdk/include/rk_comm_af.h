/* GPL-2.0 WITH Linux-syscall-note OR Apache 2.0 */
/* Copyright (c) 2021 Fuzhou Rockchip Electronics Co., Ltd */

#ifndef INCLUDE_RT_MPI_MPI_COMM_AF_H_
#define INCLUDE_RT_MPI_MPI_COMM_AF_H_

#include "rk_type.h"
#include "rk_comm_aio.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

typedef enum rkAUDIO_FILTER_TYPE_E {
    AUDIO_FILTER_RESAMPLE = 0,
    AUDIO_FILTER_3A       = 1,
    AUDIO_FILTER_MAP      = 2,
    AUDIO_FILTER_CACHE    = 3,
    AUDIO_FILTER_VOLUME   = 4,
    AUDIO_FILTER_SED      = 5,
    AUDIO_FILTER_BUTT
} AUDIO_FILTER_TYPE_E;

typedef enum rkAUDIO_AF_VQE_TYPE_E {
    AUDIO_FILTER_VQE_TX = 0,
    AUDIO_FILTER_VQE_RX,
    AUDIO_FILTER_VQE_BUTT
} AUDIO_AF_VQE_TYPE_E;

typedef struct rkAF_RESAMPLE_ATTR_S {
    RK_U32 u32InRate;
    RK_U32 u32InChn;
    AUDIO_BIT_WIDTH_E   enInBitWidth;

    RK_U32 u32OutRate;
    RK_U32 u32OutChn;
    AUDIO_BIT_WIDTH_E   enOutBitWidth;
} AF_RESAMPLE_ATTR_S;

typedef struct rkAF_3A_ATTR_S {
    char   cfgPath[MAX_AUDIO_FILE_PATH_LEN];
    RK_U32 u32SampleRate;
    RK_U32 u32Channels;
    AUDIO_BIT_WIDTH_E enBitWidth;
    RK_U32 u32ChnLayout;
    RK_U32 u32RefLayout;
    RK_U32 u32RecLayout;
    AUDIO_AF_VQE_TYPE_E enTpye;
} AF_3A_ATTR_S;

typedef struct rkAF_MAP_ATTR_S {
    RK_U32              u32SampleRate;
    RK_U32              u32InChn;
    AUDIO_BIT_WIDTH_E   enBitWidth;
    RK_U8               u8MapOutChns;
    RK_U8               u8MapChns[MAX_SOUND_CARD_CHANNEL];
} AF_MAP_ATTR_S;

typedef struct rkAF_BASIC_ATTR {
    RK_U32              u32SampleRate;
    RK_U32              u32Channel;
    AUDIO_BIT_WIDTH_E   enBitWidth;
} AF_BASIC_ATTR;

typedef struct rkAF_CACHE_ATTR_S {
    AF_BASIC_ATTR       stBasicAttr;
    RK_S32              s32Size;
} AF_CACHE_ATTR_S;

typedef struct rkAF_VOLUME_ATTR_S {
    AF_BASIC_ATTR       stBasicAttr;
    RK_S32              s32Volume;
} AF_VOLUME_ATTR_S;

typedef struct rkAF_SED_PARAM {
    RK_S32    s32FrameLen;
    RK_S32    s32Nclass;
    RK_S32    s32BabyCryDecisionLen;
    RK_S32    s32BuzzerDecisionLen;
    RK_S32    s32GlassBreakingDecisionLen;
    RK_FLOAT  fBabycryConfirmProb;
    RK_FLOAT  fBuzzerConfirmProb;
    RK_FLOAT  fGlassBreakingConfirmProb;
    RK_CHAR   fBcdModelPath[MAX_AUDIO_FILE_PATH_LEN];
} AF_SED_PARAM;

typedef struct rkAF_SED_ATTR_S {
    AF_BASIC_ATTR       stBasicAttr;
    RK_S32              s32ModelEnabled;
    AUDIO_AGC_CONFIG_S  stAgcParam;
    AI_AED_CONFIG_S     stAedParam;
    AF_SED_PARAM        stSedLenParam;
    RK_S64              s64RecChannelType;
    RK_BOOL             bCache;
    RK_S32              s32TimeOut;
} AF_SED_ATTR_S;

typedef struct rkAF_ATTR_S {
    AUDIO_FILTER_TYPE_E enType;
    RK_U32 u32InBufCount;   /* max buffer count for input cache */
    RK_U32 u32OutBufCount;  /* max buffer count for output cache */
    union {
        AF_RESAMPLE_ATTR_S stResAttr;
        AF_3A_ATTR_S       st3AAttr;
        AF_MAP_ATTR_S      stMapAttr;
        AF_CACHE_ATTR_S    stCacheAttr;
        AF_VOLUME_ATTR_S   stVolumeAttr;
        AF_SED_ATTR_S      stSedAttr;
    };
    RK_U32 u32Depth;
    RK_U32 u32MaxInputSize;
} AF_ATTR_S;

typedef struct rkAF_EXTRA_INFO_S{
    RK_BOOL  bExtra;
    RK_BOOL  bAed;
    RK_BOOL  bLsd;
    RK_BOOL  bBcd;
    RK_BOOL  bBuz;
    RK_BOOL  bGbs;
    RK_FLOAT fLsdResult;
} AF_EXTRA_INFO_S;

/* at lease one parameter is illagal ,eg, an illegal enumeration value  */
#define RK_ERR_AF_INVALID_CHNID     RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_INVALID_CHNID)
#define RK_ERR_AF_EXIST             RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_EXIST)
#define RK_ERR_AF_UNEXIST           RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_UNEXIST)
#define RK_ERR_AF_ILLEGAL_PARAM     RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_ILLEGAL_PARAM)
#define RK_ERR_AF_NULL_PTR          RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_NULL_PTR)
#define RK_ERR_AF_BUF_FULL          RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_BUF_FULL)
#define RK_ERR_AF_NOBUF             RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_NOBUF)
#define RK_ERR_AF_SYS_NOTREADY      RK_DEF_ERR(RK_ID_AF, RK_ERR_LEVEL_ERROR, RK_ERR_NOTREADY)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif  // INCLUDE_RT_MPI_MPI_COMM_AF_H_
