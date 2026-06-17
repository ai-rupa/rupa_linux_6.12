/* GPL-2.0 WITH Linux-syscall-note OR Apache 2.0 */
/* Copyright (c) 2022 Fuzhou Rockchip Electronics Co., Ltd */

#ifndef INCLUDE_RT_MPI_MPI_AMIX_H_
#define INCLUDE_RT_MPI_MPI_AMIX_H_

#include "rk_common.h"
#include "rk_comm_aio.h"
#include "rk_comm_amix.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

RK_S32 RK_MPI_AMIX_ListContents(AUDIO_DEV AmixDevId);
RK_S32 RK_MPI_AMIX_ListControls(AUDIO_DEV AmixDevId);
RK_S32 RK_MPI_AMIX_SetControl(AUDIO_DEV AmixDevId, const char *control, char *values);
RK_S32 RK_MPI_AMIX_GetControl(AUDIO_DEV AmixDevId, const char *control, char *values);
RK_S32 RK_MPI_AMIX_CreateGrp(AMIX_GRP AMIXGrp, const AMIX_GRP_ATTR_S *pstGrpAttr);
RK_S32 RK_MPI_AMIX_DestroyGrp(AMIX_GRP AMIXGrp);
RK_S32 RK_MPI_AMIX_StartGrp(AMIX_GRP AMIXGrp);
RK_S32 RK_MPI_AMIX_StopGrp(AMIX_GRP AMIXGrp);
RK_S32 RK_MPI_AMIX_SetPipeAttr(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe,
                             const AMIX_PIPE_ATTR_S *pstPipeAttr);
RK_S32 RK_MPI_AMIX_GetPipeAttr(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe, AMIX_PIPE_ATTR_S *pstPipeAttr);
RK_S32 RK_MPI_AMIX_SetPipeUpdateAttr(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe,
                                    const AMIX_UPDATE_PIPE_ATTR_S *pstPipeAttr);
RK_S32 RK_MPI_AMIX_GetPipeUpdateAttr(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe, AMIX_UPDATE_PIPE_ATTR_S *pstPipeAttr);
RK_S32 RK_MPI_AMIX_EnablePipe(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe);
RK_S32 RK_MPI_AMIX_DisablePipe(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe);
RK_S32 RK_MPI_AMIX_SendPipeFrame(AMIX_GRP AMIXGrp, AMIX_PIPE AMIXPipe,
                                const AUDIO_FRAME_S *pstFrm,
                                RK_S32 s32MilliSec);
RK_S32 RK_MPI_AMIX_SetChnAttr(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn,
                             const AMIX_CHN_ATTR_S *pstChnAttr);
RK_S32 RK_MPI_AMIX_GetChnAttr(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn, AMIX_CHN_ATTR_S *pstChnAttr);
RK_S32 RK_MPI_AMIX_EnableChn(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn);
RK_S32 RK_MPI_AMIX_DisableChn(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn);
RK_S32 RK_MPI_AMIX_GetChnFrame(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn,
                              AUDIO_FRAME_S *pstFrm, RK_S32 s32MilliSec);
RK_S32 RK_MPI_AMIX_ReleaseChnFrame(AMIX_GRP AMIXGrp, AMIX_CHN AMIXChn,
                                  const AUDIO_FRAME_S *pstFrm);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif  /* End of INCLUDE_RT_MPI_MPI_AMIX_H_ */
