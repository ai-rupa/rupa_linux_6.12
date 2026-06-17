/*
 *  Copyright (c) 2019 Rockchip Corporation
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
#include "sample_comm.h"

#ifdef SAMPLE_SMART_IR

#include <fcntl.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "rk_smart_ir_api.h"
#include "uAPI2/rk_aiq_user_api2_ae.h"
#include "uAPI2/rk_aiq_user_api2_awb.h"

typedef struct sample_smartIr_s {
    pthread_t tid;
    bool tquit;
    bool started;
    const rk_aiq_sys_ctx_t* aiq_ctx;
    rk_smart_ir_ctx_t* ir_ctx;
    bool camGroup;
} sample_smartIr_t;

typedef struct smartIr_dualCam_s {
    pthread_t tid;
    bool tquit;
    bool started;
    const rk_aiq_sys_ctx_t* aiq_ctx0;
    rk_smart_ir_ctx_t* ir_ctx0;
    const rk_aiq_sys_ctx_t* aiq_ctx1;
    rk_smart_ir_ctx_t* ir_ctx1;
} smartIr_dualCam_t;

static sample_smartIr_t g_sample_smartIr_ctx;
static smartIr_dualCam_t g_smartIr_dualCam_ctx;

static void ir_cutter_ctrl(bool on)
{
    // TODO: implement user-defined IR-Cutter control
}

static void fill_led_ctrl(bool led_on, float led_value)
{
    // TODO: implement user-defined LED control
}

static void* switch_thread_irled(void* args)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;
    rk_smart_ir_result_t result;

    while (!smartIr_ctx->tquit) {

        rk_smart_ir_run(smartIr_ctx->ir_ctx, smartIr_ctx->camGroup, &result);

        if (result.status == RK_SMART_IR_STATUS_NIGHT) {
            if (result.is_status_change) {

                printf("SAMPLE_SMART_IR: switch to Night\n");
                // Step 1: switch ISP to night scene
                rk_aiq_uapi2_setGrayMode(smartIr_ctx->aiq_ctx, true);
                usleep(100 * 1000); // add a one-frame delay
                rk_aiq_uapi2_sysctl_switch_scene(smartIr_ctx->aiq_ctx, "normal", "night");
                // Step 2: turn off IR-cut
                ir_cutter_ctrl(false);
            }
            if (result.is_fill_change) {
                // Step 3: turn on IR LED
                fill_led_ctrl(true, result.fill_value);
            }

        } else if (result.status == RK_SMART_IR_STATUS_DAY && result.is_status_change) {
            printf("SAMPLE_SMART_IR: switch to Day\n");

            // Step 1: turn off IR LED
            fill_led_ctrl(false, result.fill_value);
            // Step 2: turn on IR-cut
            ir_cutter_ctrl(true);
            // Step 3: switch ISP to day scene
            rk_aiq_uapi2_sysctl_switch_scene(smartIr_ctx->aiq_ctx, "normal", "day");
            rk_aiq_uapi2_setGrayMode(smartIr_ctx->aiq_ctx, false);
        }
    }

    return NULL;
}

static void* switch_thread_visled(void* args)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;
    rk_smart_ir_result_t result;

    // turn on IR-cut
    ir_cutter_ctrl(true);
    // switch ISP to day scene
    rk_aiq_uapi2_sysctl_switch_scene(smartIr_ctx->aiq_ctx, "normal", "day");

    while (!smartIr_ctx->tquit) {

        rk_smart_ir_run(smartIr_ctx->ir_ctx, smartIr_ctx->camGroup, &result);

        if (result.status == RK_SMART_IR_STATUS_NIGHT) {
            if (result.is_status_change) {
                printf("SAMPLE_SMART_IR: switch to Night\n");
            }
            if (result.is_fill_change) {
                // turn on VIS LED
                fill_led_ctrl(true, result.fill_value);
            }

        } else if (result.status == RK_SMART_IR_STATUS_DAY && result.is_status_change) {
            printf("SAMPLE_SMART_IR: switch to Day\n");
            // turn off VIS LED
            fill_led_ctrl(false, result.fill_value);
        }
    }

    return NULL;
}

static void sample_smartIr_start_irled(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    // 1) init
    smartIr_ctx->ir_ctx = rk_smart_ir_init((rk_aiq_sys_ctx_t*)arg);

    // 2) load configs: auto switch, manual ir led
    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.init_status = RK_SMART_IR_STATUS_DAY;
    attr.switch_mode = RK_SMART_IR_SWITCH_MODE_AUTO;
    attr.light_mode = RK_SMART_IR_LIGHT_MODE_MANUAL;
    attr.light_type = RK_SMART_IR_LIGHT_TYPE_IR;
    attr.light_value = 100;
    attr.params.d2n_envL_th = 0.04f;
    attr.params.n2d_envL_th = 0.20f;
    attr.params.rggain_base = 1.00f;
    attr.params.bggain_base = 1.00f;
    attr.params.awbgain_rad = 0.10f;
    attr.params.awbgain_dis = 0.20f;
    attr.params.switch_cnts_th = 50;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);

    // 3) create thread
    smartIr_ctx->tquit = false;
    pthread_create(&smartIr_ctx->tid, NULL, switch_thread_irled, NULL);
    smartIr_ctx->started = true;
}

static void sample_smartIr_start_visled(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    // 1) init
    smartIr_ctx->ir_ctx = rk_smart_ir_init((rk_aiq_sys_ctx_t*)arg);

    // 2) load configs: auto switch, auto vis led
    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.init_status = RK_SMART_IR_STATUS_DAY;
    attr.switch_mode = RK_SMART_IR_SWITCH_MODE_AUTO;
    attr.light_mode = RK_SMART_IR_LIGHT_MODE_AUTO;
    attr.light_type = RK_SMART_IR_LIGHT_TYPE_VIS;
    attr.light_value = 100;
    attr.params.d2n_envL_th = 0.04f;
    attr.params.n2d_envL_th = 0.60f;
    attr.params.rggain_base = 0.0f;
    attr.params.bggain_base = 0.0f;
    attr.params.awbgain_rad = 0.0f;
    attr.params.awbgain_dis = 0.0f;
    attr.params.switch_cnts_th = 50;
    attr.en_auto_n2dth = true;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);

    // 3) create thread
    smartIr_ctx->tquit = false;
    pthread_create(&smartIr_ctx->tid, NULL, switch_thread_visled, NULL);
    smartIr_ctx->started = true;
}

void sample_smartIr_cb(rk_smart_ir_result_t result)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    if (result.status == RK_SMART_IR_STATUS_NIGHT) {
        if (result.is_status_change) {
            printf("SAMPLE_SMART_IR: switch to Night\n");

            // Step 1: switch ISP to night scene
            rk_aiq_uapi2_setGrayMode(smartIr_ctx->aiq_ctx, true);
            usleep(100 * 1000); // add a one-frame delay
            rk_aiq_uapi2_sysctl_switch_scene(smartIr_ctx->aiq_ctx, "normal", "night");
            // Step 2: turn off IR-cut
            ir_cutter_ctrl(false);
        }
        if (result.is_fill_change) {
            // Step 3: turn on IR LED
            fill_led_ctrl(true, result.fill_value);
        }

    } else if (result.status == RK_SMART_IR_STATUS_DAY && result.is_status_change) {
        printf("SAMPLE_SMART_IR: switch to Day\n");

        // Step 1: turn off IR LED
        fill_led_ctrl(false, result.fill_value);
        // Step 2: turn on IR-cut
        ir_cutter_ctrl(true);
        // Step 3: switch ISP to day scene
        rk_aiq_uapi2_sysctl_switch_scene(smartIr_ctx->aiq_ctx, "normal", "day");
        rk_aiq_uapi2_setGrayMode(smartIr_ctx->aiq_ctx, false);
    }
}

static void sample_smartIr_start(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    // 1) Initialize smartIr context
    smartIr_ctx->ir_ctx = rk_smart_ir_init((rk_aiq_sys_ctx_t*)arg);

    // 2) Configure smartIr parameters (choose one of the following methods)
    /*
     * NOTE:
     * - You can use either Option A or Option B.
     * - If both are called, the later configuration will override the earlier one.
     *
     * Option A: Load parameters from an INI file
     *   Example:
     *     rk_smart_ir_iniCfg(smartIr_ctx->ir_ctx, "tmp/smart_ir.ini");
     *
     * Option B: Configure parameters manually via getAttr/setAttr API
     */

    // Example Option B settings
    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.init_status = RK_SMART_IR_STATUS_DAY;
    attr.switch_mode = RK_SMART_IR_SWITCH_MODE_AUTO;
    attr.light_mode = RK_SMART_IR_LIGHT_MODE_MANUAL;
    attr.light_type = RK_SMART_IR_LIGHT_TYPE_IR;
    attr.light_value = 100;
    attr.params.d2n_envL_th = 0.04f;
    attr.params.n2d_envL_th = 0.20f;
    attr.params.rggain_base = 1.00f;
    attr.params.bggain_base = 1.00f;
    attr.params.awbgain_rad = 0.10f;
    attr.params.awbgain_dis = 0.20f;
    attr.params.switch_cnts_th = 50;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);

    // 3) Start smartIr by registering the run callback
    rk_smart_ir_runCb(smartIr_ctx->ir_ctx, smartIr_ctx->camGroup, sample_smartIr_cb);
}

static void sample_smartIr_stop(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    if (smartIr_ctx->started) {
        smartIr_ctx->tquit = true;
        pthread_join(smartIr_ctx->tid, NULL);
    }
    smartIr_ctx->started = false;

    if (smartIr_ctx->ir_ctx) {
        rk_smart_ir_deInit(smartIr_ctx->ir_ctx);
        smartIr_ctx->ir_ctx = NULL;
    }

    printf("stop smartIr\n");
}

static void* smartIr_dualCam_thread(void* args)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    smartIr_dualCam_t* dualCam_ctx = &g_smartIr_dualCam_ctx;
    rk_smart_ir_result_t result;

    while (!dualCam_ctx->tquit) {

        rk_smart_ir_dualRun(dualCam_ctx->ir_ctx0, dualCam_ctx->ir_ctx1, &result);

        if (result.status == RK_SMART_IR_STATUS_NIGHT && result.is_status_change) {
            printf("SAMPLE_SMART_IR: switch to Night\n");

            // Step 1: switch ISP to night scene
            rk_aiq_uapi2_sysctl_switch_scene(dualCam_ctx->aiq_ctx0, "normal", "night");
            rk_aiq_uapi2_sysctl_switch_scene(dualCam_ctx->aiq_ctx1, "normal", "night");
            // Step 2: turn off IR-cut
            // Step 3: turn on IR LED

        } else if (result.status == RK_SMART_IR_STATUS_DAY && result.is_status_change) {
            printf("SAMPLE_SMART_IR: switch to Day\n");

            // Step 1: turn off IR LED
            // Step 2: turn on IR-cut
            // Step 3: switch ISP to day scene
            rk_aiq_uapi2_sysctl_switch_scene(dualCam_ctx->aiq_ctx0, "normal", "day");
            rk_aiq_uapi2_sysctl_switch_scene(dualCam_ctx->aiq_ctx1, "normal", "day");
        }

    }

    return NULL;
}

static void smartIr_dualCam_start(const void* arg)
{
    smartIr_dualCam_t* dualCam_ctx = &g_smartIr_dualCam_ctx;

    // 1) cam0 cfg
    dualCam_ctx->aiq_ctx0 = (rk_aiq_sys_ctx_t*)arg; //need to modify
    dualCam_ctx->ir_ctx0 = rk_smart_ir_init(dualCam_ctx->aiq_ctx0);

    rk_smart_ir_attr_t attr0;
    memset(&attr0, 0, sizeof(attr0));
    rk_smart_ir_getAttr(dualCam_ctx->ir_ctx0, &attr0);
    attr0.init_status = RK_SMART_IR_STATUS_DAY;
    attr0.switch_mode = RK_SMART_IR_SWITCH_MODE_AUTO;
    attr0.light_mode = RK_SMART_IR_LIGHT_MODE_MANUAL;
    attr0.light_type = RK_SMART_IR_LIGHT_TYPE_IR;
    attr0.light_value = 100;
    attr0.params.d2n_envL_th = 0.04f; //need to tune
    attr0.params.n2d_envL_th = 0.20f; //need to tune
    attr0.params.rggain_base = 1.00f; //need to tune
    attr0.params.bggain_base = 1.00f; //need to tune
    attr0.params.awbgain_rad = 0.10f; //need to tune
    attr0.params.awbgain_dis = 0.20f; //need to tune
    attr0.params.switch_cnts_th = 50;
    rk_smart_ir_setAttr(dualCam_ctx->ir_ctx0, &attr0);

    // 2) cam1 cfg
    dualCam_ctx->aiq_ctx1 = (rk_aiq_sys_ctx_t*)arg; //need to modify
    dualCam_ctx->ir_ctx1 = rk_smart_ir_init(dualCam_ctx->aiq_ctx1);

    rk_smart_ir_attr_t attr1;
    memset(&attr1, 0, sizeof(attr1));
    rk_smart_ir_getAttr(dualCam_ctx->ir_ctx1, &attr1);
    attr1.init_status = RK_SMART_IR_STATUS_DAY;
    attr1.switch_mode = RK_SMART_IR_SWITCH_MODE_AUTO;
    attr1.light_mode = RK_SMART_IR_LIGHT_MODE_MANUAL;
    attr1.light_type = RK_SMART_IR_LIGHT_TYPE_IR;
    attr1.light_value = 100;
    attr1.params.d2n_envL_th = 0.04f; //need to tune
    attr1.params.n2d_envL_th = 0.20f; //need to tune
    attr1.params.rggain_base = 1.00f; //need to tune
    attr1.params.bggain_base = 1.00f; //need to tune
    attr1.params.awbgain_rad = 0.10f; //need to tune
    attr1.params.awbgain_dis = 0.20f; //need to tune
    attr1.params.switch_cnts_th = 50;
    rk_smart_ir_setAttr(dualCam_ctx->ir_ctx1, &attr1);

    // 3) create thread
    dualCam_ctx->tquit = false;
    pthread_create(&dualCam_ctx->tid, NULL, smartIr_dualCam_thread, NULL);
    dualCam_ctx->started = true;
}

static void smartIr_dualCam_stop(const void* arg)
{
    smartIr_dualCam_t* dualCam_ctx = &g_smartIr_dualCam_ctx;

    if (dualCam_ctx->started) {
        dualCam_ctx->tquit = true;
        pthread_join(dualCam_ctx->tid, NULL);
    }
    dualCam_ctx->started = false;

    if (dualCam_ctx->ir_ctx0) {
        rk_smart_ir_deInit(dualCam_ctx->ir_ctx0);
        dualCam_ctx->ir_ctx0 = NULL;
    }

    if (dualCam_ctx->ir_ctx1) {
        rk_smart_ir_deInit(dualCam_ctx->ir_ctx1);
        dualCam_ctx->ir_ctx1 = NULL;
    }

    printf("stop smartIr\n");
}

static void sample_smartIr_calib(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

#if 1
    // Method 1: Set calibration via INI file (Recommended)
    /*
     * - Ensure `calib_en = true` in the INI file to enable calibration.
     * - Select calibration method via `calib_mode`.
     * - Calibration results are saved directly back to the INI file.
     */
    sample_smartIr_start(arg);
    rk_smart_ir_iniCfg(smartIr_ctx->ir_ctx, "tmp/smart_ir.ini");

#elif 0
    // Method 2: Set calibration via getAttr/setAttr API
    /*
     * - Enable calibration by setting `attr.calib_cfg.calib_en = true`.
     * - Select calibration method via `attr.calib_cfg.calib_mode`.
     * - Calibration results are stored in `attr.calib_cfg.calib_result`.
     */
    sample_smartIr_start(arg);
    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.calib_cfg.calib_en = true;
    attr.calib_cfg.calib_mode = RK_SMART_IR_CALIB_MODE_D2N;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);

#elif 0
    // Method 3: Set calibration via 'rk_smart_ir_calib' API (Not recommended)

    smartIr_ctx->ir_ctx = rk_smart_ir_init((rk_aiq_sys_ctx_t*)arg);
    rk_smart_ir_calib_t calib_cfg;
    calib_cfg.calib_en = true;

    /* Calib d2n_envL_th
     * Step 1: switch ISP to day scene
     * Step 2: enable IR-cutter, disable IR-LED
     * Step 3: adjust brightness threshold for day-to-night switch
     */
    calib_cfg.calib_mode = RK_SMART_IR_CALIB_MODE_D2N;
    rk_smart_ir_calib(smartIr_ctx->ir_ctx, &calib_cfg);
    printf("calib result: d2n_envL_th = %f\n", calib_cfg.calib_result.d2n_envL_th);

    /* Calib n2d_envL_th
     * Step 1: switch ISP to night scene
     * Step 2: disable IR-cutter, enable IR-LED
     * Step 3: adjust brightness threshold for night-to-day switch
     */
    calib_cfg.calib_mode = RK_SMART_IR_CALIB_MODE_N2D;
    rk_smart_ir_calib(smartIr_ctx->ir_ctx, &calib_cfg);
    printf("calib result: n2d_envL_th = %f\n", calib_cfg.calib_result.n2d_envL_th);

    /* Calib awbgain base
     * Step 1: switch ISP to night scene
     * Step 2: disable IR-cutter, enable IR-LED
     * Step 3: ensure no visible light
     */
    calib_cfg.calib_mode = RK_SMART_IR_CALIB_MODE_BASE;
    rk_smart_ir_calib(smartIr_ctx->ir_ctx, &calib_cfg);
    printf("calib result: awbgain_base = %f, %f\n", calib_cfg.calib_result.rggain_base,
           calib_cfg.calib_result.bggain_base);

    /* Tune awbgain dis
     * Step 1: switch ISP to night scene
     * Step 2: disable IR-cutter, enable IR-LED
     * Step 3: test various scenarios and get the maximum value
     */
    calib_cfg.calib_mode = RK_SMART_IR_CALIB_MODE_DIS;
    calib_cfg.calib_params.rggain_base = 1.00f; //need cfg, init rggain_base param
    calib_cfg.calib_params.bggain_base = 1.00f; //need cfg, init bggain_base param
    calib_cfg.calib_params.awbgain_rad = 0.10f; //need cfg, init awbgain_rad param
    rk_smart_ir_calib(smartIr_ctx->ir_ctx, &calib_cfg);
    printf("calib result: awbgain_dis = %f\n", calib_cfg.calib_result.awbgain_dis);

    if (smartIr_ctx->ir_ctx) {
        rk_smart_ir_deInit(smartIr_ctx->ir_ctx);
        smartIr_ctx->ir_ctx = NULL;
    }
#endif
}

static void sample_smartIr_test_switch(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    sample_smartIr_start(arg);

    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.switch_test.switch_en = true;
    attr.switch_test.switch_interval = 100;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);
}

static void sample_smartIr_lock_switch(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    sample_smartIr_start(arg);

    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.lock_cfg.lock_en = true;
    attr.lock_cfg.lock_interval = 100;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);
}

static void sample_smartIr_starlight(const void* arg)
{
    sample_smartIr_t* smartIr_ctx = &g_sample_smartIr_ctx;

    sample_smartIr_start(arg);

    rk_smart_ir_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    rk_smart_ir_getAttr(smartIr_ctx->ir_ctx, &attr);
    attr.starlight_cfg.mode_en = true;
    attr.starlight_cfg.mid_trans_en = true;
    attr.starlight_cfg.low_envL_th = 0.8f;
    attr.starlight_cfg.high_envL_th = 1.0f;
    rk_smart_ir_setAttr(smartIr_ctx->ir_ctx, &attr);
}

static XCamReturn sample_getIspStats(const rk_aiq_sys_ctx_t* ctx)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    uint32_t R_blk[225] = { 0 }, G_blk[225] = { 0 }, B_blk[225] = { 0 };
    uint32_t WpNo_blk[225] = { 0 };
    float rgain = 0.0f, bgain = 0.0f;
    int cnt = 0;
    uint16_t Y_blk[225] = { 0 };

#ifdef USE_NEWSTRUCT
    // 1) get isp stats
    rk_aiq_isp_statistics_t isp_stats;
    isp_stats.bValid_aec_stats = false;
    isp_stats.bValid_awb_stats = false;
    ret = rk_aiq_uapi2_stats_getIspStats(ctx, &isp_stats, 1000);
    if (ret == XCAM_RETURN_NO_ERROR && isp_stats.bValid_aec_stats && isp_stats.bValid_awb_stats) {
        // do nothing
    } else {
        printf("ret=%d, stats_id=%d, valid=%d,%d, getIspStats fail!\n", ret, isp_stats.frame_id,
               isp_stats.bValid_aec_stats, isp_stats.bValid_awb_stats);
        return ret;
    }

    // 2) calc awb gain
    for (int i = 0; i < RAWAEBIG_WIN_NUM; i++) {
        R_blk[i] = isp_stats.awb_stats.com.pixEngine.zonePix[i].hw_awbCfg_rSum_val;
        G_blk[i] = isp_stats.awb_stats.com.pixEngine.zonePix[i].hw_awbCfg_gSum_val;
        B_blk[i] = isp_stats.awb_stats.com.pixEngine.zonePix[i].hw_awbCfg_bSum_val;
        WpNo_blk[i] = isp_stats.awb_stats.com.pixEngine.zonePix[i].hw_awbCfg_statsPix_count;
        if (G_blk[i] > 0) {
            rgain = rgain + (float)R_blk[i] / (float)G_blk[i];
            bgain = bgain + (float)B_blk[i] / (float)G_blk[i];
            cnt++;
        }
    }
    if (cnt > 0) {
        rgain = rgain / cnt;
        bgain = bgain / cnt;
    }

    // 3) get ae stats
    Uapi_RkAeStats_t AeHwStats;
    rk_aiq_user_api2_ae_getRKAeStats(ctx, &AeHwStats);
    memcpy(Y_blk, AeHwStats.chn[0].rawae_big.channely_xy, sizeof(Y_blk));

    // 4) querry ae/awb info
    rk_aiq_wb_querry_info_t wbInfo;
    ae_api_queryInfo_t expInfo;
    rk_aiq_user_api2_awb_QueryWBInfo(ctx, &wbInfo);
    rk_aiq_user_api2_ae_queryExpResInfo(ctx, &expInfo);

    printf("frame_id=%d,cnt=%d,rgain=%f,bgain=%f,wbrgain=%f,wbbgain=%f\nae_converged=%d,meanluma=%f,exp=[%f,%f,%f]\n",
           isp_stats.frame_id, cnt, rgain, bgain, wbInfo.stat_gain_blk.rgain, wbInfo.stat_gain_blk.bgain,
           expInfo.isConverged, expInfo.linExpInfo.meanLuma, expInfo.linExpInfo.expParam.integration_time,
           expInfo.linExpInfo.expParam.analog_gain, expInfo.linExpInfo.expParam.isp_dgain);

#else
    // 1) get isp stats
    rk_aiq_isp_stats_t *stats_ref = NULL;
    ret = rk_aiq_uapi2_sysctl_get3AStatsBlk(ctx, &stats_ref, 1000);
    if (ret != XCAM_RETURN_NO_ERROR || stats_ref == NULL) {
        printf("ret=%d, get3AStatsBlk fail!\n", ret);
        rk_aiq_uapi2_sysctl_release3AStatsRef(ctx, stats_ref);
        return ret;
    }

    // 2) calc awb gain
#if defined(ISP_HW_V32) || defined(ISP_HW_V32_LITE)
    for (int i = 0; i < RAWAEBIG_WIN_NUM; i++) {
        R_blk[i] = (float)stats_ref->awb_stats_v32.blockResult[i].Rvalue;
        G_blk[i] = (float)stats_ref->awb_stats_v32.blockResult[i].Gvalue;
        B_blk[i] = (float)stats_ref->awb_stats_v32.blockResult[i].Bvalue;
        WpNo_blk[i] = (float)stats_ref->awb_stats_v32.blockResult[i].WpNo;
    }
#endif
#if defined(ISP_HW_V30)
    for (int i = 0; i < RAWAEBIG_WIN_NUM; i++) {
        R_blk[i] = (float)stats_ref->awb_stats_v3x.blockResult[i].Rvalue;
        G_blk[i] = (float)stats_ref->awb_stats_v3x.blockResult[i].Gvalue;
        B_blk[i] = (float)stats_ref->awb_stats_v3x.blockResult[i].Bvalue;
        WpNo_blk[i] = (float)stats_ref->awb_stats_v3x.blockResult[i].WpNo;
    }
#endif
#if defined(ISP_HW_V21)
    for (int i = 0; i < RAWAEBIG_WIN_NUM; i++) {
        R_blk[i] = (float)stats_ref->awb_stats_v21.blockResult[i].Rvalue;
        G_blk[i] = (float)stats_ref->awb_stats_v21.blockResult[i].Gvalue;
        B_blk[i] = (float)stats_ref->awb_stats_v21.blockResult[i].Bvalue;
        WpNo_blk[i] = (float)stats_ref->awb_stats_v21.blockResult[i].WpNo;
    }
#endif
    for (int i = 0; i < RAWAEBIG_WIN_NUM; i++) {
        if (G_blk[i] > 0) {
            rgain = rgain + (float)R_blk[i] / (float)G_blk[i];
            bgain = bgain + (float)B_blk[i] / (float)G_blk[i];
            cnt++;
        }
    }
    if (cnt > 0) {
        rgain = rgain / cnt;
        bgain = bgain / cnt;
    }

    // 3) get ae stats
    memcpy(Y_blk, stats_ref->aec_stats.ae_data.chn[0].rawae_big.channely_xy, sizeof(Y_blk));
    rk_aiq_uapi2_sysctl_release3AStatsRef(ctx, stats_ref);

    // 4) querry ae/awb info
    rk_aiq_wb_querry_info_t wb_info;
    Uapi_ExpQueryInfo_t exp_info;
    rk_aiq_user_api2_awb_QueryWBInfo(ctx, &wb_info);
    rk_aiq_user_api2_ae_queryExpResInfo(ctx, &exp_info);

    printf("frame_id=%d,cnt=%d,rgain=%f,bgain=%f,wbrgain=%f,wbbgain=%f\nae_converged=%d,meanluma=%f,exp=[%f,%f,%f]\n",
           stats_ref->frame_id, cnt, rgain, bgain, wb_info.stat_gain_blk.rgain, wb_info.stat_gain_blk.bgain,
           exp_info.IsConverged, exp_info.LinAeInfo.MeanLuma, exp_info.LinAeInfo.LinearExp.integration_time,
           exp_info.LinAeInfo.LinearExp.analog_gain, exp_info.LinAeInfo.LinearExp.isp_dgain);
#endif

    printf("================================= Source statistics log =================================\n");
    printf("block luma:\n");
    for (int i = 0; i < 15; i++) {
        printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
               Y_blk[15 * i + 0], Y_blk[15 * i + 1], Y_blk[15 * i + 2], Y_blk[15 * i + 3], Y_blk[15 * i + 4],
               Y_blk[15 * i + 5], Y_blk[15 * i + 6], Y_blk[15 * i + 7], Y_blk[15 * i + 8], Y_blk[15 * i + 9],
               Y_blk[15 * i + 10], Y_blk[15 * i + 11], Y_blk[15 * i + 12], Y_blk[15 * i + 13], Y_blk[15 * i + 14]);
    }

    printf("block wbR:\n");
    for (int i = 0; i < 15; i++) {
        printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
               R_blk[15 * i + 0], R_blk[15 * i + 1], R_blk[15 * i + 2], R_blk[15 * i + 3], R_blk[15 * i + 4],
               R_blk[15 * i + 5], R_blk[15 * i + 6], R_blk[15 * i + 7], R_blk[15 * i + 8], R_blk[15 * i + 9],
               R_blk[15 * i + 10], R_blk[15 * i + 11], R_blk[15 * i + 12], R_blk[15 * i + 13], R_blk[15 * i + 14]);
    }

    printf("block wbG:\n");
    for (int i = 0; i < 15; i++) {
        printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
               G_blk[15 * i + 0], G_blk[15 * i + 1], G_blk[15 * i + 2], G_blk[15 * i + 3], G_blk[15 * i + 4],
               G_blk[15 * i + 5], G_blk[15 * i + 6], G_blk[15 * i + 7], G_blk[15 * i + 8], G_blk[15 * i + 9],
               G_blk[15 * i + 10], G_blk[15 * i + 11], G_blk[15 * i + 12], G_blk[15 * i + 13], G_blk[15 * i + 14]);
    }

    printf("block wbB:\n");
    for (int i = 0; i < 15; i++) {
        printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
               B_blk[15 * i + 0], B_blk[15 * i + 1], B_blk[15 * i + 2], B_blk[15 * i + 3], B_blk[15 * i + 4],
               B_blk[15 * i + 5], B_blk[15 * i + 6], B_blk[15 * i + 7], B_blk[15 * i + 8], B_blk[15 * i + 9],
               B_blk[15 * i + 10], B_blk[15 * i + 11], B_blk[15 * i + 12], B_blk[15 * i + 13], B_blk[15 * i + 14]);
    }

    printf("block wpNo: %d,%d\n", WpNo_blk[0], WpNo_blk[224]);
    printf("========================================== end ==========================================\n\n");

    return ret;
}

static void sample_smartIr_usage()
{
    printf("Usage : \n");
    printf("  SmartIr API: \n");
    printf("\t i) SmartIr:         Start smartIr irled test.\n");
    printf("\t v) SmartIr:         Start smartIr visled test.\n");
    printf("\t s) SmartIr:         Start smartIr (callback).\n");
    printf("\t e) SmartIr:         Stop smartIr test.\n");
    printf("\t a) SmartIr:         Start duamcam smartIr.\n");
    printf("\t b) SmartIr:         Stop duamcam smartIr.\n");
    printf("\t c) SmartIr:         Calib smartIr params.\n");
    printf("\t t) SmartIr:         Test switch smartIr.\n");
    printf("\t l) SmartIr:         Lock switch smartIr.\n");
    printf("\t f) SmartIr:         Enable starlight mode.\n");
    printf("\t g) SmartIr:         Get isp stats.\n");

    printf("\n");
    printf("\t h) SmartIr:         help.\n");
    printf("\t q) SmartIr:         return to main sample screen.\n");
    printf("\n");
    printf("\t please press the key: ");

    return;
}

void sample_print_smartIr_info(const void* arg)
{
    printf("enter SmartIr modult test!\n");
}

XCamReturn sample_smartIr_module(const void* arg)
{
    int key = -1;
    CLEAR();

    const demo_context_t *demo_ctx = (demo_context_t *)arg;
    const rk_aiq_sys_ctx_t* ctx;
    if (demo_ctx->camGroup) {
        ctx = (rk_aiq_sys_ctx_t*)(demo_ctx->camgroup_ctx);
    } else {
        ctx = (rk_aiq_sys_ctx_t*)(demo_ctx->aiq_ctx);
    }

    if (ctx == NULL) {
        ERR("%s, ctx is nullptr\n", __FUNCTION__);
        return XCAM_RETURN_ERROR_PARAM;
    }

    sample_smartIr_usage();

    g_sample_smartIr_ctx.tquit = false;
    g_sample_smartIr_ctx.started = false;
    g_sample_smartIr_ctx.aiq_ctx = ctx;
    g_sample_smartIr_ctx.ir_ctx = NULL;
    g_sample_smartIr_ctx.camGroup = demo_ctx->camGroup;

    do {
        key = getchar();
        while (key == '\n' || key == '\r')
            key = getchar();
        printf("\n");

        switch (key) {
        case 'h':
            CLEAR();
            sample_smartIr_usage();
            break;
        case 'i':
            sample_smartIr_start_irled(ctx);
            break;
        case 'v':
            sample_smartIr_start_visled(ctx);
            break;
        case 's':
            sample_smartIr_start(ctx);
            break;
        case 'e':
            sample_smartIr_stop(ctx);
            break;
        case 'a':
            smartIr_dualCam_start(ctx);
            break;
        case 'b':
            smartIr_dualCam_stop(ctx);
            break;
        case 'c':
            sample_smartIr_calib(ctx);
            break;
        case 't':
            sample_smartIr_test_switch(ctx);
            break;
        case 'l':
            sample_smartIr_lock_switch(ctx);
            break;
        case 'f':
            sample_smartIr_starlight(ctx);
            break;
        case 'g':
            sample_getIspStats(ctx);
            break;
        default:
            break;
        }
    } while (key != 'q' && key != 'Q');

    sample_smartIr_stop(ctx);

    return XCAM_RETURN_NO_ERROR;
}

#else
void sample_print_smartIr_info(const void* arg)
{
    printf("enter SmartIr modult test!\n");
}

XCamReturn sample_smartIr_module(const void* arg)
{
    printf("Not enabled! Add option SAMPLE_SMART_IR in makefile \n");
    return XCAM_RETURN_NO_ERROR;
}
#endif
