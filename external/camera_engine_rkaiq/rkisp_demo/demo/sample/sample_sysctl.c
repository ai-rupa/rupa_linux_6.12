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

#include "sample_sysctl.h"

#include "sample_comm.h"
#if defined(ISP_HW_V33)||defined(ISP_HW_V39) || defined(ISP_HW_V35) || defined(ISP_HW_V351s)
#include "uAPI2/rk_aiq_user_api2_ae.h"
#include "uAPI2/rk_aiq_user_api2_awb_v3.h"
#endif

void sample_print_sysctl_info(const void *arg)
{
    printf("test sysctl !\n");
}

static void sample_sysctl_usage()
{
    printf("Usage : \n");
    printf("\t 0) SYSCTL:         sample_pause_resume.\n");
    printf("\t 1) SYSCTL:         sample_mirror_flip.\n");
    printf("\t 2) SYSCTL:         sample_getHdrComprCurve.\n");
    printf("\t 3) SYSCTL:         sample_testPreviewCaptureWorkflow.\n");
    printf("\n");
    printf("\t please press the key: \n");

    return;
}

static void sample_pause_resume(const rk_aiq_sys_ctx_t* ctx)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    static bool g_pause = false;
    if (!g_pause) {
        ret = rk_aiq_uapi2_sysctl_pause((rk_aiq_sys_ctx_t*)ctx, true);
    } else {
        ret = rk_aiq_uapi2_sysctl_resume((rk_aiq_sys_ctx_t*)ctx);
    }
    g_pause = !g_pause;
    printf("%s done, ret: %d, pause:%d\n", __func__, ret, g_pause);
}

static void sample_mirrflip(const rk_aiq_sys_ctx_t* ctx)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    static int g_mirrflip = 0;
    bool flip = g_mirrflip & 0x1;
    bool mirror = g_mirrflip & 0x2;
    ret = rk_aiq_uapi2_setMirrorFlip((rk_aiq_sys_ctx_t*)ctx, mirror, flip, 3);
    g_mirrflip++;
    g_mirrflip %= 4;
    printf("%s done, ret: %d, flip :%d, mirror:%d\n", __func__, ret, flip, mirror);
}

static void sample_getHdrComprCurve(const rk_aiq_sys_ctx_t* ctx) {
#ifdef USE_NEWSTRUCT
    XCamReturn ret        = XCAM_RETURN_NO_ERROR;
    RkAiqHdrCompr_t compr = {0};
    ret                   = rk_aiq_uapi2_sysctl_getHdrComprCurve((rk_aiq_sys_ctx_t*)ctx, &compr);
    if (ret != XCAM_RETURN_NO_ERROR) {
        printf("get hdr compr curve failed, ret: %d\n", ret);
        return;
    }
#endif
}

static rk_aiq_sys_ctx_t* demo_ctx_get_aiq_ctx(const demo_context_t* ctx) {
    if (ctx->camGroup)
        return (rk_aiq_sys_ctx_t*)(ctx->camgroup_ctx);
    else
        return (rk_aiq_sys_ctx_t*)(ctx->aiq_ctx);
}

static void sample_testPreviewCaptureWorkflow(const demo_context_t* demo_ctx) {
    XCamReturn ret  = XCAM_RETURN_NO_ERROR;
    int loop_count  = 1;
    int frame_count = 1;

    // Collect all aiq contexts: main + peers
    rk_aiq_sys_ctx_t* ctx_list[1 + DEMO_CTX_MAX_PEERS];
    int ctx_count = 0;

    ctx_list[ctx_count++] = demo_ctx_get_aiq_ctx(demo_ctx);
    for (int p = 0; p < demo_ctx->num_peers && p < DEMO_CTX_MAX_PEERS; p++) {
        if (demo_ctx->peer_ctx[p]) {
            rk_aiq_sys_ctx_t* peer = demo_ctx_get_aiq_ctx(demo_ctx->peer_ctx[p]);
            if (peer) ctx_list[ctx_count++] = peer;
        }
    }

    printf("\n");
    printf("+--------------------------------------------------+\n");
    printf("|       Preview-Capture Workflow Configuration      |\n");
    printf("+--------------------------------------------------+\n");
    printf("|  loop_count  : number of trigger iterations       |\n");
    printf("|               (1 = infinite loop)                 |\n");
    printf("|  frame_count : frames per trigger (default: 1)    |\n");
    printf("+--------------------------------------------------+\n");
    printf("\n");

    char buf[32];

    printf(">>> Enter loop_count (0 for infinite) [default %d]: ", loop_count);
    if (fgets(buf, sizeof(buf), stdin) != NULL && buf[0] != '\n') {
        sscanf(buf, "%d", &loop_count);
    }
    printf("    -> loop_count = %d\n\n", loop_count);

    printf(">>> Enter frame_count per trigger [default %d]: ", frame_count);
    if (fgets(buf, sizeof(buf), stdin) != NULL && buf[0] != '\n') {
        int tmp = 0;
        if (sscanf(buf, "%d", &tmp) == 1 && tmp > 0) frame_count = tmp;
    }
    printf("    -> frame_count = %d\n\n", frame_count);

    printf("==================================================\n");
    printf("  Preview-Capture Workflow Test START\n");
    printf("  loop_count=%d, frame_count=%d, ctx_count=%d\n", loop_count, frame_count, ctx_count);
    printf("==================================================\n");

    int i = 0;
    int err_count = 0;
#if defined(ISP_HW_V33)||defined(ISP_HW_V39) || defined(ISP_HW_V35) || defined(ISP_HW_V351s)
    // Loop: collect 3A metadata + trigger capture with metadata
    while (loop_count == 0 || i < loop_count) {
        if (loop_count > 0)
            printf("\n--- [iter %d/%d] Step 1: Collecting 3A metadata ---\n", i + 1, loop_count);
        else
            printf("\n--- [iter %d/inf] Step 1: Collecting 3A metadata ---\n", i + 1);
        rk_aiq_frame_info_t meta = {0};
        // Get 3A info from peer ctx if available, otherwise from self
        rk_aiq_sys_ctx_t* src_ctx = (ctx_count > 1) ? ctx_list[1] : ctx_list[0];
        int src_idx               = (ctx_count > 1) ? 1 : 0;
        {
            // [Optional] Get ISP statistics for reference/debug
            rk_aiq_isp_statistics_t aiq_stats;
            ret = rk_aiq_uapi2_stats_getIspStats(src_ctx, &aiq_stats, 300);
            if (ret == XCAM_RETURN_NO_ERROR) {
                printf("    Got ISP stats from ctx[%d] for reference: frameId=%d\n", src_idx,
                       aiq_stats.frame_id);
                meta.frame_id = aiq_stats.frame_id;
            } else {
                printf("    Failed to get ISP stats from ctx[%d]: %d\n", src_idx, ret);
            }

            rk_aiq_wb_gain_t gain = {0};
            ret                   = rk_aiq_uapi2_getWBGain(src_ctx, &gain);
            if (ret == XCAM_RETURN_NO_ERROR) {
                meta.awg_rgain = gain.rgain;
                meta.awg_bgain = gain.bgain;
                printf("    awb wbgain from ctx[%d]: rgain=%.4f, bgain=%.4f\n", src_idx,
                       meta.awg_rgain, meta.awg_bgain);
            } else {
                printf("    Failed to get AWB wbgain from ctx[%d]: %d\n", src_idx, ret);
            }

            ae_api_queryInfo_t ae_info = {0};
            ret                        = rk_aiq_user_api2_ae_queryExpResInfo(src_ctx, &ae_info);
            if (ret == XCAM_RETURN_NO_ERROR) {
                meta.normal_exp   = ae_info.linExpInfo.expParam.integration_time;
                meta.normal_gain  = ae_info.linExpInfo.expParam.analog_gain;
                meta.isp_dgain[0] = ae_info.linExpInfo.expParam.isp_dgain;

                if (meta.normal_exp <= 0.0f) meta.normal_exp = 0.1f;
                if (meta.normal_gain <= 0.0f) meta.normal_gain = 15.0f;
                if (meta.isp_dgain[0] <= 0.0f) meta.isp_dgain[0] = 1.0f;

                printf("    ae linExpInfo from ctx[%d]: exp=%.6f, gain=%.3f, isp_dgain=%.3f\n",
                       src_idx, meta.normal_exp, meta.normal_gain, meta.isp_dgain[0]);
            } else {
                printf("    Failed to get AE expInfo from ctx[%d]: %d\n", src_idx, ret);
            }
        }

        if (loop_count > 0)
            printf("--- [iter %d/%d] Step 2: Triggering capture with 3A metadata (%d frames) ---\n",
                   i + 1, loop_count, frame_count);
        else
            printf(
                "--- [iter %d/inf] Step 2: Triggering capture with 3A metadata (%d frames) ---\n",
                i + 1, frame_count);

        rk_aiq_capture_trigger_t trigger = {0};
        trigger.frame_count              = frame_count;
        trigger.use_frame_meta           = 1;     // Enable 3A metadata processing
        trigger.frame_meta               = meta;  // Copy 3A metadata to trigger
        ret = rk_aiq_uapi2_sysctl_triggerCapture(ctx_list[0], &trigger);
        if (ret != XCAM_RETURN_NO_ERROR) {
            err_count++;
            printf("Failed to trigger capture: %d (total errors: %d)\n", ret, err_count);
        }
        i++;
        if (loop_count == 0 || i < loop_count) sleep(2);
    }
#else
    printf("Preview-Capture workflow test is only supported on ISP35, skipping test.\n");
#endif

    printf("\n==================================================\n");
    printf("  Preview-Capture Workflow Test COMPLETE\n");
    printf("  total loops: %d, last ret: %d, total errors: %d\n", i, ret, err_count);
    printf("==================================================\n");
}

XCamReturn sample_sysctl(const void *arg)
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

    do {
        sample_sysctl_usage ();

        key = getchar ();
        while (key == '\n' || key == '\r')
            key = getchar();
        printf ("\n");

        switch (key) {
            case '0': {
                printf("\t sample_pause_resume\n\n");
                sample_pause_resume(ctx);
                break;
            }
            case '1': {
                printf("\t sample_mirrflip\n\n");
                sample_mirrflip(ctx);
                break;
            }
            case '2': {
                printf("\t sample_getHdrComprCurve\n\n");
                sample_getHdrComprCurve(ctx);
                break;
            }
            case '3': {
                printf("\t sample_testPreviewCaptureWorkflow\n\n");
                sample_testPreviewCaptureWorkflow(demo_ctx);
                break;
            }
            default:
                break;
        }
    } while (key != 'q' && key != 'Q');

    return XCAM_RETURN_NO_ERROR;
}
