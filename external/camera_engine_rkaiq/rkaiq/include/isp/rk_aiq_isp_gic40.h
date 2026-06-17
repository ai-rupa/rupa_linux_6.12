/*
 *  Copyright (c) 2023 Rockchip Corporation
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

#ifndef _RK_AIQ_PARAM_GIC40_H_
#define _RK_AIQ_PARAM_GIC40_H_

#define 	RKGIC_V40_LUMA_POINT_NUM 			12

typedef struct gic_rbflt_static_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_RBFilter_en),
        M4_TYPE(bool),
        M4_DEFAULT(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(GIC rb filter enable.\nFreq of use: high))  */
    bool hw_gic_RBFilter_en;
} gic_rbflt_static_t;

typedef struct gic_rbflt_gaus_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_gausFlt_coeff),
        M4_TYPE(u8),
        M4_SIZE_EX(1,3),
        M4_RANGE_EX(0,127),
        M4_DEFAULT(8,16,32),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(RBFilter Gaussian coefficient.Freq of use: low))  */
	uint8_t hw_gic_gausFlt_coeff[3];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_gausFiltOut_alpha),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,1.0),
        M4_DEFAULT(1.0),
        M4_DIGIT_EX(2),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(gaus output fusion weight.Freq of use: high))  */
	float 	hw_gic_gausFiltOut_alpha;
} gic_rbflt_gaus_t;

typedef struct gic_rbflt_rsgma_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_BFlt_sigma),
        M4_TYPE(u16),
        M4_SIZE_EX(1,12),
        M4_RANGE_EX(0, 1024),
        M4_DEFAULT([128,128,128,128,128,128,128,128,128,128,128,128,128]),
        M4_HIDE_EX(0),
        M4_UI_MODULE(curve),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(Blue sigma inverse. \n
        Freq of use: low))  */
	uint16_t hw_gic_BFlt_sigma[RKGIC_V40_LUMA_POINT_NUM];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_RFlt_sigma),
        M4_TYPE(u16),
        M4_SIZE_EX(1,12),
        M4_RANGE_EX(0, 1024),
        M4_DEFAULT([128,128,128,128,128,128,128,128,128,128,128,128,128]),
        M4_HIDE_EX(0),
        M4_UI_MODULE(curve),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(Red sigma inverse. \n
        Freq of use: low))  */
	uint16_t hw_gic_RFlt_sigma[RKGIC_V40_LUMA_POINT_NUM];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_rgeSgm_scale),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,4.0),
        M4_DEFAULT(2.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(TODO .\n
        Freq of use: low))  */
	float 	hw_gic_rgeSgm_scale;
} gic_rbflt_rsgma_t;

typedef struct gic_rbflt_NoiseBalStrg_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_RNoiseBal_strg),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,1.0),
        M4_DEFAULT(2.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(R channel noise balance strength .\n
        Freq of use: low))  */
	float 	hw_gic_RNoiseBal_strg;
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_BNoiseBal_strg),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,1.0),
        M4_DEFAULT(2.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(B channel noise balance strength .\nFreq of use: low))  */
	float 	hw_gic_BNoiseBal_strg;
} gic_rbflt_NoiseBalStrg_t;

typedef struct gic_rbflt_rat2MaxWgtThred_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_rat2MinWgt_minThred),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,15.9),
        M4_DEFAULT(2.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(The scaling ratio of noise statistics in the previous frame, used as a texture threshold.\n
        Lower the value, the lower the noise into noise statistics .\n
        Freq of use: low))  */
	float 	hw_gic_rat2MinWgt_minThred;
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_rat2MaxWgt_maxThred),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,15.9),
        M4_DEFAULT(2.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(The scaling ratio of noise statistics in the previous frame, used as a texture threshold.\n
        Lower the value, the lower the noise into noise statistics .\n
        Freq of use: low))  */
	float 	hw_gic_rat2MaxWgt_maxThred;
} gic_rbflt_rat2MaxWgtThred_t;

typedef struct gic_rbflt_dyn_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(gausFlt),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_gaus_t gausFlt;
    /* M4_GENERIC_DESC(
        M4_ALIAS(rsgma),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_rsgma_t rsgma;
    /* M4_GENERIC_DESC(
        M4_ALIAS(rat2MaxWgtThred),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_rat2MaxWgtThred_t rat2MaxWgtThred;
    /* M4_GENERIC_DESC(
        M4_ALIAS(noiseBalStrg),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_NoiseBalStrg_t noiseBalStrg;
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_bfFlt_coeff),
        M4_TYPE(u8),
        M4_SIZE_EX(1,3),
        M4_RANGE_EX(0,127),
        M4_DEFAULT(8,16,32),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(RBFilter Bilateral distance coefficien.Freq of use: low))  */
	uint8_t hw_gic_bfFlt_coeff[3];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_bfFltWgt_min),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,0.99),
        M4_DEFAULT(0.1),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(The scaling ratio of noise statistics in the previous frame, used as a texture threshold.\n
        Lower the value, the lower the noise into noise statistics .\n
        Freq of use: low))  */
	float	hw_gic_bfFltWgt_min;
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_RBFiltOut_alpha),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,1.0),
        M4_DEFAULT(1.0),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(TODO .\nFreq of use: low))  */
	float 	hw_gic_RBFiltOut_alpha;
} gic_rbflt_dyn_t;

typedef struct gic_gic_static_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(sw_gic_curve_idx),
        M4_TYPE(u16),
        M4_SIZE_EX(1,12),
        M4_RANGE_EX(0, 4096),
        M4_DEFAULT([0, 64, 128, 256, 384, 640, 896, 1408, 1920, 2944, 3968, 4096]),
        M4_HIDE_EX(0),
        M4_UI_MODULE(curve),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(The x-axis of gic luma curve. \nFreq of use: low))  */
    uint16_t sw_gic_curve_idx[RKGIC_V40_LUMA_POINT_NUM];
} gic_gic_static_t;

typedef struct gic_params_static_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(RBFilter),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_static_t RBFilter;
    /* M4_GENERIC_DESC(
        M4_ALIAS(gic),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_gic_static_t   gic;
} gic_params_static_t;

typedef struct gic_gic_dyn_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_loFltGr_coeff),
        M4_TYPE(u8),
        M4_SIZE_EX(1,4),
        M4_RANGE_EX(0,128),
        M4_DEFAULT(4,4,4,4),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(Gr channel lowpass filter coefficien.Freq of use: low))  */
	uint8_t hw_gic_loFltGr_coeff[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_loFltGb_coeff),
        M4_TYPE(u8),
        M4_SIZE_EX(1,2),
        M4_RANGE_EX(0,128),
        M4_DEFAULT(9,9),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(Gb channel lowpass filter coefficien.Freq of use: low))  */
	uint8_t hw_gic_loFltGb_coeff[2];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_fusionWgt_minThred),
        M4_TYPE(u16),
        M4_SIZE_EX(1,4),
        M4_RANGE_EX(0, 4095),
        M4_DEFAULT([4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095]),
        M4_HIDE_EX(0),
        M4_UI_MODULE(curve),
        M4_DATAX([0, 64, 128, 256, 384, 640, 896, 1408, 1920, 2944, 3968, 4096]),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(The gic fusion wgt thred. \nFreq of use: low))  */
	uint16_t hw_gic_fusionWgt_minThred[RKGIC_V40_LUMA_POINT_NUM];
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_fusionWgt_slope),
        M4_TYPE(f32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,1),
        M4_DEFAULT(1),
        M4_DIGIT_EX(3),
        M4_FP_EX(0,4,4),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(1),
        M4_NOTES(gic fusion wgt slope.\nFreq of use: low))  */
	float hw_gic_fusionWgt_slope;	
    /* M4_GENERIC_DESC(
        M4_ALIAS(hw_gic_luma2SofThd_thred),
        M4_TYPE(u16),
        M4_SIZE_EX(1,12),
        M4_RANGE_EX(0, 512),
        M4_DEFAULT([0, 16, 32, 64, 96, 128, 128, 128, 128, 128, 128, 128]),
        M4_HIDE_EX(0),
        M4_UI_MODULE(curve),
        M4_DATAX([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(The softhred curve. \nFreq of use: low))  */
	uint16_t hw_gic_luma2SofThd_thred[RKGIC_V40_LUMA_POINT_NUM];
} gic_gic_dyn_t;

typedef struct gic_params_dyn_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(RBFilter),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_rbflt_dyn_t RBFilter;
    /* M4_GENERIC_DESC(
        M4_ALIAS(gic),
        M4_TYPE(struct),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_gic_dyn_t   gic;
} gic_params_dyn_t;

typedef struct gic_param_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(sta),
        M4_TYPE(struct),
        M4_UI_MODULE(static_ui),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_params_static_t sta;
    /* M4_GENERIC_DESC(
        M4_ALIAS(dyn),
        M4_TYPE(struct),
        M4_UI_MODULE(dynamic_ui),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(2),
        M4_NOTES(TODO))  */
    gic_params_dyn_t dyn;
} gic_param_t;

#endif

