/*
 * Copyright (c) 2026-2030 Rockchip Eletronics Co., Ltd.
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

#include "rk_aiq_user_api2_extproc.h"

#include "RkAibnrManager.h"
#include "c_base/aiq_base.h"
#include "c_base/aiq_list.h"
#include "c_base/aiq_pool.h"
#include "hwi_c/aiq_CamHwBase.h"
#include "hwi_c/aiq_camHw.h"
#include "include/common/rkisp2-config.h"
#include "uAPI2_c/rk_aiq_api_private_c.h"
#include "xcam_log.h"

#define EXTPROC_MAX_INFLIGHT 4

typedef struct rk_aiq_extproc_ctx_s rk_aiq_extproc_ctx_t;
typedef struct rk_aiq_extproc_req_s rk_aiq_extproc_req_t;

/**
 * struct rk_aiq_extproc_req_s - Inflight request for external ISP processing
 *
 * One instance per frame delivered to the user callback; storage is usually
 * backed by struct rk_aiq_extproc_ctx_s::req_pool (BTNR IIR path today).
 *
 * @type: Buffer type (see enum rk_aiq_extproc_type_e)
 * @result_queued: Set after rk_aiq_uapi2_extProc_queueResult()
 * @aiisp_idxbuf: Indices and sequence from the ISP AIISP event
 * @pool_item: Pool handle when this request comes from req_pool
 * @p_mutex: Same lock as struct rk_aiq_extproc_ctx_s::p_mutex (sysctx API mutex)
 * @ext_ctx: Owning extproc context
 * @buf: Buffer descriptor passed to the rk_aiq_extproc_cb callback
 */
struct rk_aiq_extproc_req_s {
    rk_aiq_extproc_type_t type;
    bool result_queued;
    struct rkisp_aiisp_st aiisp_idxbuf;
    AiqPoolItem_t* pool_item;
    AiqMutex_t* p_mutex;
    rk_aiq_extproc_ctx_t* ext_ctx;
    rk_aiq_extproc_buffer_t buf;
};

/**
 * struct rk_aiq_extproc_ctx_s - Private state for rk_aiq_uapi2_extProc_*
 *
 * The active context pointer is stored in %AiqManager_t.mExtProcCtx so the
 * ISP poll path can dispatch without holding rk_aiq_sys_ctx_t.
 *
 * @enable_mask: Bitmask of enabled types (enum rk_aiq_extproc_type_e)
 * @max_inflight: Cap on concurrent struct rk_aiq_extproc_req_s instances
 * @inflight: Number of requests currently listed in @req_list
 * @cb: Application callback for new buffers (WR linecnt)
 * @user_data: Opaque pointer passed to @cb
 * @has_raw_cfg: True if @raw_cfg was filled at init
 * @has_btnr_iir_cfg: True if @btnr_iir_cfg was supplied at init
 * @raw_cfg: Stashed RAW configuration (future use)
 * @btnr_iir_cfg: Stashed Bay3D IIR configuration
 * @req_list: Inflight struct rk_aiq_extproc_req_s pointers
 * @req_pool: Fixed pool of request objects
 * @p_mutex: Typically struct rk_aiq_sys_ctx_s::_apiMutex
 * @p_btnr_drv_buf: Cached BNR/AIISP DMA view from CamHw
 * @camHw: CamHw instance for linecnt and ISP BE trigger
 * @aiisp_buf_slot_busy: Tracks which AIISP output slots are in flight
 * @isp_acq_width: ISP acquire width (e.g. from sensor mode at init)
 * @isp_acq_height: ISP acquire height
 * @prepare: Program hardware for enabled extproc types (dispatches per-type helpers)
 */
struct rk_aiq_extproc_ctx_s {
    uint32_t enable_mask;
    uint32_t max_inflight;
    uint32_t inflight;
    rk_aiq_extproc_cb cb;
    void* user_data;
    bool has_raw_cfg;
    bool has_btnr_iir_cfg;
    rk_aiq_extproc_raw_cfg_t raw_cfg;
    rk_aiq_extproc_btnr_iir_cfg_t btnr_iir_cfg;
    AiqList_t* req_list;
    AiqPool_t* req_pool;
    AiqMutex_t* p_mutex;
    AiqCamHwBnrDrvBuf_t* p_btnr_drv_buf;
    AiqCamHwBase_t* camHw;
    bool aiisp_buf_slot_busy[AIBNR_AIISP_BUF_CNT];
    uint32_t isp_acq_width;
    uint32_t isp_acq_height;

    XCamReturn (*prepare)(rk_aiq_extproc_ctx_t* ext_ctx, uint32_t width, uint32_t height);
};

static void _extproc_set_ctx(rk_aiq_sys_ctx_t* ctx, rk_aiq_extproc_ctx_t* ext_ctx) {
    if (!ctx || !ctx->_rkAiqManager) return;

    ctx->_rkAiqManager->mExtProcCtx = ext_ctx;
}

static void _extproc_put_req(rk_aiq_extproc_req_t* req) {
    AiqPoolItem_t* pool_item = NULL;

    if (!req) return;

    pool_item = req->pool_item;

    if (pool_item) {
        aiqPoolItem_unref(pool_item);
        return;
    }

    aiq_free(req);
}

static rk_aiq_extproc_req_t* _extproc_find_req_locked(rk_aiq_extproc_ctx_t* ext_ctx,
                                                      const void* priv) {
    AiqListItem_t* item       = NULL;
    rk_aiq_extproc_req_t* req = NULL;

    if (!ext_ctx || !ext_ctx->req_list || !priv) return NULL;

    aiqMutex_lock(&ext_ctx->req_list->_mutex);
    item = ext_ctx->req_list->_used_list;
    while (item) {
        req = *(rk_aiq_extproc_req_t**)item->_pData;
        if ((const void*)req == priv) break;

        item = item->_pNext;
        if (item == ext_ctx->req_list->_used_list) item = NULL;
    }
    aiqMutex_unlock(&ext_ctx->req_list->_mutex);

    return req;
}

static rk_aiq_extproc_req_t* _extproc_detach_req_locked(rk_aiq_extproc_ctx_t* ext_ctx,
                                                        const void* priv) {
    AiqListItem_t* item       = NULL;
    rk_aiq_extproc_req_t* req = NULL;

    if (!ext_ctx || !ext_ctx->req_list || !priv) return NULL;

    aiqMutex_lock(&ext_ctx->req_list->_mutex);
    item = ext_ctx->req_list->_used_list;
    while (item) {
        req = *(rk_aiq_extproc_req_t**)item->_pData;
        if ((const void*)req == priv) {
            aiqList_erase_item_locked(ext_ctx->req_list, item);
            aiqMutex_unlock(&ext_ctx->req_list->_mutex);
            return req;
        }

        item = item->_pNext;
        if (item == ext_ctx->req_list->_used_list) item = NULL;
    }
    aiqMutex_unlock(&ext_ctx->req_list->_mutex);

    return NULL;
}

static rk_aiq_extproc_req_t* _extproc_detach_req_for_rd_locked(rk_aiq_extproc_ctx_t* ext_ctx,
                                                               const struct rkisp_aiisp_st* rd) {
    AiqListItem_t* item       = NULL;
    rk_aiq_extproc_req_t* req = NULL;

    if (!ext_ctx || !ext_ctx->req_list || !rd) return NULL;

    aiqMutex_lock(&ext_ctx->req_list->_mutex);
    item = ext_ctx->req_list->_used_list;
    while (item) {
        req = *(rk_aiq_extproc_req_t**)item->_pData;
        if (req && req->type == RK_AIQ_EXTPROC_TYPE_BTNR_IIR &&
            req->aiisp_idxbuf.sequence == rd->sequence &&
            req->aiisp_idxbuf.aiisp_index == rd->aiisp_index) {
            aiqList_erase_item_locked(ext_ctx->req_list, item);
            aiqMutex_unlock(&ext_ctx->req_list->_mutex);
            return req;
        }

        item = item->_pNext;
        if (item == ext_ctx->req_list->_used_list) item = NULL;
    }
    aiqMutex_unlock(&ext_ctx->req_list->_mutex);

    return NULL;
}

static void _extproc_cleanup_locked(rk_aiq_sys_ctx_t* ctx) {
    rk_aiq_extproc_ctx_t* ext_ctx = NULL;
    rk_aiq_extproc_req_t* req     = NULL;
    AiqListItem_t* item           = NULL;
    AiqPool_t* req_pool           = NULL;

    if (!ctx || !ctx->_rkAiqManager) return;

    ext_ctx = (rk_aiq_extproc_ctx_t*)ctx->_rkAiqManager->mExtProcCtx;

    ctx->_rkAiqManager->mExtProcCtx       = NULL;
    ctx->_rkAiqManager->mExtProcBtnrIirCb = NULL;

    if (!ext_ctx && !ctx->_camHw) return;

    if (ext_ctx) {
        req_pool          = ext_ctx->req_pool;
        ext_ctx->req_pool = NULL;
        while (ext_ctx->req_list && (item = aiqList_get_item(ext_ctx->req_list, NULL))) {
            req = *(rk_aiq_extproc_req_t**)item->_pData;
            aiqList_erase_item(ext_ctx->req_list, item);
            _extproc_put_req(req);
        }

        if (ext_ctx->req_list) aiqList_deinit(ext_ctx->req_list);
    }

#if RKAIQ_HAVE_AIBNR
    {
        /* deinit only when removing a BTNR extproc session, or stray inited hw; not on first init.
         */
        bool outgoing_btnr = ext_ctx && (ext_ctx->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK);
        bool stray_btnr_hw = !ext_ctx && ctx->_camHw && ctx->_camHw->mBtnrIirExtproc.inited;

        if ((outgoing_btnr || stray_btnr_hw) && ctx->_camHw &&
            ctx->_camHw->mBtnrIirExtproc.inited && ctx->_camHw->mBtnrIirExtproc.deinit)
            ctx->_camHw->mBtnrIirExtproc.deinit(ctx->_camHw);
    }
#endif

    if (ext_ctx) aiq_free(ext_ctx);
    if (req_pool) aiqPool_deinit(req_pool);
}

/* Caller holds ext_ctx->p_mutex; always unlocks before return. */
static XCamReturn _extproc_btnr_iir_handle_rd_linecnt(rk_aiq_extproc_ctx_t* ext_ctx,
                                                      AiqHwAinnEvt_t* ainn_data) {
    struct rkisp_aiisp_st* isp_idxbuf   = (struct rkisp_aiisp_st*)&ainn_data->queue_buf.aibnr_st;
    struct rkisp_bnr_buf_info* bay3dbuf = NULL;
    rk_aiq_extproc_req_t* stale_req     = NULL;

    if (isp_idxbuf->aiisp_index < 0) {
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_NO_ERROR;
    }
    bay3dbuf = &ext_ctx->p_btnr_drv_buf->_bay3dbuf;
    if (isp_idxbuf->aiisp_index >= bay3dbuf->u.v35.aiisp.buf_cnt) {
        LOGE_AIBNR("%s: RD aiisp_index %d out of range %d", __func__, isp_idxbuf->aiisp_index,
                   bay3dbuf->u.v35.aiisp.buf_cnt);
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_ERROR_FAILED;
    }

    stale_req = _extproc_detach_req_for_rd_locked(ext_ctx, isp_idxbuf);
    if (stale_req) {
        if (ext_ctx->inflight) ext_ctx->inflight--;
        {
            int ai = stale_req->aiisp_idxbuf.aiisp_index;
            if (ai >= 0 && ai < AIBNR_AIISP_BUF_CNT) ext_ctx->aiisp_buf_slot_busy[ai] = false;
        }
    }
    aiqMutex_unlock(ext_ctx->p_mutex);
    _extproc_put_req(stale_req);
    LOGD_AIBNR("%s: sequence %d, return aiisp buf index %d to pool", __func__, isp_idxbuf->sequence,
               isp_idxbuf->aiisp_index);

    return XCAM_RETURN_NO_ERROR;
}

/* Caller holds ext_ctx->p_mutex; unlocks before user cb and on all early exits. */
static XCamReturn _extproc_btnr_iir_handle_wr_linecnt(rk_aiq_extproc_ctx_t* ext_ctx,
                                                      AiqHwAinnEvt_t* ainn_data) {
    rk_aiq_extproc_req_t* req           = NULL;
    struct rkisp_aiisp_st* isp_idxbuf   = NULL;
    struct rkisp_bnr_buf_info* bay3dbuf = NULL;
    rk_aiq_extproc_cb cb                = NULL;
    void* user_data                     = NULL;
    XCamReturn ret                      = XCAM_RETURN_NO_ERROR;

    if ((ext_ctx->max_inflight && ext_ctx->inflight >= ext_ctx->max_inflight) || !ext_ctx->cb) {
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_NO_ERROR;
    }

    {
        AiqPool_t* wr_pool = ext_ctx->req_pool;
        AiqPoolItem_t* _pi = wr_pool ? aiqPool_getFree(wr_pool) : NULL;
        if (!_pi) {
            aiqMutex_unlock(ext_ctx->p_mutex);
            return XCAM_RETURN_NO_ERROR;
        }
        req = AIQPOOLITEMCAST(_pi, rk_aiq_extproc_req_t);
        memset(req, 0, sizeof(*req));
        req->pool_item = _pi;
        req->p_mutex   = ext_ctx->p_mutex;
        req->ext_ctx   = ext_ctx;
    }

    if (!ext_ctx->p_btnr_drv_buf) {
        LOGE_AIBNR("%s: setIspBufInf failed or not supported", __func__);
        _extproc_put_req(req);
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_ERROR_FAILED;
    }

    isp_idxbuf = &ainn_data->queue_buf.aibnr_st;
    bay3dbuf   = &ext_ctx->p_btnr_drv_buf->_bay3dbuf;

    if (isp_idxbuf->iir_index < 0 || isp_idxbuf->iir_index >= bay3dbuf->iir.buf_cnt ||
        isp_idxbuf->gain_index < 0 || isp_idxbuf->gain_index >= bay3dbuf->u.v35.gain.buf_cnt) {
        LOGE_AIBNR(
            "%s: sequence %d, iir_index %d invalid, range [0, %d), gain_index %d invalid, "
            "range [0, %d)",
            __func__, isp_idxbuf->sequence, isp_idxbuf->iir_index, bay3dbuf->iir.buf_cnt,
            isp_idxbuf->gain_index, bay3dbuf->u.v35.gain.buf_cnt);
        aiqMutex_unlock(ext_ctx->p_mutex);
        _extproc_put_req(req);
        return XCAM_RETURN_NO_ERROR;
    }

    if (isp_idxbuf->aiisp_index < 0 || isp_idxbuf->aiisp_index >= bay3dbuf->u.v35.aiisp.buf_cnt) {
        int buf_cnt  = bay3dbuf->u.v35.aiisp.buf_cnt;
        int free_idx = -1;
        int i;

        for (i = 0; i < buf_cnt; i++) {
            if (!ext_ctx->aiisp_buf_slot_busy[i]) {
                free_idx = i;
                break;
            }
        }
        if (free_idx < 0) {
            free_idx = 0;
            LOGD_AIBNR("%s: sequence %d, no free aiisp buf, use buf 0", __func__,
                       isp_idxbuf->sequence);
        }
        isp_idxbuf->aiisp_index = free_idx;
        LOGD_AIBNR("%s: sequence %d, assign aiisp buf index %d from pool", __func__,
                   isp_idxbuf->sequence, isp_idxbuf->aiisp_index);
    }

    req->type                        = RK_AIQ_EXTPROC_TYPE_BTNR_IIR;
    req->aiisp_idxbuf                = *isp_idxbuf;
    req->buf.type                    = RK_AIQ_EXTPROC_TYPE_BTNR_IIR;
    req->buf.frame_id                = isp_idxbuf->sequence;
    req->buf.timestamp               = isp_idxbuf->timestamp;
    req->buf.priv                    = req;
    req->buf.u.btnr_iir.iir.dma_fd   = bay3dbuf->iir.buf_fd[isp_idxbuf->iir_index];
    req->buf.u.btnr_iir.iir.vir_addr = ext_ctx->p_btnr_drv_buf->iir_address[isp_idxbuf->iir_index];
    req->buf.u.btnr_iir.iir.size     = bay3dbuf->iir.buf_size;
    req->buf.u.btnr_iir.iir.width    = ext_ctx->isp_acq_width;
    req->buf.u.btnr_iir.iir.height   = ext_ctx->isp_acq_height;
    req->buf.u.btnr_iir.iir.stride   = bay3dbuf->iir.buf_stride;
    req->buf.u.btnr_iir.iir.format   = bay3dbuf->u.v35.iir_rw_fmt;
    req->buf.u.btnr_iir.gain.dma_fd  = bay3dbuf->u.v35.gain.buf_fd[isp_idxbuf->gain_index];
    req->buf.u.btnr_iir.gain.vir_addr =
        ext_ctx->p_btnr_drv_buf->gain_address[isp_idxbuf->gain_index];
    req->buf.u.btnr_iir.gain.size   = bay3dbuf->u.v35.gain.buf_size;
    req->buf.u.btnr_iir.gain.stride = bay3dbuf->u.v35.gain.buf_stride;

    req->buf.u.btnr_iir.iir_out.dma_fd = bay3dbuf->u.v35.aiisp.buf_fd[isp_idxbuf->aiisp_index];
    req->buf.u.btnr_iir.iir_out.vir_addr =
        ext_ctx->p_btnr_drv_buf->aiisp_address[isp_idxbuf->aiisp_index];
    req->buf.u.btnr_iir.iir_out.size   = bay3dbuf->u.v35.aiisp.buf_size;
    req->buf.u.btnr_iir.iir_out.width  = ext_ctx->isp_acq_width;
    req->buf.u.btnr_iir.iir_out.height = ext_ctx->isp_acq_height;
    req->buf.u.btnr_iir.iir_out.stride = bay3dbuf->u.v35.aiisp.buf_stride;
    req->buf.u.btnr_iir.iir_out.format = bay3dbuf->u.v35.iir_rw_fmt;

    if (aiqList_push(ext_ctx->req_list, &req) != 0) {
        aiqMutex_unlock(ext_ctx->p_mutex);
        _extproc_put_req(req);
        return XCAM_RETURN_NO_ERROR;
    }
    ext_ctx->inflight++;
    if (isp_idxbuf->aiisp_index >= 0 && isp_idxbuf->aiisp_index < RKISP_BUFFER_MAX)
        ext_ctx->aiisp_buf_slot_busy[isp_idxbuf->aiisp_index] = true;
    cb        = ext_ctx->cb;
    user_data = ext_ctx->user_data;
    aiqMutex_unlock(ext_ctx->p_mutex);

    LOGD_AIBNR("%s: seq %d iir_idx %d gain_idx %d aiisp_idx %d", __func__, isp_idxbuf->sequence,
               isp_idxbuf->iir_index, isp_idxbuf->gain_index, isp_idxbuf->aiisp_index);
    LOGD_AIBNR("%s: iir(%dx%d s%d fmt%d fd%d %p) gain(fd%d %p) iir_out(fd%d %p)", __func__,
               req->buf.u.btnr_iir.iir.width, req->buf.u.btnr_iir.iir.height,
               req->buf.u.btnr_iir.iir.stride, req->buf.u.btnr_iir.iir.format,
               req->buf.u.btnr_iir.iir.dma_fd, req->buf.u.btnr_iir.iir.vir_addr,
               req->buf.u.btnr_iir.gain.dma_fd, req->buf.u.btnr_iir.gain.vir_addr,
               req->buf.u.btnr_iir.iir_out.dma_fd, req->buf.u.btnr_iir.iir_out.vir_addr);

    ret = cb(&req->buf, user_data);
    if (ret == XCAM_RETURN_NO_ERROR) return XCAM_RETURN_BYPASS;

    LOGE_AIBNR("%s: user cb failed seq %d ret %d, rollback req", __func__, isp_idxbuf->sequence,
               ret);
    aiqMutex_lock(ext_ctx->p_mutex);
    {
        rk_aiq_extproc_req_t* detached = _extproc_detach_req_locked(ext_ctx, req);
        if (detached) {
            if (ext_ctx->inflight) ext_ctx->inflight--;
            {
                int ai = detached->aiisp_idxbuf.aiisp_index;
                if (ai >= 0 && ai < AIBNR_AIISP_BUF_CNT) ext_ctx->aiisp_buf_slot_busy[ai] = false;
            }
        } else {
            LOGE_AIBNR("%s: seq %d cb failed but req not in list", __func__, isp_idxbuf->sequence);
        }
        aiqMutex_unlock(ext_ctx->p_mutex);
        _extproc_put_req(detached);
    }

    return XCAM_RETURN_NO_ERROR;
}

static XCamReturn _extproc_dispatch_btnr_iir(void* dispatch_ctx, AiqHwEvt_t* hwres) {
    rk_aiq_extproc_ctx_t* ext_ctx = (rk_aiq_extproc_ctx_t*)dispatch_ctx;
    AiqHwAinnEvt_t* ainn_data     = (AiqHwAinnEvt_t*)hwres;

    if (!ext_ctx || !hwres || !ext_ctx->p_mutex) return XCAM_RETURN_NO_ERROR;

    LOGD_AIBNR("%s: event type %d", __func__, ainn_data->_event_id);

    aiqMutex_lock(ext_ctx->p_mutex);
    if (!(ext_ctx->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK) || !ext_ctx->camHw) {
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_NO_ERROR;
    }

    if (!ext_ctx->p_btnr_drv_buf && ext_ctx->camHw->mBtnrIirExtproc.setIspBufInf)
        ext_ctx->camHw->mBtnrIirExtproc.setIspBufInf(ext_ctx->camHw, &ext_ctx->p_btnr_drv_buf);

    if (ainn_data->_event_id == RKISP_AIISP_RD_LINECNT_ID)
        return _extproc_btnr_iir_handle_rd_linecnt(ext_ctx, ainn_data);
    if (ainn_data->_event_id == RKISP_AIISP_WR_LINECNT_ID)
        return _extproc_btnr_iir_handle_wr_linecnt(ext_ctx, ainn_data);

    aiqMutex_unlock(ext_ctx->p_mutex);
    return XCAM_RETURN_NO_ERROR;
}

#if RKAIQ_HAVE_AIBNR
static XCamReturn _extproc_prepare_btnr_iir(rk_aiq_extproc_ctx_t* ext_ctx, uint32_t width,
                                            uint32_t height) {
    struct rkisp_aiisp_cfg btnr_iir_extproc_cfg;
    AiqCamHwBase_t* camHw;

    (void)width;

    if (!ext_ctx || !(ext_ctx->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK))
        return XCAM_RETURN_NO_ERROR;

    camHw = ext_ctx->camHw;
    if (!camHw || !camHw->mBtnrIirExtproc.init) return XCAM_RETURN_NO_ERROR;

    if (!ext_ctx->has_btnr_iir_cfg) {
        btnr_iir_extproc_cfg.mode       = 1;
        btnr_iir_extproc_cfg.wr_linecnt = (int)height;
        btnr_iir_extproc_cfg.rd_linecnt = (int)height;
        btnr_iir_extproc_cfg.wr_mode    = 0;
    } else {
        btnr_iir_extproc_cfg.mode       = ext_ctx->btnr_iir_cfg.mode;
        btnr_iir_extproc_cfg.wr_linecnt = ext_ctx->btnr_iir_cfg.wr_linecnt;
        btnr_iir_extproc_cfg.rd_linecnt = ext_ctx->btnr_iir_cfg.rd_linecnt;
        btnr_iir_extproc_cfg.wr_mode    = ext_ctx->btnr_iir_cfg.wr_mode;
    }

    return camHw->mBtnrIirExtproc.init(camHw, &btnr_iir_extproc_cfg);
}
#endif

static XCamReturn _extproc_prepare(rk_aiq_extproc_ctx_t* ext_ctx, uint32_t width, uint32_t height) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (!ext_ctx) return XCAM_RETURN_ERROR_PARAM;

#if RKAIQ_HAVE_AIBNR
    if (ext_ctx->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK) {
        ret = _extproc_prepare_btnr_iir(ext_ctx, width, height);
        if (ret != XCAM_RETURN_NO_ERROR) return ret;
    }
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn rk_aiq_uapi2_extProc_init(rk_aiq_sys_ctx_t* ctx,
                                     const rk_aiq_extproc_init_params_t* params) {
    rk_aiq_extproc_ctx_t* ext_ctx = NULL;
    AiqPool_t* req_pool           = NULL;
    XCamReturn ret                = XCAM_RETURN_NO_ERROR;
    uint32_t prep_w               = 0;
    uint32_t prep_h               = 0;

    if (!ctx || !ctx->_rkAiqManager || !params || !params->cb || !params->enable_mask)
        return XCAM_RETURN_ERROR_PARAM;

    if (params->enable_mask & RK_AIQ_EXTPROC_TYPE_BAYER_RAW_MASK) {
        LOGE("%s: BAYER_RAW extproc is not supported\n", __func__);
        return XCAM_RETURN_ERROR_PARAM;
    }

    ext_ctx = (rk_aiq_extproc_ctx_t*)aiq_mallocz(sizeof(*ext_ctx));
    if (!ext_ctx) return XCAM_RETURN_ERROR_MEM;

    ext_ctx->enable_mask  = params->enable_mask;
    ext_ctx->max_inflight = EXTPROC_MAX_INFLIGHT;
    ext_ctx->cb           = params->cb;
    ext_ctx->user_data    = params->user_data;

    {
        AiqListConfig_t req_list_cfg;

        req_list_cfg._name      = "ExtProcReqList";
        req_list_cfg._item_size = sizeof(rk_aiq_extproc_req_t*);
        req_list_cfg._item_nums = ext_ctx->max_inflight;
        ext_ctx->req_list       = aiqList_init(&req_list_cfg);
        if (!ext_ctx->req_list) {
            aiq_free(ext_ctx);
            return XCAM_RETURN_ERROR_MEM;
        }
    }

    {
        AiqPoolConfig_t req_pool_cfg;

        req_pool_cfg._name      = "ExtProcReqPool";
        req_pool_cfg._item_size = sizeof(rk_aiq_extproc_req_t);
        req_pool_cfg._item_nums = ext_ctx->max_inflight;
        req_pool                = aiqPool_init(&req_pool_cfg);
        if (!req_pool) {
            aiqList_deinit(ext_ctx->req_list);
            aiq_free(ext_ctx);
            return XCAM_RETURN_ERROR_MEM;
        }
    }

    if (params->raw_cfg) {
        ext_ctx->raw_cfg     = *params->raw_cfg;
        ext_ctx->has_raw_cfg = true;
    }
    if (params->btnr_iir_cfg) {
        ext_ctx->btnr_iir_cfg     = *params->btnr_iir_cfg;
        ext_ctx->has_btnr_iir_cfg = true;
    }

    ext_ctx->camHw = ctx->_camHw;

#if RKAIQ_HAVE_AIBNR
    if ((params->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK) && ext_ctx->camHw &&
        ext_ctx->camHw->mBtnrIirExtproc.init) {
        rk_aiq_exposure_sensor_descriptor sensor_des;

        if (AiqCamHw_getSensorModeData(ctx->_camHw, ctx->_sensor_entity_name, &sensor_des) ==
            XCAM_RETURN_NO_ERROR) {
            ext_ctx->isp_acq_width  = sensor_des.isp_acq_width;
            ext_ctx->isp_acq_height = sensor_des.isp_acq_height;
            if (!params->btnr_iir_cfg) {
                prep_w = sensor_des.isp_acq_width;
                prep_h = sensor_des.isp_acq_height;
            }
        }
    }
#endif

    if (params->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK)
        ext_ctx->prepare = _extproc_prepare;

    ext_ctx->p_mutex  = &ctx->_apiMutex;
    ext_ctx->req_pool = req_pool;

    aiqMutex_lock(&ctx->_apiMutex);
    _extproc_cleanup_locked(ctx);
    _extproc_set_ctx(ctx, ext_ctx);
    if (ctx->_rkAiqManager) {
        ctx->_rkAiqManager->mExtProcBtnrIirCb =
            (params->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK) ? _extproc_dispatch_btnr_iir
                                                                      : NULL;
    }

#if RKAIQ_HAVE_AIBNR
    if ((params->enable_mask & RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK) && ext_ctx->camHw &&
        ext_ctx->camHw->mBtnrIirExtproc.init) {
        ret = _extproc_prepare_btnr_iir(ext_ctx, prep_w, prep_h);
        if (ret != XCAM_RETURN_NO_ERROR) {
            ctx->_rkAiqManager->mExtProcCtx       = NULL;
            ctx->_rkAiqManager->mExtProcBtnrIirCb = NULL;
            aiqMutex_unlock(&ctx->_apiMutex);
            if (req_pool) aiqPool_deinit(req_pool);
            aiqList_deinit(ext_ctx->req_list);
            aiq_free(ext_ctx);
            return ret;
        }
    }
#endif

    aiqMutex_unlock(&ctx->_apiMutex);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqExtproc_prepare(void* extproc_ctx, uint32_t width, uint32_t height) {
    rk_aiq_extproc_ctx_t* ext_ctx = (rk_aiq_extproc_ctx_t*)extproc_ctx;

    if (!ext_ctx || !ext_ctx->prepare) return XCAM_RETURN_NO_ERROR;

    ext_ctx->isp_acq_width  = width;
    ext_ctx->isp_acq_height = height;

    return ext_ctx->prepare(ext_ctx, width, height);
}

static XCamReturn _extproc_queue_btnr_iir_result(rk_aiq_extproc_req_t* req,
                                                 const rk_aiq_extproc_result_t* result) {
    struct rkisp_aiisp_st* isp_idxbuf = &req->aiisp_idxbuf;
    AiqCamHwBase_t* camHw             = req->ext_ctx ? req->ext_ctx->camHw : NULL;

    LOGD_AIBNR("%s: seq %d iir_idx %d gain_idx %d aiisp_idx %d", __func__, isp_idxbuf->sequence,
               isp_idxbuf->iir_index, isp_idxbuf->gain_index, isp_idxbuf->aiisp_index);

    req->result_queued = true;

#if RKAIQ_HAVE_AIBNR
    if (camHw && camHw->mBtnrIirExtproc.doIspBe)
        return camHw->mBtnrIirExtproc.doIspBe(camHw, isp_idxbuf);
#endif
    return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn rk_aiq_uapi2_extProc_queueResult(const rk_aiq_extproc_result_t* result) {
    rk_aiq_extproc_req_t* req     = NULL;
    rk_aiq_extproc_ctx_t* ext_ctx = NULL;
    XCamReturn ret                = XCAM_RETURN_NO_ERROR;

    if (!result || !result->priv) return XCAM_RETURN_ERROR_PARAM;

    req = (rk_aiq_extproc_req_t*)result->priv;
    if (!req->p_mutex || !req->ext_ctx) return XCAM_RETURN_ERROR_PARAM;

    aiqMutex_lock(req->p_mutex);
    ext_ctx = req->ext_ctx;
    req     = _extproc_find_req_locked(ext_ctx, result->priv);
    if (!req || req->type != result->type || req->result_queued) {
        aiqMutex_unlock(ext_ctx->p_mutex);
        return XCAM_RETURN_ERROR_PARAM;
    }

    if (result->type == RK_AIQ_EXTPROC_TYPE_BAYER_RAW)
        ret = XCAM_RETURN_ERROR_PARAM;
    else if (result->type == RK_AIQ_EXTPROC_TYPE_BTNR_IIR)
        ret = _extproc_queue_btnr_iir_result(req, result);
    else
        ret = XCAM_RETURN_ERROR_PARAM;

    aiqMutex_unlock(ext_ctx->p_mutex);

    return ret;
}

XCamReturn rk_aiq_uapi2_extProc_releaseBuffer(const rk_aiq_extproc_buffer_t* buf) {
    rk_aiq_extproc_req_t* req     = NULL;
    rk_aiq_extproc_ctx_t* ext_ctx = NULL;

    if (!buf || !buf->priv) return XCAM_RETURN_ERROR_PARAM;

    req = (rk_aiq_extproc_req_t*)buf->priv;
    if (!req->p_mutex || !req->ext_ctx) return XCAM_RETURN_NO_ERROR;

    aiqMutex_lock(req->p_mutex);
    ext_ctx = req->ext_ctx;
    req     = _extproc_detach_req_locked(ext_ctx, buf->priv);
    if (req) {
        if (ext_ctx->inflight) ext_ctx->inflight--;
        {
            int ai = req->aiisp_idxbuf.aiisp_index;
            if (ai >= 0 && ai < AIBNR_AIISP_BUF_CNT) ext_ctx->aiisp_buf_slot_busy[ai] = false;
        }
    }
    aiqMutex_unlock(ext_ctx->p_mutex);

    if (!req) return XCAM_RETURN_NO_ERROR;

    _extproc_put_req(req);
    return XCAM_RETURN_NO_ERROR;
}
