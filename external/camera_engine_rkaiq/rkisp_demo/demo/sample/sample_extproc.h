#ifndef _SAMPLE_EXTPROC_H_
#define _SAMPLE_EXTPROC_H_

#include "uAPI2/rk_aiq_user_api2_extproc.h"
#include "xcore/base/xcam_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sample_extproc_stats_s {
    int inited;
    unsigned int cb_count;
    unsigned int queue_count;
    unsigned int error_count;
    unsigned int dump_frames; /* number of frames left to dump, 0 = disabled */
} sample_extproc_stats_t;

extern sample_extproc_stats_t g_sample_extproc_stats;

XCamReturn sample_extproc_bay3d_iir_cb(const rk_aiq_extproc_buffer_t* buf, void* user_data);

XCamReturn sample_extproc_bay3dnr_init(rk_aiq_sys_ctx_t* ctx);

void sample_extproc_set_dump_frames(unsigned int n);

XCamReturn sample_extproc(const void* arg);

#ifdef __cplusplus
}
#endif

#endif
