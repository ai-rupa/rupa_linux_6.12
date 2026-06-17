/* GPL-2.0 WITH Linux-syscall-note OR Apache 2.0 */
/* Copyright (c) 2021 Fuzhou Rockchip Electronics Co., Ltd */

#ifndef INCLUDE_RT_MPI_MPI_COMM_AMIX_H_
#define INCLUDE_RT_MPI_MPI_COMM_AMIX_H_

#include "rk_type.h"
#include "rk_comm_aio.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

typedef enum rkAMIX_MODE_E {
    AMIX_MODE_MIX        = 0,

    AMIX_MODE_BUTT
} AMIX_MODE_E;

typedef struct rkAMIX_SOUND_ATTR {
    RK_U32              u32Channels;
    RK_S32              s32SampleRate;
    AUDIO_BIT_WIDTH_E   enBitWidth;
} AMIX_SOUND_ATTR;

typedef struct rkAMIX_GRP_ATTR_S {
    AMIX_MODE_E          enMode;
    RK_U32               u32PeriodSize;
    RK_U32               u32StartDelay;
} AMIX_GRP_ATTR_S;

typedef struct rkAMIX_PIPE_ATTR_S {
    AMIX_SOUND_ATTR      stAttr;
    RK_U32               u32InBufCnt;
} AMIX_PIPE_ATTR_S;

typedef struct rkAMIX_CHN_ATTR_S {
    AMIX_SOUND_ATTR      stAttr;
    RK_U32               u32OutBufCnt;
    RK_U32               u32Depth;
} AMIX_CHN_ATTR_S;

typedef struct rkAMIX_UPDATE_PIPE_ATTR_S {
    RK_FLOAT             fDB;
} AMIX_UPDATE_PIPE_ATTR_S;

/* at lease one parameter is illagal ,eg, an illegal enumeration value  */
#define RK_ERR_AMIX_NULL_PTR         RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NULL_PTR)
#define RK_ERR_AMIX_NOTREADY         RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NOTREADY)
#define RK_ERR_AMIX_INVALID_DEVID    RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_INVALID_DEVID)
#define RK_ERR_AMIX_INVALID_CHNID    RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_INVALID_CHNID)
#define RK_ERR_AMIX_INVALID_PIPEID   RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_INVALID_PIPEID)
#define RK_ERR_AMIX_EXIST            RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_EXIST)
#define RK_ERR_AMIX_UNEXIST          RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_UNEXIST)
#define RK_ERR_AMIX_NOT_SUPPORT      RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NOT_SUPPORT)
#define RK_ERR_AMIX_NOT_PERM         RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NOT_PERM)
#define RK_ERR_AMIX_NOMEM            RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NOMEM)
#define RK_ERR_AMIX_NOBUF            RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_NOBUF)
#define RK_ERR_AMIX_ILLEGAL_PARAM    RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_ILLEGAL_PARAM)
#define RK_ERR_AMIX_BUF_EMPTY        RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_BUF_EMPTY)
#define RK_ERR_AMIX_BUF_FULL         RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_BUF_FULL)
#define RK_ERR_AMIX_BUSY             RK_DEF_ERR(RK_ID_AMIX, RK_ERR_LEVEL_ERROR, RK_ERR_BUSY)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif  // INCLUDE_RT_MPI_MPI_COMM_AMIX_H_
