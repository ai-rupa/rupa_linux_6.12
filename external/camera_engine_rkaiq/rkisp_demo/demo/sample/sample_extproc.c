#include "sample_extproc.h"

#include <linux/dma-buf.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "rkisp_demo.h"
#include "uAPI2/rk_aiq_user_api2_extproc.h"

static void sample_extproc_copy_mem(const rk_aiq_extproc_mem_t* src,
                                    const rk_aiq_extproc_mem_t* dst) {
    const uint8_t* s   = (const uint8_t*)src->vir_addr;
    uint8_t* d         = (uint8_t*)dst->vir_addr;
    uint32_t row_bytes = src->width * 2;
    uint32_t row;

    if (!s || !d || !src->width || !src->height || !src->stride || !dst->stride) return;

    for (row = 0; row < src->height; row++) {
        memcpy(d, s, row_bytes);
        s += src->stride;
        d += dst->stride;
    }
}

static int sample_extproc_dump_iir_buf(const rk_aiq_extproc_mem_t* mem, const char* filename) {
    const uint8_t* src = (const uint8_t*)mem->vir_addr;
    uint32_t row_bytes = mem->width * 2;
    uint32_t row;
    FILE* fp;

    if (!src || !filename || !mem->width || !mem->height || !mem->stride) return -1;

    fp = fopen(filename, "wb");
    if (!fp) {
        printf("dump_iir_buf: failed to open %s\n", filename);
        return -1;
    }

    for (row = 0; row < mem->height; row++) {
        if (fwrite(src, 1, row_bytes, fp) != row_bytes) {
            printf("dump_iir_buf: write error at row %u\n", row);
            fclose(fp);
            return -1;
        }
        src += mem->stride;
    }

    fclose(fp);
    return 0;
}

sample_extproc_stats_t g_sample_extproc_stats;

void sample_extproc_set_dump_frames(unsigned int n) { g_sample_extproc_stats.dump_frames = n; }

static rk_aiq_sys_ctx_t* sample_extproc_get_aiq_ctx(const demo_context_t* demo_ctx) {
    if (demo_ctx->camGroup) return (rk_aiq_sys_ctx_t*)demo_ctx->camgroup_ctx;

    return (rk_aiq_sys_ctx_t*)demo_ctx->aiq_ctx;
}

XCamReturn sample_extproc_bay3d_iir_cb(const rk_aiq_extproc_buffer_t* buf, void* user_data) {
    sample_extproc_stats_t* sample_stats = (sample_extproc_stats_t*)user_data;
    rk_aiq_extproc_result_t result;
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (!buf || !sample_stats) return XCAM_RETURN_ERROR_PARAM;

    sample_stats->cb_count++;

    if (buf->type != RK_AIQ_EXTPROC_TYPE_BTNR_IIR) {
        sample_stats->error_count++;
        return XCAM_RETURN_ERROR_PARAM;
    }

    if (sample_stats->dump_frames > 0) {
        char filename[128];
        const rk_aiq_extproc_mem_t* mem = &buf->u.btnr_iir.iir;
#ifndef ANDROID
        snprintf(filename, sizeof(filename), "/tmp/frame%u_btnr_iir_%ux%u.bin", buf->frame_id,
                 mem->width, mem->height);
#else
        snprintf(filename, sizeof(filename), "/data/frame%u_btnr_iir_%ux%u.bin", buf->frame_id,
                 mem->width, mem->height);
#endif
        sample_extproc_dump_iir_buf(mem, filename);
        sample_stats->dump_frames--;
    }

    {
        struct dma_buf_sync sync;

        sync.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_START;
        ioctl(buf->u.btnr_iir.iir.dma_fd, DMA_BUF_IOCTL_SYNC, &sync);
        ioctl(buf->u.btnr_iir.iir_out.dma_fd, DMA_BUF_IOCTL_SYNC, &sync);

        sample_extproc_copy_mem(&buf->u.btnr_iir.iir, &buf->u.btnr_iir.iir_out);

        sync.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END;
        ioctl(buf->u.btnr_iir.iir.dma_fd, DMA_BUF_IOCTL_SYNC, &sync);
        ioctl(buf->u.btnr_iir.iir_out.dma_fd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    memset(&result, 0, sizeof(result));
    result.type     = RK_AIQ_EXTPROC_TYPE_BTNR_IIR;
    result.frame_id = buf->frame_id;
    result.priv     = buf->priv;

#if USE_NEWSTRUCT
    ret = rk_aiq_uapi2_extProc_queueResult(&result);
    if (ret == XCAM_RETURN_NO_ERROR)
        sample_stats->queue_count++;
    else
        sample_stats->error_count++;
#endif

    return ret;
}

XCamReturn sample_extproc_bay3dnr_init(rk_aiq_sys_ctx_t* ctx) {
    rk_aiq_extproc_init_params_t params;
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (!ctx) return XCAM_RETURN_ERROR_PARAM;

    if (g_sample_extproc_stats.inited) {
        printf("extproc bay3dnr already inited\n");
        return XCAM_RETURN_NO_ERROR;
    }

    memset(&params, 0, sizeof(params));
    params.enable_mask = RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK;
    params.cb          = sample_extproc_bay3d_iir_cb;
    params.user_data   = &g_sample_extproc_stats;

#if USE_NEWSTRUCT
    ret = rk_aiq_uapi2_extProc_init(ctx, &params);
    if (ret != XCAM_RETURN_NO_ERROR) {
        printf("extproc bay3dnr init failed, ret:%d\n", ret);
        return ret;
    }
#endif

    g_sample_extproc_stats.inited = 1;
    printf("extproc bay3dnr init done\n");

    return ret;
}

XCamReturn sample_extproc(const void* arg) {
    int key                        = -1;
    const demo_context_t* demo_ctx = (const demo_context_t*)arg;
    rk_aiq_sys_ctx_t* ctx          = NULL;

    if (!demo_ctx) {
        printf("%s, arg is NULL\n", __func__);
        return XCAM_RETURN_ERROR_PARAM;
    }

    ctx = sample_extproc_get_aiq_ctx(demo_ctx);
    if (!ctx) {
        printf("%s, aiq ctx is NULL\n", __func__);
        return XCAM_RETURN_ERROR_PARAM;
    }

    do {
        printf("Usage : \n");
        printf("\t 0) EXTPROC:       BAY3DNR init.\n");
        printf("\t 1) EXTPROC:       dump next N IIR frames to file.\n");
        printf("\t q) EXTPROC:       quit.\n");
        printf("\n");
        printf("\t please press the key: ");

        key = getchar();
        while (key == '\n' || key == '\r') key = getchar();
        printf("\n");

        switch (key) {
            case '0':
                sample_extproc_bay3dnr_init(ctx);
                break;
            case '1': {
                unsigned int n = 0;
                printf("enter frame count to dump: ");
                if (scanf("%u", &n) == 1) sample_extproc_set_dump_frames(n);
                break;
            }
            default:
                break;
        }
    } while (key != 'q' && key != 'Q');

    memset(&g_sample_extproc_stats, 0, sizeof(g_sample_extproc_stats));

    return XCAM_RETURN_NO_ERROR;
}
