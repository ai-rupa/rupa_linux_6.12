/*
 * Copyright (c) 2019-2022 Rockchip Eletronics Co., Ltd.
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
#include "uAPI2/rk_aiq_user_api2_af.h"

#include "aiq_core_c/algo_handlers/RkAiqAfHandler.h"
#include "hwi_c/aiq_CamHwBase.h"
#include "base/xcam_common.h"

RKAIQ_BEGIN_DECLARE

#ifdef RK_SIMULATOR_HW
#define CHECK_USER_API_ENABLE
#endif

#if RKAIQ_HAVE_AF

XCamReturn
rk_aiq_user_api2_af_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_attrib_t *attr)
{
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_AF);
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

            if (algo_handle) {
                LOGD_AF("%s: AfMode %d, Win: %d, %d, %d, %d",
                    __func__, attr->AfMode, attr->h_offs, attr->v_offs, attr->h_size, attr->v_size);
                AiqAlgoHandlerAf_setAttrib(algo_handle, attr);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            LOGD_AF("%s: AfMode %d, Win: %d, %d, %d, %d",
                __func__, attr->AfMode, attr->h_offs, attr->v_offs, attr->h_size, attr->v_size);
            return AiqAlgoHandlerAf_setAttrib(algo_handle, attr);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_attrib_t *attr)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_getAttrib(algo_handle, attr);
    }

    return XCAM_RETURN_NO_ERROR;
}

#else

XCamReturn
rk_aiq_user_api2_af_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_attrib_t *attr)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_attrib_t *attr)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

#endif

#if RKAIQ_HAVE_AF_V20 || RKAIQ_HAVE_AF_V30 || RKAIQ_HAVE_AF_V31 || RKAIQ_HAVE_AF_V32_LITE || RKAIQ_HAVE_AF_V33

XCamReturn
rk_aiq_user_api2_af_SetCalib(const rk_aiq_sys_ctx_t* sys_ctx, void *calib)
{
    CHECK_USER_API_ENABLE2(sys_ctx);
    CHECK_USER_API_ENABLE(RK_AIQ_ALGO_TYPE_AF);
    RKAIQ_API_SMART_LOCK(sys_ctx);

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
#if ISP_HW_V35
        GlobalParamsManager_checkStatsrc(&ctx_array.ctx[i]->_rkAiqManager->mGlobalParamsManager, calib, RESULT_TYPE_AF_PARAM);
#endif
            if (algo_handle) {
                AiqAlgoHandlerAf_setCalib(algo_handle, calib);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
#if ISP_HW_V35
        GlobalParamsManager_checkStatsrc(&sys_ctx->_rkAiqManager->mGlobalParamsManager, calib, RESULT_TYPE_AF_PARAM);
#endif
        if (algo_handle) {
            return AiqAlgoHandlerAf_setCalib(algo_handle, calib);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_GetCalib(const rk_aiq_sys_ctx_t* sys_ctx, void *calib)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_getCalib(algo_handle, calib);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_Lock(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_lock(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_lock(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_Unlock(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_unlock(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_unlock(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_Oneshot(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_Oneshot(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_Oneshot(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_ManualTriger(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_ManualTriger(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_ManualTriger(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_Tracking(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_Tracking(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_Tracking(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_SetZoomIndex(const rk_aiq_sys_ctx_t* sys_ctx, int index)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_setZoomIndex(algo_handle, index);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_setZoomIndex(algo_handle, index);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_EndZoomChg(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_endZoomChg(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_endZoomChg(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_GetZoomIndex(const rk_aiq_sys_ctx_t* sys_ctx, int *index)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_getZoomIndex(algo_handle, index);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_StartZoomCalib(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_startZoomCalib(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_startZoomCalib(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_resetZoom(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            AiqAlgoHandlerAf_t* algo_handle =
                (AiqAlgoHandlerAf_t*)ctx_array.ctx[i]->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];
            if (algo_handle) {
                AiqAlgoHandlerAf_resetZoom(algo_handle);
            }
        }
    } else {
        AiqAlgoHandlerAf_t* algo_handle =
            (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

        if (algo_handle) {
            return AiqAlgoHandlerAf_resetZoom(algo_handle);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_SetVcmCfg(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_lens_vcmcfg* cfg)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_setLensVcmCfg(ctx_array.ctx[i]->_camHw, cfg);
        }
    } else {
        ret = AiqCamHw_setLensVcmCfg(sys_ctx->_camHw, cfg);
    }
    return ret;
}

XCamReturn
rk_aiq_user_api2_af_GetVcmCfg(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_lens_vcmcfg* cfg)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_getLensVcmCfg(ctx_array.ctx[i]->_camHw, cfg);
        }
    } else {
        ret = AiqCamHw_getLensVcmCfg(sys_ctx->_camHw, cfg);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_GetSearchPath(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_sec_path_t* path)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_GetSearchPath(algo_handle, path);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_GetSearchResult(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_result_t* result)
{
	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);

    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_GetSearchResult(algo_handle, result);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_GetZoomRange(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_zoomrange* range)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_GetZoomRange(algo_handle, range);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_GetFocusRange(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_focusrange* range)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);

	const rk_aiq_sys_ctx_t* ctx = rk_aiq_user_api2_common_getSysCtx(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_GetFocusRange(algo_handle, range);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_user_api2_af_FocusCorrection(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_FocusCorrection(ctx_array.ctx[i]->_camHw);
        }
    } else {
        ret = AiqCamHw_FocusCorrection(sys_ctx->_camHw);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_ZoomCorrection(const rk_aiq_sys_ctx_t* sys_ctx)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_ZoomCorrection(ctx_array.ctx[i]->_camHw);
        }
    } else {
        ret = AiqCamHw_ZoomCorrection(sys_ctx->_camHw);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_setAngleZ(const rk_aiq_sys_ctx_t* sys_ctx, float angleZ)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_setAngleZ(ctx_array.ctx[i]->_camHw, angleZ);
        }
    } else {
        ret = AiqCamHw_setAngleZ(sys_ctx->_camHw, angleZ);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_GetFocusPos(const rk_aiq_sys_ctx_t* sys_ctx, int* pos)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (sys_ctx->cam_type == RK_AIQ_CAM_TYPE_GROUP) {
        rk_aiq_sys_ctx_array_t ctx_array = rk_aiq_user_api2_common_getSysCtxArray(sys_ctx);
        for (int i = 0; i < ctx_array.num; i++) {
            ret = AiqCamHw_getFocusPosition(ctx_array.ctx[i]->_camHw, pos);
        }
    } else {
        ret = AiqCamHw_getFocusPosition(sys_ctx->_camHw, pos);
    }

    return ret;
}

XCamReturn
rk_aiq_user_api2_af_SetOtpAf(const rk_aiq_sys_ctx_t* sys_ctx, struct rkmodule_af_inf *otp_af, struct rkmodule_pdaf_inf *otp_pdaf)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_setOtpAf(algo_handle, otp_af, otp_pdaf);
    }

    return XCAM_RETURN_ERROR_FAILED;
}

#else

XCamReturn
rk_aiq_user_api2_af_SetCalib(const rk_aiq_sys_ctx_t* sys_ctx, void *calib)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetCalib(const rk_aiq_sys_ctx_t* sys_ctx, void *calib)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_Lock(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_Unlock(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_Oneshot(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_ManualTriger(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_Tracking(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_SetZoomIndex(const rk_aiq_sys_ctx_t* sys_ctx, int index)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_EndZoomChg(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetZoomIndex(const rk_aiq_sys_ctx_t* sys_ctx, int *index)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_StartZoomCalib(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_resetZoom(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_SetVcmCfg(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_lens_vcmcfg* cfg)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetVcmCfg(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_lens_vcmcfg* cfg)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetSearchPath(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_sec_path_t* path)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetSearchResult(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_result_t* result)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetZoomRange(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_zoomrange* range)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetFocusRange(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_af_focusrange* range)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_FocusCorrection(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_ZoomCorrection(const rk_aiq_sys_ctx_t* sys_ctx)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_setAngleZ(const rk_aiq_sys_ctx_t* sys_ctx, float angleZ)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetFocusPos(const rk_aiq_sys_ctx_t* sys_ctx, int* pos)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_SetOtpAf(const rk_aiq_sys_ctx_t* sys_ctx, struct rkmodule_af_inf *otp_af, struct rkmodule_pdaf_inf *otp_pdaf)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

#endif

#if RKAIQ_HAVE_AF_V30 || RKAIQ_HAVE_AF_V33

XCamReturn
rk_aiq_user_api2_af_SetPdafLibInput(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_pdlib_input* pdlib_in)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_setPdafLibInput(algo_handle, pdlib_in);
    }

    return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn
rk_aiq_user_api2_af_GetPdafLibOutput(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_pdlib_output* pdlib_output, int timeout_ms)
{
    RKAIQ_API_SMART_LOCK(sys_ctx);
    AiqAlgoHandlerAf_t* algo_handle =
        (AiqAlgoHandlerAf_t*)sys_ctx->_analyzer->mAlgoHandleMaps[RK_AIQ_ALGO_TYPE_AF];

    if (algo_handle) {
        return AiqAlgoHandlerAf_getPdafLibOutput(algo_handle, pdlib_output, timeout_ms);
    }

    return XCAM_RETURN_ERROR_FAILED;
}

#else

XCamReturn
rk_aiq_user_api2_af_SetPdafLibInput(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_pdlib_input* pdlib_in)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

XCamReturn
rk_aiq_user_api2_af_GetPdafLibOutput(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_pdlib_output* pdlib_output, int timeout_ms)
{
    return XCAM_RETURN_ERROR_UNKNOWN;
}

#endif

RKAIQ_END_DECLARE
