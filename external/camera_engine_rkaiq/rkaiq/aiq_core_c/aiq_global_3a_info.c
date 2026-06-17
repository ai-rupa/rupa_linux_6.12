/*
 * Copyright (c) 2025 Rockchip Eletronics Co., Ltd.
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

#include "aiq_global_3a_info.h"

#include "aiq_base.h"
#include "c_base/aiq_map.h"
#include "c_base/aiq_pool.h"

#define ISP_PARAMS_EFFECT_DELAY_CNT        2
#define AIQ_MASTER_3A_INFO_CACHE_SIZE      5
#define AIQ_MASTER_3A_INFO_EVICT_THRESHOLD (AIQ_MASTER_3A_INFO_CACHE_SIZE - 2)
#define AIQ_MASTER_SYNC_DEFAULT_MASK       ((1ULL << RK_AIQ_CAM_GROUP_MAX_CAMS) - 1)

static const char* const g3aBlockTypeStr[] = {
    [AIQ_G3A_BLOCK_TYPE_AWB_STATS]  = "awb_stats",
    [AIQ_G3A_BLOCK_TYPE_AWB_RESULT] = "awb_result",
};

static inline const char* g3aBlockType2Str(uint16_t type) {
    if (type < AIQ_G3A_BLOCK_TYPE_MAX) return g3aBlockTypeStr[type];
    return "unknown";
}

typedef struct AiqGlobal3AInfoLatestCache_s {
    AiqAwbAlgoResultData_t awb_result;
    /* add more fields if needed */
} AiqGlobal3AInfoLatestCache_t;

typedef struct AiqGlobal3AInfoCamCtx_s {
    uint32_t currentCamId;
    AiqPool_t* pool;
    AiqMap_t* map;
    uint32_t noFreeBufCnt;
    AiqGlobal3AInfoLatestCache_t latest_cache;
} AiqGlobal3AInfoCamCtx_t;

/**
 * @brief Global 3A Info Manager
 *
 * Manages per-camera 3A info for multi-cam synchronization.
 * Uses pool for memory management with ref counting, and map for
 * frame_id based lookup and insertion order tracking.
 *
 * @camCtx: Per-camera pool/map/cache contexts
 * @masterCamId: Master camera ID
 * @syncMask: Bitmask of cameras to sync with master
 * @requiredMask: Required validMask bits for get to succeed
 * @mutex: Mutex to protect all members
 * @inited: Initialization flag
 * @refCnt: Reference count for multi AiqCore instances
 */
typedef struct AiqGlobal3AInfoMgr_s {
    AiqGlobal3AInfoCamCtx_t camCtx[RK_AIQ_CAM_GROUP_MAX_CAMS];
    uint32_t masterCamId;
    uint64_t syncMask;
    uint32_t requiredMask;
    AiqMutex_t mutex;
    bool inited;
    int refCnt;
} AiqGlobal3AInfoMgr_t;

static AiqGlobal3AInfoMgr_t g_3aMgr = {
    .masterCamId  = UINT32_MAX,
    .syncMask     = AIQ_MASTER_SYNC_DEFAULT_MASK,
    .requiredMask = 0,
    .inited       = false,
    .refCnt       = 0,
};

static inline bool g3aCamIdValid(uint32_t camId) { return camId < RK_AIQ_CAM_GROUP_MAX_CAMS; }

static XCamReturn g3aGetLatestInfoLocked(uint32_t camId, AiqGlobal3AInfo_t** out) {
    if (!out || !g3aCamIdValid(camId)) return XCAM_RETURN_ERROR_PARAM;

    *out = NULL;

    AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];
    if (!camCtx->map || aiqMap_size(camCtx->map) == 0) {
        LOGW_ANALYZER("g3A: camId:%u map empty", camId);
        return XCAM_RETURN_ERROR_FAILED;
    }

    // Get the latest (last) element in the map
    AiqMapItem_t* pMapItem = aiqMap_rbegin(camCtx->map);  // get the newest
    if (pMapItem) {
        AiqPoolItem_t* poolItem      = *(AiqPoolItem_t**)pMapItem->_pData;
        AiqGlobal3AInfo_t* candidate = (AiqGlobal3AInfo_t*)poolItem->_pData;
        uint32_t validMask           = candidate->validMask;
        uint32_t requiredMask        = g_3aMgr.requiredMask;
        if ((validMask & requiredMask) == requiredMask) {
            /* Copy awb_result from latest_cache if required */
            if (requiredMask & AIQ_G3A_VALID_AWB_RESULT) {
                memcpy(&candidate->awb_result, &camCtx->latest_cache.awb_result,
                       sizeof(AiqAwbAlgoResultData_t));
            }
            candidate->cam_id = camId;
            *out              = candidate;
            LOGD_ANALYZER("g3A: cam:%u get fid:%u mask:0x%x, map:%d, pool free:%d", camId,
                          candidate->frame_id, candidate->validMask, aiqMap_size(camCtx->map),
                          aiqPool_freeNums(camCtx->pool));
            return XCAM_RETURN_NO_ERROR;
        } else {
            /* Check if only AIQ_G3A_VALID_AWB_RESULT is missing */
            uint32_t missing = requiredMask & ~validMask;
            if (missing == AIQ_G3A_VALID_AWB_RESULT) {
                memcpy(&candidate->awb_result, &camCtx->latest_cache.awb_result,
                       sizeof(AiqAwbAlgoResultData_t));
                candidate->validMask |= AIQ_G3A_VALID_AWB_RESULT;
                candidate->cam_id = camId;
                *out              = candidate;
                LOGD_ANALYZER("g3A: camId:%u patched awb_result from latest_cache for fid:%u",
                              camId, candidate->frame_id);
                return XCAM_RETURN_NO_ERROR;
            } else {
                LOGW_ANALYZER(
                    "g3A: latest fid:%u validMask:0x%x not meet required:0x%x, and not only "
                    "AWB_RESULT missing",
                    candidate->frame_id, validMask, requiredMask);
            }
        }
    }
    LOGW_ANALYZER("g3A: camId:%u no suitable item in map", camId);
    return XCAM_RETURN_ERROR_FAILED;
}

static void g3aCamCtxReset(AiqGlobal3AInfoCamCtx_t* camCtx) {
    if (!camCtx) return;
    camCtx->currentCamId = UINT32_MAX;
    camCtx->noFreeBufCnt = 0;
    memset(&camCtx->latest_cache, 0, sizeof(camCtx->latest_cache));
}

static void g3aCamCtxDeinit(AiqGlobal3AInfoCamCtx_t* camCtx) {
    if (!camCtx) return;
    if (camCtx->map) {
        aiqMap_deinit(camCtx->map);
        camCtx->map = NULL;
    }
    if (camCtx->pool) {
        aiqPool_deinit(camCtx->pool);
        camCtx->pool = NULL;
    }
    g3aCamCtxReset(camCtx);
}

/**
 * @brief Copy block data to destination based on block type
 * @param dst Destination AiqGlobal3AInfo_t
 * @param block Source block with header
 */
static void copyBlockData(AiqGlobal3AInfoCamCtx_t* camCtx, AiqGlobal3AInfo_t* dst,
                          const AiqGlobal3aBlockHeader_t* block) {
    const AiqGlobal3aBlockConfig_t* cfg = (const AiqGlobal3aBlockConfig_t*)block;

    switch (block->type) {
        case AIQ_G3A_BLOCK_TYPE_AWB_STATS:
            if (cfg->awb_stats.config) dst->awb_stats = *cfg->awb_stats.config;
            dst->validMask |= AIQ_G3A_VALID_AWB_STATS;
            break;
        case AIQ_G3A_BLOCK_TYPE_AWB_RESULT:
            if (cfg->awb_result.config && camCtx) {
                RkAiqAlgoProcResAwb* src = cfg->awb_result.config;
                /* Assign to latest cache, field by field */
                camCtx->latest_cache.awb_result.frame_id       = dst->frame_id;
                camCtx->latest_cache.awb_result.awbConverged   = src->awbConverged;
                camCtx->latest_cache.awb_result.cctGloabl.CCT  = src->cctGloabl.CCT;
                camCtx->latest_cache.awb_result.cctGloabl.CCRI = src->cctGloabl.CCRI;
                memcpy(&camCtx->latest_cache.awb_result.awb_gain_algo, src->awb_gain_algo,
                        sizeof(rk_aiq_wb_gain_t));
                camCtx->latest_cache.awb_result.fLVValue        = src->LVValue;
                memcpy(camCtx->latest_cache.awb_result.fRGBLv,src->fRGBLv,
                    sizeof(camCtx->latest_cache.awb_result.fRGBLv));
                camCtx->latest_cache.awb_result.stat_res = src->stat_res;
                camCtx->latest_cache.awb_result.awb_gain_befmulcamfuse = src->awb_gain_befmulcamfuse;
                /* awb_result will be copied from latest_cache when get */
            }
            dst->validMask |= AIQ_G3A_VALID_AWB_RESULT;
            break;
        default:
            break;
    }
}

static void pushInfoLocked(uint32_t camId, uint32_t frame_id,
                           const AiqGlobal3aBlockHeader_t* block) {
    AiqPoolItem_t* poolItem = NULL;
    AiqGlobal3AInfo_t* dst  = NULL;
    int mapSize;
    AiqGlobal3AInfoCamCtx_t* camCtx = NULL;

    if (!block) return;
    if (camId >= RK_AIQ_CAM_GROUP_MAX_CAMS) return;
    if (block->type >= AIQ_G3A_BLOCK_TYPE_MAX) return;

    camCtx = &g_3aMgr.camCtx[camId];
    if (!camCtx->pool || !camCtx->map) return;

    /* Check if frame_id already exists in map */
    AiqMapItem_t* pExistItem = aiqMap_get(camCtx->map, (void*)(intptr_t)frame_id);
    if (pExistItem) {
        /* Update existing item with new data type */
        poolItem = *(AiqPoolItem_t**)pExistItem->_pData;
        dst      = (AiqGlobal3AInfo_t*)poolItem->_pData;

        dst->cam_id      = camId;
        copyBlockData(camCtx, dst, block);
        LOGD_ANALYZER("g3A: cam:%u reuse fid:%u type:%s mask:0x%x, map:%d, pool free:%d", camId,
                      frame_id, g3aBlockType2Str(block->type), dst->validMask,
                      aiqMap_size(camCtx->map), aiqPool_freeNums(camCtx->pool));
        return;
    }

    /* New frame_id, check if map is full */
    mapSize = aiqMap_size(camCtx->map);
    if (mapSize >= AIQ_MASTER_3A_INFO_EVICT_THRESHOLD) {
        AiqMapItem_t* pOldMapItem = aiqMap_begin(camCtx->map);
        if (pOldMapItem) {
            AiqPoolItem_t* pOldPoolItem = *(AiqPoolItem_t**)pOldMapItem->_pData;
            AiqGlobal3AInfo_t* oldInfo  = (AiqGlobal3AInfo_t*)pOldPoolItem->_pData;
            uint32_t evictFid           = oldInfo->frame_id;
            aiqMap_erase(camCtx->map, pOldMapItem->_key);
            AIQ_REF_BASE_UNREF(&oldInfo->_ref_base);
            LOGD_ANALYZER("g3A: cam:%u evict fid:%u, map:%d, pool free:%d", camId, evictFid,
                          aiqMap_size(camCtx->map), aiqPool_freeNums(camCtx->pool));
        }
    }

    poolItem = aiqPool_getFree(camCtx->pool);
    if (!poolItem) {
        LOGE_ANALYZER("g3A: pool empty, check unref!");
        camCtx->noFreeBufCnt++;
        return;
    }

    dst              = (AiqGlobal3AInfo_t*)poolItem->_pData;
    dst->cam_id      = camId;
    dst->frame_id    = frame_id;
    dst->validMask   = 0;

    /* Set data according to block type */
    copyBlockData(camCtx, dst, block);

    if (!aiqMap_insert(camCtx->map, (void*)(intptr_t)frame_id, &poolItem)) {
        LOGE_ANALYZER("g3A: map insert failed!");
        AIQ_REF_BASE_UNREF(&dst->_ref_base);
        camCtx->noFreeBufCnt++;
        return;
    }

    LOGD_ANALYZER("g3A: cam:%u push fid:%u type:%s mask:0x%x, map:%d, pool free:%d", camId,
                  frame_id, g3aBlockType2Str(block->type), dst->validMask, aiqMap_size(camCtx->map),
                  aiqPool_freeNums(camCtx->pool));
}

void aiqGlobal3AInfoMgr_init(void) {
    AiqPoolConfig_t poolCfg;
    AiqMapConfig_t mapCfg;
    AiqPoolItem_t* pItem = NULL;
    uint32_t camId       = 0;

    if (g_3aMgr.inited) {
        g_3aMgr.refCnt++;
        LOGI_ANALYZER("g3A: ref++ -> %d", g_3aMgr.refCnt);
        return;
    }

    aiqMutex_init(&g_3aMgr.mutex);

    for (camId = 0; camId < RK_AIQ_CAM_GROUP_MAX_CAMS; ++camId) {
        char poolName[32];
        char mapName[32];
        AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];

        /* Init pool for memory and ref counting */
        snprintf(poolName, sizeof(poolName), "g3A_pool_%u", camId);
        poolCfg._name      = poolName;
        poolCfg._item_size = sizeof(AiqGlobal3AInfo_t);
        poolCfg._item_nums = AIQ_MASTER_3A_INFO_CACHE_SIZE;
        camCtx->pool       = aiqPool_init(&poolCfg);
        if (!camCtx->pool) {
            LOGE_ANALYZER("g3A: cam:%u pool init failed!", camId);
            goto fail;
        }

        AIQ_POOL_FOREACH(camCtx->pool, pItem) {
            AiqGlobal3AInfo_t* info = (AiqGlobal3AInfo_t*)pItem->_pData;
            AIQ_REF_BASE_INIT(&info->_ref_base, pItem, aiqPoolItem_ref, aiqPoolItem_unref);
        }

        /* Init map for frame_id lookup, value is AiqPoolItem_t* */
        snprintf(mapName, sizeof(mapName), "g3A_map_%u", camId);
        mapCfg._name      = mapName;
        mapCfg._key_type  = AIQ_MAP_KEY_TYPE_UINT32;
        mapCfg._item_size = sizeof(AiqPoolItem_t*);
        mapCfg._item_nums = AIQ_MASTER_3A_INFO_CACHE_SIZE;
        camCtx->map       = aiqMap_init(&mapCfg);
        if (!camCtx->map) {
            LOGE_ANALYZER("g3A: cam:%u map init failed!", camId);
            goto fail;
        }

        g3aCamCtxReset(camCtx);
    }

    g_3aMgr.inited = true;
    g_3aMgr.refCnt = 1;
    LOGI_ANALYZER("g3A: init done, size:%d, cams:%d", AIQ_MASTER_3A_INFO_CACHE_SIZE,
                  RK_AIQ_CAM_GROUP_MAX_CAMS);
    return;

fail:
    for (uint32_t i = 0; i < RK_AIQ_CAM_GROUP_MAX_CAMS; ++i) {
        g3aCamCtxDeinit(&g_3aMgr.camCtx[i]);
    }
    aiqMutex_deInit(&g_3aMgr.mutex);
}

void aiqGlobal3AInfoMgr_deinit(void) {
    if (!g_3aMgr.inited) {
        LOGW_ANALYZER("g3A: not inited");
        return;
    }

    aiqMutex_lock(&g_3aMgr.mutex);

    if (--g_3aMgr.refCnt > 0) {
        LOGI_ANALYZER("g3A: ref-- -> %d", g_3aMgr.refCnt);
        aiqMutex_unlock(&g_3aMgr.mutex);
        return;
    }

    for (uint32_t camId = 0; camId < RK_AIQ_CAM_GROUP_MAX_CAMS; ++camId) {
        AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];
        if (camCtx->map) aiqMap_reset(camCtx->map);
        g3aCamCtxDeinit(camCtx);
    }

    g_3aMgr.masterCamId  = UINT32_MAX;
    g_3aMgr.syncMask     = AIQ_MASTER_SYNC_DEFAULT_MASK;
    g_3aMgr.requiredMask = 0;
    g_3aMgr.inited       = false;

    aiqMutex_unlock(&g_3aMgr.mutex);
    aiqMutex_deInit(&g_3aMgr.mutex);
    LOGI_ANALYZER("g3A: deinit done");
}

void aiqGlobal3AInfoMgr_push(uint32_t camId, uint32_t frame_id,
                             const AiqGlobal3aBlockHeader_t* block) {
    if (!block) return;
    if (block->type >= AIQ_G3A_BLOCK_TYPE_MAX) return;
    if (!g3aCamIdValid(camId)) return;

    aiqMutex_lock(&g_3aMgr.mutex);
    AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];
    camCtx->currentCamId            = camId;
    pushInfoLocked(camId, frame_id, block);
    aiqMutex_unlock(&g_3aMgr.mutex);
}

void aiqGlobal3AInfoMgr_pushAwbStats(uint32_t camId, uint32_t frame_id,
                                     const aiq_awb_stats_wrapper_t* wrap) {
    if (!wrap) return;

    AiqGlobal3aBlockConfig_t block;
    block.header.type      = AIQ_G3A_BLOCK_TYPE_AWB_STATS;
    block.header.flags     = 0;
    block.header.size      = sizeof(block);
    block.awb_stats.config = (aiq_awb_stats_wrapper_t*)wrap;

    // Adjust frame_id for effect delay
    frame_id += ISP_PARAMS_EFFECT_DELAY_CNT;

    LOGD_ANALYZER("g3A: pushAwbStats camId:%u fid:%u", camId, frame_id);

    aiqGlobal3AInfoMgr_push(camId, frame_id, &block.header);
}

void aiqGlobal3AInfoMgr_pushAwbResult(uint32_t camId, uint32_t frame_id,
                                      const RkAiqAlgoProcResAwb* awbRes) {
    if (!awbRes) return;

    AiqGlobal3aBlockConfig_t block;
    block.header.type       = AIQ_G3A_BLOCK_TYPE_AWB_RESULT;
    block.header.flags      = 0;
    block.header.size       = sizeof(block);
    block.awb_result.config = (RkAiqAlgoProcResAwb*)awbRes;

    if (awbRes->awb_gain_algo) {
        LOGD_ANALYZER("g3A: pushAwbResult camId:%u fid:%u converged:%d gain:[%.3f,%.3f,%.3f,%.3f]",
                      camId, frame_id, awbRes->awbConverged, awbRes->awb_gain_algo->rgain,
                      awbRes->awb_gain_algo->grgain, awbRes->awb_gain_algo->gbgain,
                      awbRes->awb_gain_algo->bgain);
    } else {
        LOGD_ANALYZER("g3A: pushAwbResult camId:%u fid:%u converged:%d gain:null", camId, frame_id,
                      awbRes->awbConverged);
    }

    aiqGlobal3AInfoMgr_push(camId, frame_id, &block.header);
}

XCamReturn aiqGlobal3AInfo_get(uint32_t camId, AiqGlobal3AInfo_t** out) {
    if (!out) return XCAM_RETURN_ERROR_PARAM;

    *out = NULL;
    if (!g3aCamIdValid(camId)) return XCAM_RETURN_ERROR_PARAM;

    aiqMutex_lock(&g_3aMgr.mutex);
    AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];
    camCtx->currentCamId            = camId;

    /* Check if camId is in sync mask */
    if (!(g_3aMgr.syncMask & (1ULL << camId))) {
        aiqMutex_unlock(&g_3aMgr.mutex);
        LOGD_ANALYZER("g3A: camId:%u not in syncMask:0x%llx", camId,
                      (unsigned long long)g_3aMgr.syncMask);
        return XCAM_RETURN_ERROR_FAILED;
    }

    XCamReturn ret = g3aGetLatestInfoLocked(camId, out);
    aiqMutex_unlock(&g_3aMgr.mutex);
    return ret;
}

XCamReturn aiqGlobal3AInfo_getOthers(uint32_t camId, AiqGlobal3AInfo_t** out_infos,
                                     uint32_t max_num, uint32_t* out_num) {
    if (!out_infos || !out_num || max_num == 0) return XCAM_RETURN_ERROR_PARAM;
    if (!g3aCamIdValid(camId)) return XCAM_RETURN_ERROR_PARAM;

    *out_num = 0;

    LOGD_ANALYZER("g3A: getOthers for camId:%u", camId);

    aiqMutex_lock(&g_3aMgr.mutex);
    AiqGlobal3AInfoCamCtx_t* camCtx = &g_3aMgr.camCtx[camId];
    camCtx->currentCamId            = camId;

    if (!(g_3aMgr.syncMask & (1ULL << camId))) {
        aiqMutex_unlock(&g_3aMgr.mutex);
        LOGD_ANALYZER("g3A: camId:%u not in syncMask:0x%llx", camId,
                      (unsigned long long)g_3aMgr.syncMask);
        return XCAM_RETURN_ERROR_FAILED;
    }

    for (uint32_t id = 0; id < RK_AIQ_CAM_GROUP_MAX_CAMS && *out_num < max_num; ++id) {
        if (id == camId) continue;
        if (!(g_3aMgr.syncMask & (1ULL << id))) continue;

        AiqGlobal3AInfo_t* info = NULL;
        if (g3aGetLatestInfoLocked(id, &info) == XCAM_RETURN_NO_ERROR) {
            out_infos[*out_num] = info;
            (*out_num)++;
            LOGD_ANALYZER("g3A: camId:%u added to out_infos, total:%u", id, *out_num);
        }
    }

    aiqMutex_unlock(&g_3aMgr.mutex);

    return *out_num ? XCAM_RETURN_NO_ERROR : XCAM_RETURN_ERROR_FAILED;
}

void aiqGlobal3AInfo_ref(AiqGlobal3AInfo_t* info) {
    if (!info) return;

    aiqMutex_lock(&g_3aMgr.mutex);
    AIQ_REF_BASE_REF(&info->_ref_base);
    aiqMutex_unlock(&g_3aMgr.mutex);
}

void aiqGlobal3AInfo_unref(AiqGlobal3AInfo_t* info) {
    if (!info) return;

    aiqMutex_lock(&g_3aMgr.mutex);
    uint32_t camId                  = info->cam_id;
    AiqGlobal3AInfoCamCtx_t* camCtx = g3aCamIdValid(camId) ? &g_3aMgr.camCtx[camId] : NULL;
    int prevFree                    = camCtx && camCtx->pool ? aiqPool_freeNums(camCtx->pool) : 0;
    uint32_t fid = info->frame_id;
    AIQ_REF_BASE_UNREF(&info->_ref_base);

    /* If pool free increased, item was released, remove from map */
    if (camCtx && camCtx->pool && camCtx->map && aiqPool_freeNums(camCtx->pool) > prevFree) {
        aiqMap_erase(camCtx->map, (void*)(intptr_t)fid);
        LOGD_ANALYZER("g3A: cam:%u unref released fid:%u, map:%d, pool free:%d", camId, fid,
                      aiqMap_size(camCtx->map), aiqPool_freeNums(camCtx->pool));
    }
    aiqMutex_unlock(&g_3aMgr.mutex);
}

XCamReturn aiqGlobal3AInfoMgr_setMasterCamSync(uint32_t master_cam_id, uint64_t sync_cam_mask) {
    if (master_cam_id >= RK_AIQ_CAM_GROUP_MAX_CAMS) return XCAM_RETURN_ERROR_PARAM;

    aiqMutex_lock(&g_3aMgr.mutex);
    g_3aMgr.masterCamId = master_cam_id;
    if (g3aCamIdValid(master_cam_id)) g_3aMgr.camCtx[master_cam_id].currentCamId = master_cam_id;
    g_3aMgr.syncMask = sync_cam_mask;
    aiqMutex_unlock(&g_3aMgr.mutex);

    LOGI_ANALYZER("g3A: set master cam id:%u sync mask:0x%llx", master_cam_id, sync_cam_mask);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn aiqGlobal3AInfoMgr_setMasterCamSyncList(uint32_t master_cam_id,
                                                   const uint32_t* sync_cam_ids,
                                                   uint32_t sync_cam_num) {
    uint64_t mask = 0;

    if (master_cam_id >= RK_AIQ_CAM_GROUP_MAX_CAMS) return XCAM_RETURN_ERROR_PARAM;
    if (sync_cam_num > 0 && !sync_cam_ids) return XCAM_RETURN_ERROR_PARAM;

    for (uint32_t i = 0; i < sync_cam_num; ++i) {
        if (sync_cam_ids[i] >= RK_AIQ_CAM_GROUP_MAX_CAMS) return XCAM_RETURN_ERROR_PARAM;
        mask |= (1ULL << sync_cam_ids[i]);
    }

    return aiqGlobal3AInfoMgr_setMasterCamSync(master_cam_id, mask);
}

void aiqGlobal3AInfoMgr_setRequiredMask(uint32_t required_mask) {
    aiqMutex_lock(&g_3aMgr.mutex);
    g_3aMgr.requiredMask = required_mask;
    aiqMutex_unlock(&g_3aMgr.mutex);

    LOGI_ANALYZER("g3A: set required mask:0x%x", required_mask);
}

uint32_t aiqGlobal3AInfoMgr_getRequiredMask(void) { return g_3aMgr.requiredMask; }

void aiqGlobal3AInfoMgr_getPoolAndMap(uint32_t camId, AiqPool_t** pool, AiqMap_t** map) {
    AiqGlobal3AInfoCamCtx_t* camCtx = g3aCamIdValid(camId) ? &g_3aMgr.camCtx[camId] : NULL;

    if (pool) *pool = camCtx ? camCtx->pool : NULL;
    if (map) *map = camCtx ? camCtx->map : NULL;
}

uint32_t aiqGlobal3AInfoMgr_getNoFreeBufCnt(uint32_t camId) {
    AiqGlobal3AInfoCamCtx_t* camCtx = g3aCamIdValid(camId) ? &g_3aMgr.camCtx[camId] : NULL;

    return camCtx ? camCtx->noFreeBufCnt : 0;
}

uint32_t aiqGlobal3AInfoMgr_getMasterCamId(void) {
    uint32_t id;
    aiqMutex_lock(&g_3aMgr.mutex);
    id = g_3aMgr.masterCamId;
    aiqMutex_unlock(&g_3aMgr.mutex);
    return id;
}
