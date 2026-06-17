#ifndef _RK_AIQ_ISP351S_MODULES_H_
#define _RK_AIQ_ISP351S_MODULES_H_

#include "rk_aiq_isp32_modules.h"

#include "rk_aiq_module_btnr_common.h"
#include "rk_aiq_module_common.h"
#include "common/rk-isp33-config.h"
#include "common/rk-isp35-config.h"
#include "common/rk-isp351s-config.h"

RKAIQ_BEGIN_DECLARE
void rk_aiq_dm25_params_cvt(void* attr, isp_params_t* isp_params, common_cvt_info_t* cvtinfo);
void rk_aiq_btnr51_params_cvt(void* attr, isp_params_t* isp_params, common_cvt_info_t* cvtinfo, btnr_cvt_info_t* pBtnrInfo, mergeLuma2Wgt_t* pMergeLumaWgt);
void rk_aiq_btnr51_l2_params_cvt(void* attr, isp_params_t* isp_params, common_cvt_info_t* cvtinfo, btnr_cvt_info_t* pBtnrInfo, mergeLuma2Wgt_t* pMergeLumaWgt);
void rk_aiq_gic40_params_cvt(void* attr, struct isp351s_gic_cfg* gic_cfg, btnr_cvt_info_t* pBtnrInfo);
RKAIQ_END_DECLARE

#endif

