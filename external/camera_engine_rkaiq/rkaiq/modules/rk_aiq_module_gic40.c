#include "rk_aiq_isp351s_modules.h"
#include <math.h> 

#define 	RKGIC_V40_LUMA_POINT_NUM 			12
// gic
#define		RKGIC_V40_COEFF_INV_FIX_BITS 		12
#define		RKGIC_V40_GR_GB_COEFF_FIX_BITS		8
#define 	RKGIC_V40_ALPHA_FIX_BITS 			10

// rb filter
#define 	RKGIC_V40_GAUS_FLT_WGT_FIX_BIT		6
#define 	RKGIC_V40_DIV_SIGMA_FIX_BIT 		28
#define		RKGIC_V40_BF_WGT_OFFSET_FIX_BITS 	10
#define		RKGIC_V40_BF_WGT_SCALE_FIX_BITS 	10
#define 	RKGIC_V40_BF_WGT_FIX_BIT 			10
#define 	RKGIC_V40_NOISE_BAL_SCALE_FIX_BITS 	10
#define 	RKGIC_V40_RB_FLT_ALPHA_FIX_BIT		6

void rk_aiq_gic40_params_cvt(void* attr, struct isp351s_gic_cfg* gic_cfg, btnr_cvt_info_t* pBtnrInfo)
{
    int i, tmp;
    struct isp351s_gic_cfg *pFix = gic_cfg;
    gic_param_t *gic_param = (gic_param_t *) attr;
    gic_params_dyn_t* pdyn = &gic_param->dyn;
    gic_params_static_t* psta = &gic_param->sta;

	// gic
    pFix->loflt_gr_coef0 = pdyn->gic.hw_gic_loFltGr_coeff[0];
    pFix->loflt_gr_coef1 = pdyn->gic.hw_gic_loFltGr_coeff[1];
    pFix->loflt_gr_coef2 = pdyn->gic.hw_gic_loFltGr_coeff[2];
    pFix->loflt_gr_coef3 = pdyn->gic.hw_gic_loFltGr_coeff[3];

    pFix->loflt_gb_coef0 = pdyn->gic.hw_gic_loFltGb_coeff[0];
    pFix->loflt_gb_coef1 = pdyn->gic.hw_gic_loFltGb_coeff[1];

	int	sumLoFltGrCoeff = 4 * pFix->loflt_gr_coef0 + 2 * pFix->loflt_gr_coef1 +\
                          2 * pFix->loflt_gr_coef2 + pFix->loflt_gr_coef3;
	int	sumLoFltGbCoeff = 2 * pFix->loflt_gb_coef0 + 2 * pFix->loflt_gb_coef1;
	//gic_v40.hw_gic_sumFltGCoeff_inv 		= ROUND_F(1.0f / sumLoFltGrCoeff * (1 << RKGIC_V40_COEFF_INV_FIX_BITS));

	for(int k = 0; k < 4; k ++)
	{
		// coeff < 256, sum == 256
		float tmp							= (float)pdyn->gic.hw_gic_loFltGr_coeff[k] / sumLoFltGrCoeff;
		tmp									= MIN(tmp, (1 << (RKGIC_V40_GR_GB_COEFF_FIX_BITS - 1)));
        if (k == 0)
            pFix->loflt_gr_coef0 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
        else if (k == 1)
            pFix->loflt_gr_coef1 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
        else if (k == 2)
            pFix->loflt_gr_coef2 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
        else if (k == 3)
            pFix->loflt_gr_coef3 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
	}

	for(int k = 0; k < 2; k ++)
	{
		float tmp							= (float)pdyn->gic.hw_gic_loFltGb_coeff[k] / sumLoFltGbCoeff;
        if (k == 0)
            pFix->loflt_gb_coef0 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
        else if (k == 1)
            pFix->loflt_gb_coef1 = ROUND_F(tmp * (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS));
	}

	sumLoFltGrCoeff = 4 * pFix->loflt_gr_coef0 + 2 * pFix->loflt_gr_coef1 +\
                      2 * pFix->loflt_gr_coef2 + pFix->loflt_gr_coef3;
	int offset 	= (1 << RKGIC_V40_GR_GB_COEFF_FIX_BITS) - sumLoFltGrCoeff;
	pFix->loflt_gr_coef3 += offset;
	
	for (int k = 0; k < RKGIC_V40_LUMA_POINT_NUM - 1; k++)
	{
		pFix->curve_idx[k] = LOG2(psta->gic.sw_gic_curve_idx[k + 1] - psta->gic.sw_gic_curve_idx[k]);
	}

	for(int k = 0; k < RKGIC_V40_LUMA_POINT_NUM; k ++)
	{
		pFix->luma2soft_thred[k] = pdyn->gic.hw_gic_luma2SofThd_thred[k];
	}

	// fusion wgt
	for(int k = 0; k < RKGIC_V40_LUMA_POINT_NUM; k ++)
	{
		pFix->fusion_min_thred[k] = pdyn->gic.hw_gic_fusionWgt_minThred[k];
	}
    pFix->fusion_wgt_slope = pdyn->gic.hw_gic_fusionWgt_slope * (1 << RKGIC_V40_ALPHA_FIX_BITS);

	// RB filter
	pFix->rb_flt_en = psta->RBFilter.hw_gic_RBFilter_en;
	pFix->gaus_alpha = ROUND_F(pdyn->RBFilter.gausFlt.hw_gic_gausFiltOut_alpha * (1 << RKGIC_V40_GAUS_FLT_WGT_FIX_BIT));
	pFix->rat2wgt_min_thed = ROUND_F(pdyn->RBFilter.rat2MaxWgtThred.hw_gic_rat2MinWgt_minThred * (1 << RKGIC_V40_BF_WGT_OFFSET_FIX_BITS));
	float bfFltWgt_slope = 1.0 / MAX(pdyn->RBFilter.rat2MaxWgtThred.hw_gic_rat2MaxWgt_maxThred - pdyn->RBFilter.rat2MaxWgtThred.hw_gic_rat2MinWgt_minThred, 0.1);
	pFix->bf_wet_scale= ROUND_F(bfFltWgt_slope * (1 << RKGIC_V40_BF_WGT_SCALE_FIX_BITS));
	pFix->bf_out_alpha = ROUND_F(pdyn->RBFilter.hw_gic_RBFiltOut_alpha * (1 << RKGIC_V40_RB_FLT_ALPHA_FIX_BIT));

	// sigma inv
	for(int k = 0; k < RKGIC_V40_LUMA_POINT_NUM; k ++)
    {
		int RSigma 							= pdyn->RBFilter.rsgma.hw_gic_RFlt_sigma[k] * pdyn->RBFilter.rsgma.hw_gic_rgeSgm_scale;
        if (RSigma == 0) {
            pFix->r_sigma_inv[k] = (1 << RKGIC_V40_DIV_SIGMA_FIX_BIT) - 1;
        } else {
            RSigma								= MAX(RSigma, 1);
            pFix->r_sigma_inv[k]	= 1.0f / (float)RSigma * (1 << RKGIC_V40_DIV_SIGMA_FIX_BIT);
        }

		int BSigma 							= pdyn->RBFilter.rsgma.hw_gic_BFlt_sigma[k] * pdyn->RBFilter.rsgma.hw_gic_rgeSgm_scale;
        if (BSigma == 0) {
            pFix->b_sigma_inv[k] = (1 << RKGIC_V40_DIV_SIGMA_FIX_BIT) - 1;
        } else {
            BSigma								= MAX(BSigma, 1);
            pFix->b_sigma_inv[k]	= 1.0f / (float)BSigma * (1 << RKGIC_V40_DIV_SIGMA_FIX_BIT);
        }
    }	
	pFix->bf_wet_min = ROUND_F(pdyn->RBFilter.hw_gic_bfFltWgt_min * (1 << RKGIC_V40_BF_WGT_FIX_BIT));
	
	pFix->r_noise_strg = ROUND_F((1.0 - pdyn->RBFilter.noiseBalStrg.hw_gic_RNoiseBal_strg) * (1 << RKGIC_V40_NOISE_BAL_SCALE_FIX_BITS));
    pFix->b_noise_strg = ROUND_F((1.0 - pdyn->RBFilter.noiseBalStrg.hw_gic_BNoiseBal_strg) * (1 << RKGIC_V40_NOISE_BAL_SCALE_FIX_BITS));

	pFix->bf_coef0 = pdyn->RBFilter.hw_gic_bfFlt_coeff[0];
	pFix->bf_coef1 = pdyn->RBFilter.hw_gic_bfFlt_coeff[1];
	pFix->bf_coef2 = pdyn->RBFilter.hw_gic_bfFlt_coeff[2];

	pFix->gaus_coef0 = pdyn->RBFilter.gausFlt.hw_gic_gausFlt_coeff[0];
	pFix->gaus_coef1 = pdyn->RBFilter.gausFlt.hw_gic_gausFlt_coeff[1];
	pFix->gaus_coef2 = pdyn->RBFilter.gausFlt.hw_gic_gausFlt_coeff[2];

    return;
}
