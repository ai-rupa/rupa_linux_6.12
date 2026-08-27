/*
 * Copyright (c) 2021-2022 Rockchip Eletronics Co., Ltd.
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
 */

#include "rk_aiq_isp33_modules.h"

RKAIQ_BEGIN_DECLARE

#define LIMIT_VALUE(value,max_value,min_value)      (value > max_value? max_value : value < min_value ? min_value : value)

static int ClipFloatValue(float posx, int BitInt, int BitFloat, bool fullMax) {
    int yOutInt    = 0;
    int yOutIntMax = 0;
    int yOutIntMin = 0;
    if (fullMax)
        yOutIntMax = 1 << (BitFloat + BitInt);
    else
        yOutIntMax = (1 << (BitFloat + BitInt)) - 1;

    yOutInt = LIMIT_VALUE((int)(posx * (1 << BitFloat)), yOutIntMax, yOutIntMin);

    return yOutInt;
}

unsigned int ClipIntValue(unsigned int posx, int BitInt, int BitFloat) {
    unsigned int yOutInt    = 0;
    unsigned int yOutIntMax = (1 << (BitFloat + BitInt)) - 1;

    posx <<= BitFloat;
    yOutInt = posx > yOutIntMax ? yOutIntMax : posx;
    return yOutInt;
}

void histeqCreateKernelCoeffs(int radius, int max_radius, float rsigma, int* kernel_coeffs,
                              int fix_bits, int dim) {
    // calcute distance
    int i                        = 0;
    int j                        = 0;
    int k                        = 0;
    double e                     = 2.71828182845905f;
    float gaus_table[10]         = {0.0f};
    float distance_table[10]     = {0.0f};
    float sumTable               = 0.0f;
    float tmp                    = 0.0f;
    float coeffScale_table1D[10] = {1.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
    float coeffScale_table2D[10] = {1.0f, 4.0f, 4.0f, 4.0f, 8.0f, 4.0f, 4.0f, 8.0f, 8.0f, 4.0f};
    int coeffNums                = (radius + 1) * (radius + 2) / 2;
    int coeffNums_max            = (max_radius + 1) * (max_radius + 2) / 2;
    if (dim == 1) {
        coeffNums = radius + 1;
    }

    // calc distance
    if (dim == 2) {
        for (i = 0; i <= radius; i++) {
            for (j = 0; j <= i; j++) {
                distance_table[i * (i + 1) / 2 + j] = pow(i, 2.0f) + pow(j, 2.0f);
            }
        }
    } else {
        for (i = 0; i <= radius; i++) {
            distance_table[i] = pow(i, 2.0f);
        }
    }

    // calc coeff
    for (k = 0; k < coeffNums; k++) {
        tmp           = pow(e, -distance_table[k] / 2.0f / rsigma / rsigma);
        gaus_table[k] = tmp;
        if (dim == 2) {
            sumTable += coeffScale_table2D[k] * gaus_table[k];
        } else {
            sumTable += coeffScale_table1D[k] * gaus_table[k];
        }
    }

    for (k = 0; k < coeffNums_max; k++) {
        gaus_table[k]    = gaus_table[k] / sumTable;
        kernel_coeffs[k] = ROUND_F(gaus_table[k] * (1 << fix_bits));
    }
}

#define AHIST_INT_EVEN_NUM_COEF (2)
#if defined(ISP_HW_V351s)
#define AHIST_THUMB_COLS_MAX  (20)
#define AHIST_THUMB_ROWS_MAX  (16)
#define HIST_BLOCK_HEIGHT_MIN (18)
#define HIST_BLOCK_HEIGHT_MAX (768)
#define HIST_BLOCK_WIDTH_MIN  (66)
#define HIST_BLOCK_WIDTH_MAX  (1024)
#else
#define AHIST_THUMB_COLS_MAX                        (10)
#define AHIST_THUMB_ROWS_MAX                        (8)
#define HIST_BLOCK_HEIGHT_MIN                       (64)
#define HIST_BLOCK_HEIGHT_MAX                       (876)
#define HIST_BLOCK_WIDTH_MIN                        (66)
#define HIST_BLOCK_WIDTH_MAX                        (1168)
#endif
#define AHIST_THUMB_COLS_MIN    (4)
#define AHIST_THUMB_ROWS_MIN    (4)

void rk_aiq_histeq30_params_cvt(void* attr, isp_params_t* isp_params, common_cvt_info_t* cvtinfo) {
#if defined(ISP_HW_V351s)
    struct isp351s_hist_cfg* pFix = &isp_params->isp_cfg->others.hist_cfg;
#else
    struct isp33_hist_cfg *pFix = &isp_params->isp_cfg->others.hist_cfg;
#endif
    histeq_param_t* histeq_attrib = (histeq_param_t*) attr;
    histeq_params_dyn_t *pdyn = &histeq_attrib->dyn;
    histeq_params_static_t* psta = &histeq_attrib->sta;
    int tmp;
    int rows = cvtinfo->rawHeight;
    int cols = cvtinfo->rawWidth;

    pFix->mem_mode = 0;  
    /* HF_STAT */
    pFix->count_scale  = ClipFloatValue(pdyn->statsHist.sw_histCfg_noiseCount_scale, 3, 2, false);
    pFix->count_offset = ClipIntValue(pdyn->statsHist.hw_histCfg_noiseCount_offset, 8, 0);
    pFix->count_min_limit =
        ClipFloatValue(pdyn->statsHist.sw_histCfg_countWgt_minLimit, 0, 8, true);
    /* THUMB_SIZE */
    tmp = psta->statsHist.hw_histCfg_zonesVert_num / AHIST_INT_EVEN_NUM_COEF *
          AHIST_INT_EVEN_NUM_COEF;
    pFix->thumb_row = LIMIT_VALUE(tmp, AHIST_THUMB_ROWS_MAX, AHIST_THUMB_ROWS_MIN);;
    tmp             = psta->statsHist.hw_histCfg_zonesHorz_num / AHIST_INT_EVEN_NUM_COEF *
          AHIST_INT_EVEN_NUM_COEF;
    pFix->thumb_col = LIMIT_VALUE(tmp, AHIST_THUMB_COLS_MAX, AHIST_THUMB_COLS_MIN);
    /* BLOCK_SIZE */
    tmp           = rows / pFix->thumb_row;
    tmp           = tmp % 2 != 0 ? (tmp + 1) : tmp;
    pFix->blk_het = LIMIT_VALUE(tmp, HIST_BLOCK_HEIGHT_MAX, HIST_BLOCK_HEIGHT_MIN);
    tmp           = cols / pFix->thumb_col;
    tmp           = tmp % 2 != 0 ? (tmp + 1) : tmp;
    pFix->blk_wid = LIMIT_VALUE(tmp, HIST_BLOCK_WIDTH_MAX, HIST_BLOCK_WIDTH_MIN);
    /* MAP0 */
    pFix->merge_alpha = ClipFloatValue(pdyn->mapHist.sw_histT_mapUsrCfgStrg_alpha, 0, 8, true);
    pFix->user_set    = ClipFloatValue(pdyn->mapHist.sw_histT_mapUsrCfg_strg, 0, 8, true);
    /* MAP1 */
    pFix->map_count_scale = ClipFloatValue(pdyn->mapHist.sw_histT_mapStats_scale, 0, 8, true);
    pFix->gain_ref_wgt    = ClipFloatValue(pdyn->mapHist_idx.hw_histT_shpOut_alpha, 0, 6, false);
    /* IIR */
    pFix->flt_cur_wgt   = ClipIntValue(pdyn->mapHistIIR.hw_histT_iirFrm_maxLimit, 4, 0);
    tmp                 = LIMIT_VALUE(pdyn->mapHistIIR.sw_histT_iirSgm_scale, 255, 1);
    float tmp_float = 1.0f / ((float)tmp);
    pFix->flt_inv_sigma = ClipFloatValue(tmp_float, 0, 8, true);
    /* POS_ALPHA */
    /* NEG_ALPHA */
    if (pdyn->fusion.hw_histT_glbWgt_mode == histeq_fusion_glbWgt_mode) {
        for (int i = 0; i < ISP33_HIST_ALPHA_NUM; ++i) {
            pFix->pos_alpha[i] = ClipFloatValue(pdyn->fusion.sw_histT_glbWgt_posVal, 2, 6, false);
            pFix->neg_alpha[i] = ClipFloatValue(pdyn->fusion.sw_histT_glbWgt_negVal, 2, 6, false);
        }
    } else if (pdyn->fusion.hw_histT_glbWgt_mode == histeq_fusion_luma2Wgt_mode) {
        for (int i = 0; i < ISP33_HIST_ALPHA_NUM; ++i) {
            pFix->pos_alpha[i] =
                ClipFloatValue(pdyn->fusion.sw_histT_luma2Wgt_posVal[i], 2, 6, false);
            pFix->neg_alpha[i] =
                ClipFloatValue(pdyn->fusion.sw_histT_luma2Wgt_negVal[i], 2, 6, false);
        }
    }
    /* STAB */
    // pFix->stab_frame_cnt0 = pdyn->stab_frame_cnt0;
    // pFix->stab_frame_cnt1 = pdyn->stab_frame_cnt1;
    /* UV_SCL */
    pFix->saturate_scale = ClipFloatValue(pdyn->chroma.sw_histT_saturate_scale, 3, 5, false);
    pFix->iir_wr = 0;

#if defined(ISP_HW_V351s)
    pFix->filter_en  = psta->statsFltCfg.hw_histCfg_flt_en;
    pFix->flt_sigma  = ClipFloatValue(psta->statsFltCfg.hw_histCfg_cdfFlt_sigma, 4, 0, true);
    pFix->flt_offset = ClipFloatValue(psta->statsFltCfg.hw_histCfg_cdfFlt_offset, 4, 0, true);
    pFix->gain_max   = ClipFloatValue(psta->statsFltCfg.hw_histCfg_boost_max, 4, 6, true);
    pFix->gain_min   = ClipFloatValue(psta->statsFltCfg.hw_histCfg_suppress_min, 1, 6, true);
    int kernel_coeffs[10];
    histeqCreateKernelCoeffs(3, 3, psta->statsFltCfg.hw_histCfg_cdfFlt_coefMax, kernel_coeffs, 8,
                             1);
    pFix->coef_max0 = kernel_coeffs[0] > 255 ? 255 : kernel_coeffs[0];
    pFix->coef_max1 = kernel_coeffs[1] > 255 ? 255 : kernel_coeffs[1];
    pFix->coef_max2 = kernel_coeffs[2] > 255 ? 255 : kernel_coeffs[2];
    pFix->coef_max3 = kernel_coeffs[3] > 255 ? 255 : kernel_coeffs[3];
    histeqCreateKernelCoeffs(3, 3, psta->statsFltCfg.hw_histCfg_cdfFlt_coefMin, kernel_coeffs, 8,
                             1);
    pFix->coef_min0 = kernel_coeffs[0] > 255 ? 255 : kernel_coeffs[0];
    pFix->coef_min1 = kernel_coeffs[1] > 255 ? 255 : kernel_coeffs[1];
    pFix->coef_min2 = kernel_coeffs[2] > 255 ? 255 : kernel_coeffs[2];
    pFix->coef_min3 = kernel_coeffs[3] > 255 ? 255 : kernel_coeffs[3];
#endif
}

RKAIQ_END_DECLARE
