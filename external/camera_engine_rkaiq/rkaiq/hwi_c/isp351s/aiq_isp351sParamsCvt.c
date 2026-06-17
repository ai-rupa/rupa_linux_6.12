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

#include "hwi_c/isp33/aiq_isp33ParamsCvt.h"
#include "hwi_c/isp39/aiq_isp39ParamsCvt.h"
#include "hwi_c/isp35/aiq_isp35ParamsCvt.h"

#include "hwi_c/aiq_ispParamsCvt.h"
#include "include/algos/awb/fixfloat.h"
#include "include/algos/awb/rk_aiq_types_awb_algo_int.h"
#include "modules/rk_aiq_isp33_modules.h"
#include "modules/rk_aiq_isp35_modules.h"
#include "modules/rk_aiq_isp351s_modules.h"
#include "xcore/base/xcam_log.h"

#define ISP2X_WBGAIN_FIXSCALE_BIT 8
#define ISP3X_WBGAIN_INTSCALE_BIT 8
#define ISP3X_WBGAIN1_INTSCALE_BIT 9

#if (RKAIQ_HAVE_BAYERTNR_V51)
static void convertAiqBtnrToIsp351sParams(AiqIspParamsCvt_t* pCvt, aiq_params_base_t* pBase) {
    if (pBase->en) {
        // bayer3dnr enable  bayer2dnr must enable at the same time
        pCvt->isp_params.isp_cfg->module_ens |= ISP3X_MODULE_BAY3D;
        pCvt->isp_params.isp_cfg->module_en_update |= ISP3X_MODULE_BAY3D;
        pCvt->isp_params.isp_cfg->module_cfg_update |= ISP3X_MODULE_BAY3D;
    } else {
        // tnr can't open/close in runtime if not enable in first frame
        pCvt->isp_params.isp_cfg->module_ens &= ~ISP3X_MODULE_BAY3D;
        pCvt->isp_params.isp_cfg->module_en_update |= ISP3X_MODULE_BAY3D;
        pCvt->isp_params.isp_cfg->others.bay3d_cfg.transf_bypass_en = 1;
        pCvt->isp_params.isp_cfg->module_cfg_update |= ISP3X_MODULE_BAY3D;
        return;
    }

    pCvt->isp_params.isp_cfg->others.bay3d_cfg.bypass_en = pBase->bypass;
    pCvt->mBtnrInfo.btnr_attrib = pCvt->btnr_attrib;
    pCvt->mBtnrInfo.btnr_cis_info = pCvt->btnr_cis_info;
    pCvt->mBtnrInfo._working_mode = pCvt->_working_mode;
    rk_aiq_btnr51_params_cvt(pBase->_data, &pCvt->isp_params, &pCvt->mCommonCvtInfo, &pCvt->mBtnrInfo, &pCvt->mergeLuma2Wgt);
}

#endif

#if RKAIQ_HAVE_GIC_V40
static void convertAiqGicToIsp351sParams(AiqIspParamsCvt_t* pCvt, aiq_params_base_t* pBase) {
    if (pBase->en) {
        pCvt->isp_params.isp_cfg->module_en_update |= ISP3X_MODULE_GIC;
        pCvt->isp_params.isp_cfg->module_ens |= ISP3X_MODULE_GIC;
        pCvt->isp_params.isp_cfg->module_cfg_update |= ISP3X_MODULE_GIC;
    } else {
        pCvt->isp_params.isp_cfg->module_en_update |= ISP3X_MODULE_GIC;
        pCvt->isp_params.isp_cfg->module_ens &= ~ISP3X_MODULE_GIC;
        pCvt->isp_params.isp_cfg->module_cfg_update &= ~ISP3X_MODULE_GIC;
        return;
    }

    pCvt->isp_params.isp_cfg->others.gic_cfg.bypass_en = pBase->bypass;
    rk_aiq_gic40_params_cvt(pBase->_data, &pCvt->isp_params.isp_cfg->others.gic_cfg, &pCvt->mBtnrInfo);
}
#endif

#if RKAIQ_HAVE_CAC_V30
static void convertAiqCacToIsp351sParams(AiqIspParamsCvt_t* pCvt, aiq_params_base_t* pBase,
                                       struct isp351s_isp_params_cfg* isp_cfg,
                                       struct isp351s_isp_params_cfg* isp_cfg_right,
                                       bool is_multi_isp) {
    if (pBase->en) {
        isp_cfg->module_en_update |= ISP3X_MODULE_CAC;
        isp_cfg->module_ens |= ISP3X_MODULE_CAC;
        isp_cfg->module_cfg_update |= ISP3X_MODULE_CAC;
    } else {
        isp_cfg->module_en_update |= (ISP3X_MODULE_CAC);
        isp_cfg->module_ens &= ~(ISP3X_MODULE_CAC);
        isp_cfg->module_cfg_update &= ~(ISP3X_MODULE_CAC);
        return;
    }

    isp_cfg->others.cac_cfg.bypass_en = pBase->bypass;
    if(is_multi_isp)
        isp_cfg_right->others.cac_cfg.bypass_en = pBase->bypass;
    rk_aiq_cac30_params_cvt(pBase->_data, &isp_cfg->others.cac_cfg, &isp_cfg_right->others.cac_cfg, is_multi_isp, &pCvt->mCommonCvtInfo);
}
#endif
void convertAiqExpIspDgainToIsp351sParams(AiqIspParamsCvt_t* pCvt,
                                        AiqSensorExpInfo_t *pSnsExp) {
    RKAiqAecExpInfo_t* ae_exp = &pSnsExp->aecExpInfo;

    struct isp351s_awb_gain_cfg* cfg      = &pCvt->mLatestWbGainCfg;
    struct isp351s_awb_gain_cfg* dest_cfg = &pCvt->isp_params.isp_cfg->others.awb_gain_cfg;
    uint16_t max_wb_gain = (1 << (ISP2X_WBGAIN_FIXSCALE_BIT + ISP3X_WBGAIN_INTSCALE_BIT)) - 1;
    uint32_t max_wb1_gain = (1 << (ISP2X_WBGAIN_FIXSCALE_BIT + ISP3X_WBGAIN1_INTSCALE_BIT)) - 1;

    if (pCvt->_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        bool isBtnrLogDomain = (pCvt->mCommonCvtInfo.btnrCfg_pixDomain_mode == btnr_pixLog2Domain_mode);

        if (isBtnrLogDomain) { // apply isdgain to wb0
            float isp_dgain = MAX(1.0f, ae_exp->LinearExp.exp_real_params.isp_dgain);

            if (fabs(isp_dgain - pCvt->mLatestIspDgain) < FLT_EPSILON &&
                    fabs(isp_dgain - 1.0f) < FLT_EPSILON)
                return;

            pCvt->mLatestIspDgain = isp_dgain;

            dest_cfg->gain0_r     = MIN(cfg->gain0_r * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain0_gr = MIN(cfg->gain0_gr * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain0_gb = MIN(cfg->gain0_gb * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain0_b    = MIN(cfg->gain0_b * isp_dgain + 0.5, max_wb_gain);

            dest_cfg->gain1_r     = MIN(cfg->gain1_r * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain1_gr = MIN(cfg->gain1_gr * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain1_gb = MIN(cfg->gain1_gb * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain1_b    = MIN(cfg->gain1_b * isp_dgain + 0.5, max_wb_gain);

            dest_cfg->gain2_r     = MIN(cfg->gain2_r * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain2_gr = MIN(cfg->gain2_gr * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain2_gb = MIN(cfg->gain2_gb * isp_dgain + 0.5, max_wb_gain);
            dest_cfg->gain2_b    = MIN(cfg->gain2_b * isp_dgain + 0.5, max_wb_gain);

            dest_cfg->awb1_gain_r  = cfg->awb1_gain_r;
            dest_cfg->awb1_gain_gr = cfg->awb1_gain_gr;
            dest_cfg->awb1_gain_b  = cfg->awb1_gain_b;
            dest_cfg->awb1_gain_gb = cfg->awb1_gain_gb;

            pCvt->isp_params.isp_cfg->module_cfg_update |= ISP39_MODULE_AWB_GAIN;
        } else {
            float isp_dgain = MAX(1.0f, ae_exp->LinearExp.exp_real_params.isp_dgain);
            bool isBtnrPreBaseMode = pCvt->mCommonCvtInfo.btnr_en && 
                (pCvt->mCommonCvtInfo.sw_btnrT_outFrmBase_mode == btnr_preBaseOut_mode);
            if (isBtnrPreBaseMode) // isPreFrameIspdgain = isBtnrPreBaseMode && ispdgainWb1
                isp_dgain = MAX(1.0f, pSnsExp->preFrameIspdgain[0]);

            if (fabs(isp_dgain - pCvt->mLatestIspDgain) < FLT_EPSILON &&
                    fabs(isp_dgain - 1.0f) < FLT_EPSILON)
                return;
            if (isp_dgain >= 64.0f) {
                LOGE_CAMHW("isp_dgain:%f is over the maxium 64", isp_dgain);
                isp_dgain = 64.0;
            }

            pCvt->mLatestIspDgain = isp_dgain;

            dest_cfg->gain0_r     = cfg->gain0_r;
            dest_cfg->gain0_gr = cfg->gain0_gr;
            dest_cfg->gain0_gb = cfg->gain0_gb;
            dest_cfg->gain0_b    = cfg->gain0_b;

            dest_cfg->gain1_r     = cfg->gain1_r;
            dest_cfg->gain1_gr = cfg->gain1_gr;
            dest_cfg->gain1_gb = cfg->gain1_gb;
            dest_cfg->gain1_b    = cfg->gain1_b;

            dest_cfg->gain2_r     = cfg->gain2_r;
            dest_cfg->gain2_gr = cfg->gain2_gr;
            dest_cfg->gain2_gb = cfg->gain2_gb;
            dest_cfg->gain2_b    = cfg->gain2_b;

            dest_cfg->awb1_gain_r  = MIN(cfg->awb1_gain_r * isp_dgain + 0.5, max_wb1_gain);
            dest_cfg->awb1_gain_gr = MIN(cfg->awb1_gain_gr * isp_dgain + 0.5, max_wb1_gain);
            dest_cfg->awb1_gain_b  = MIN(cfg->awb1_gain_b * isp_dgain + 0.5, max_wb1_gain);
            dest_cfg->awb1_gain_gb = MIN(cfg->awb1_gain_gb * isp_dgain + 0.5, max_wb1_gain);

            pCvt->isp_params.isp_cfg->module_cfg_update |= ISP39_MODULE_AWB_GAIN;
        }
    } else {
        float isp_dgain0 = MAX(1.0f, ae_exp->HdrExp[0].exp_real_params.isp_dgain);
        float isp_dgain1 = MAX(1.0f, ae_exp->HdrExp[1].exp_real_params.isp_dgain);
        float isp_dgain2 = MAX(1.0f, ae_exp->HdrExp[2].exp_real_params.isp_dgain);

        float isp_dgain = isp_dgain0 + isp_dgain1 + isp_dgain2;
        if (fabs(isp_dgain - pCvt->mLatestIspDgain) < FLT_EPSILON &&
                fabs(isp_dgain - 3.0f) < FLT_EPSILON)
            return;
        pCvt->mLatestIspDgain = isp_dgain;

        if (pCvt->mCommonCvtInfo._airms_en || RK_AIQ_HDR_IS_SENSOR_BUILTIN(pCvt->_working_mode)) {
            uint16_t fixedGain1x = 1 << ISP2X_WBGAIN_FIXSCALE_BIT;

            dest_cfg->gain0_r     = MIN(fixedGain1x * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_gr = MIN(fixedGain1x * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_gb = MIN(fixedGain1x * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_b    = MIN(fixedGain1x * isp_dgain0 + 0.5, max_wb_gain);

            dest_cfg->gain1_r     = MIN(fixedGain1x * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_gr = MIN(fixedGain1x * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_gb = MIN(fixedGain1x * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_b    = MIN(fixedGain1x * isp_dgain1 + 0.5, max_wb_gain);

            dest_cfg->gain2_r     = MIN(fixedGain1x * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_gr = MIN(fixedGain1x * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_gb = MIN(fixedGain1x * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_b    = MIN(fixedGain1x * isp_dgain2 + 0.5, max_wb_gain);

            dest_cfg->awb1_gain_r  = fixedGain1x;
            dest_cfg->awb1_gain_gr = fixedGain1x;
            dest_cfg->awb1_gain_b  = fixedGain1x;
            dest_cfg->awb1_gain_gb = fixedGain1x;
        } else {
            dest_cfg->gain0_r     = MIN(cfg->gain0_r * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_gr = MIN(cfg->gain0_gr * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_gb = MIN(cfg->gain0_gb * isp_dgain0 + 0.5, max_wb_gain);
            dest_cfg->gain0_b    = MIN(cfg->gain0_b * isp_dgain0 + 0.5, max_wb_gain);

            dest_cfg->gain1_r     = MIN(cfg->gain1_r * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_gr = MIN(cfg->gain1_gr * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_gb = MIN(cfg->gain1_gb * isp_dgain1 + 0.5, max_wb_gain);
            dest_cfg->gain1_b    = MIN(cfg->gain1_b * isp_dgain1 + 0.5, max_wb_gain);

            dest_cfg->gain2_r     = MIN(cfg->gain2_r * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_gr = MIN(cfg->gain2_gr * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_gb = MIN(cfg->gain2_gb * isp_dgain2 + 0.5, max_wb_gain);
            dest_cfg->gain2_b    = MIN(cfg->gain2_b * isp_dgain2 + 0.5, max_wb_gain);

            dest_cfg->awb1_gain_r  = cfg->awb1_gain_r;
            dest_cfg->awb1_gain_gr = cfg->awb1_gain_gr;
            dest_cfg->awb1_gain_b  = cfg->awb1_gain_b;
            dest_cfg->awb1_gain_gb = cfg->awb1_gain_gb;
        }

        pCvt->isp_params.isp_cfg->module_cfg_update |= ISP39_MODULE_AWB_GAIN;
    }
}

#define CVT_INFO(_type, _func) \
    [_type] = {                \
        .type = _type,         \
        .name = #_type,        \
        .func = _func,         \
    }

struct params_cvt_info_isp351s {
    int32_t type;
    const char* const name;
    void (*func)(AiqIspParamsCvt_t* pCvt, aiq_params_base_t* pBase);
};

static const struct params_cvt_info_isp351s params_cvts_isp351s[] = {
    CVT_INFO(RESULT_TYPE_DEBAYER_PARAM, convertAiqDmToIsp35Params),
    CVT_INFO(RESULT_TYPE_AESTATS_PARAM, convertAiqAeToIsp39Params),
    CVT_INFO(RESULT_TYPE_AWB_PARAM, convertAiqAwbToIsp35Params),
    CVT_INFO(RESULT_TYPE_AWBGAIN_PARAM, convertAiqAwbGainToIsp35Params),
    CVT_INFO(RESULT_TYPE_CCM_PARAM, convertAiqCcmToIsp39Params),
#if RKAIQ_HAVE_AF_V33 || RKAIQ_ONLY_AF_STATS_V33
    CVT_INFO(RESULT_TYPE_AF_PARAM, convertAiqAfToIsp39Params),
#endif
#if (RKAIQ_HAVE_BAYERTNR_V51)
    CVT_INFO(RESULT_TYPE_TNR_PARAM, convertAiqBtnrToIsp351sParams),
#endif
#if RKAIQ_HAVE_YNR_V41
    CVT_INFO(RESULT_TYPE_YNR_PARAM, convertAiqYnrToIsp35Params),
#endif
#if (RKAIQ_HAVE_CNR_V36)
    CVT_INFO(RESULT_TYPE_UVNR_PARAM, convertAiqCnrToIsp35Params),
#endif
    CVT_INFO(RESULT_TYPE_MERGE_PARAM, convertAiqMergeToIsp35Params),
    CVT_INFO(RESULT_TYPE_HISTEQ_PARAM, convertAiqHisteqToIsp33Params),
    CVT_INFO(RESULT_TYPE_ENH_PARAM, convertAiqEnhToIsp33Params),
#if (RKAIQ_HAVE_SHARP_V41)
    CVT_INFO(RESULT_TYPE_TEXEST_PARAM, convertAiqTexEstToIsp35Params),
    CVT_INFO(RESULT_TYPE_SHARPEN_PARAM, convertAiqSharpToIsp35Params),
#endif
    CVT_INFO(RESULT_TYPE_HSV_PARAM, convertAiqHsvToIsp35Params),
    CVT_INFO(RESULT_TYPE_BLC_PARAM, convertAiqBlcToIsp39Params),
    CVT_INFO(RESULT_TYPE_CSM_PARAM, convertAiqCsmToIsp39Params),
    CVT_INFO(RESULT_TYPE_DPCC_PARAM, convertAiqDpccToIsp39Params),
    CVT_INFO(RESULT_TYPE_AGAMMA_PARAM, convertAiqGammaToIsp39Params),
    CVT_INFO(RESULT_TYPE_LSC_PARAM, convertAiqLscToIsp39Params),
    CVT_INFO(RESULT_TYPE_DRC_PARAM, convertAiqDrcToIsp35Params),
#if RKAIQ_HAVE_GIC_V40
    CVT_INFO(RESULT_TYPE_GIC_PARAM, convertAiqGicToIsp351sParams),
#endif
    CVT_INFO(RESULT_TYPE_CGC_PARAM, convertAiqCgcToIsp21Params),
    CVT_INFO(RESULT_TYPE_CP_PARAM, convertAiqCpToIsp20Params),
    CVT_INFO(RESULT_TYPE_IE_PARAM, convertAiqIeToIsp20Params),
    CVT_INFO(RESULT_TYPE_GAIN_PARAM, convertAiqGainToIsp3xParams),
#if RKAIQ_HAVE_AIBNR
    CVT_INFO(RESULT_TYPE_AIBNR_PARAM, convertAiqAibnrToIsp35Params),
#endif
#if RKAIQ_HAVE_AIYNR
    CVT_INFO(RESULT_TYPE_AIYNR_PARAM, convertAiqAiynrToIsp35Params),
#endif
};

static bool params_cvt_is_known(int32_t type) {
    if (type >= RESULT_TYPE_MAX_PARAM) return false;
    return params_cvts_isp351s[type].type == type;
}

bool Convert3aResultsToIsp351sCfg(AiqIspParamsCvt_t* pCvt, aiq_params_base_t* pBase,
                                void* isp_cfg_p, bool is_multi_isp) {
    struct isp351s_isp_params_cfg* isp_cfg = (struct isp351s_isp_params_cfg*)isp_cfg_p;

    switch (pBase->type) {
    case RESULT_TYPE_EXPOSURE_PARAM: {
        AiqSensorExpInfo_t* exp_info = (AiqSensorExpInfo_t*)pBase;
        convertAiqExpIspDgainToIsp351sParams(pCvt, exp_info);
    }
    break;
    case RESULT_TYPE_CAC_PARAM: {
#if RKAIQ_HAVE_CAC_V30
        struct isp351s_isp_params_cfg* isp_cfg_right = isp_cfg + 1;
        convertAiqCacToIsp351sParams(pCvt, pBase, isp_cfg, isp_cfg_right, is_multi_isp);
#endif
    }
    break;
    case RESULT_TYPE_LDC_PARAM:
#if RKAIQ_HAVE_LDC
        convertAiqAldcToIsp39Params(pCvt, pBase, is_multi_isp);
#endif
    break;
    default:
        if (params_cvt_is_known(pBase->type)) {
            const struct params_cvt_info_isp351s* info = &params_cvts_isp351s[pBase->type];
            info->func(pCvt, pBase);
            return true;
        } else {
            LOGE_CAMHW("unknown param type: 0x%x!", pBase->type);
            return false;
        }
    }
    return true;
}

#if RKAIQ_HAVE_DUMPSYS
static void UpdateModEn(struct isp351s_isp_params_cfg* src, struct isp351s_isp_params_cfg* dst) {
    LOG1_CAMHW("%s seq:%d, module_en_update:0x%llx\n", __func__, src->frame_id,
               src->module_en_update);

    for (int i = 0; i <= ISP2X_ID_MAX; i++) {
        if (src->module_en_update & BIT_ULL(i)) dst->module_ens |= src->module_ens & (1LL << i);
    }
}

static void UpdateMeasModCfg(struct isp351s_isp_params_cfg* src, struct isp351s_isp_params_cfg* dst) {
    u64 module_cfg_update = src->module_cfg_update;

    LOG1_CAMHW("%s seq:%d, module_cfg_update:0x%llx\n", __func__, src->frame_id,
               src->module_cfg_update);

    if (module_cfg_update & ISP35_MODULE_RAWAE0) dst->meas.rawae0 = src->meas.rawae0;

    if (module_cfg_update & ISP35_MODULE_RAWAE3) dst->meas.rawae3 = src->meas.rawae3;

    if (module_cfg_update & ISP35_MODULE_RAWHIST0) dst->meas.rawhist0 = src->meas.rawhist0;

    if (module_cfg_update & ISP35_MODULE_RAWHIST3) dst->meas.rawhist3 = src->meas.rawhist3;

    if (module_cfg_update & ISP35_MODULE_RAWAWB) dst->meas.rawawb = src->meas.rawawb;
}

static void UpdateOthersModCfg(struct isp351s_isp_params_cfg* src, struct isp351s_isp_params_cfg* dst) {
    u64 module_cfg_update = src->module_cfg_update;

    LOG1_CAMHW("%s seq:%d, module_cfg_update:0x%llx\n", __func__, src->frame_id,
               src->module_cfg_update);

    if (module_cfg_update & ISP35_MODULE_LSC) dst->others.lsc_cfg = src->others.lsc_cfg;

    if (module_cfg_update & ISP35_MODULE_DPCC) dst->others.dpcc_cfg = src->others.dpcc_cfg;

    if (module_cfg_update & ISP35_MODULE_BLS) dst->others.bls_cfg = src->others.bls_cfg;

    if (module_cfg_update & ISP35_MODULE_AWB_GAIN)
        dst->others.awb_gain_cfg = src->others.awb_gain_cfg;

    if (module_cfg_update & ISP35_MODULE_DEBAYER) dst->others.debayer_cfg = src->others.debayer_cfg;

    if (module_cfg_update & ISP35_MODULE_CCM) dst->others.ccm_cfg = src->others.ccm_cfg;

    if (module_cfg_update & ISP35_MODULE_GOC) dst->others.gammaout_cfg = src->others.gammaout_cfg;

    if (module_cfg_update & ISP35_MODULE_CSM) dst->others.csm_cfg = src->others.csm_cfg;

    if (module_cfg_update & ISP35_MODULE_CGC) dst->others.cgc_cfg = src->others.cgc_cfg;

    if (module_cfg_update & ISP35_MODULE_CPROC) dst->others.cproc_cfg = src->others.cproc_cfg;

    if (module_cfg_update & ISP35_MODULE_HDRMGE) dst->others.hdrmge_cfg = src->others.hdrmge_cfg;

    if (module_cfg_update & ISP35_MODULE_DRC) dst->others.drc_cfg = src->others.drc_cfg;

    if (module_cfg_update & ISP35_MODULE_GIC) dst->others.gic_cfg = src->others.gic_cfg;

    if (module_cfg_update & ISP35_MODULE_ENH) dst->others.enh_cfg = src->others.enh_cfg;

    if (module_cfg_update & ISP35_MODULE_HIST) dst->others.hist_cfg = src->others.hist_cfg;

    if (module_cfg_update & ISP35_MODULE_LDCH) dst->others.ldch_cfg = src->others.ldch_cfg;

    if (module_cfg_update & ISP35_MODULE_YNR) dst->others.ynr_cfg = src->others.ynr_cfg;

    if (module_cfg_update & ISP35_MODULE_CNR) dst->others.cnr_cfg = src->others.cnr_cfg;

    if (module_cfg_update & ISP35_MODULE_SHARP) dst->others.sharp_cfg = src->others.sharp_cfg;

    if (module_cfg_update & ISP35_MODULE_BAY3D) dst->others.bay3d_cfg = src->others.bay3d_cfg;

    if (module_cfg_update & ISP35_MODULE_CAC) dst->others.cac_cfg = src->others.cac_cfg;

    if (module_cfg_update & ISP35_MODULE_GAIN) dst->others.gain_cfg = src->others.gain_cfg;

    if (module_cfg_update & ISP35_MODULE_AI) dst->others.ai_cfg = src->others.ai_cfg;
}

void AiqIspParamsCvt_updIsp351sParams(void* src, void* dst) {
    if (!src || !dst) return;

    struct isp351s_isp_params_cfg* pSrc = (struct isp351s_isp_params_cfg*)src;
    struct isp351s_isp_params_cfg* pDst = (struct isp351s_isp_params_cfg*)dst;

    pDst->frame_id          = pSrc->frame_id;
    pDst->module_en_update  = pSrc->module_en_update;
    pDst->module_cfg_update = pSrc->module_cfg_update;

    UpdateModEn(pSrc, pDst);
    UpdateMeasModCfg(pSrc, pDst);
    UpdateOthersModCfg(pSrc, pDst);
}
#endif
