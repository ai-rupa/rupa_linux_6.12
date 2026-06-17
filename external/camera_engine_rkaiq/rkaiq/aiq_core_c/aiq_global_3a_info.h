/*
 * Copyright (c) 2024 Rockchip Eletronics Co., Ltd.
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

#ifndef _AIQ_GLOBAL_3A_INFO_H_
#define _AIQ_GLOBAL_3A_INFO_H_

#include "c_base/aiq_map.h"
#include "c_base/aiq_mutex.h"
#include "c_base/aiq_pool.h"
#include "common/rk_aiq_types_priv_c.h"

XCAM_BEGIN_DECLARE

/**
 * @brief Initialize Global 3A Info Manager
 * @note Thread-safe, supports multiple calls (reference counted)
 */
void aiqGlobal3AInfoMgr_init(void);

/**
 * @brief Deinitialize Global 3A Info Manager
 * @note Thread-safe, only deinits when ref count drops to 0
 */
void aiqGlobal3AInfoMgr_deinit(void);

/**
 * @brief Set master camera and sync mask
 * @param master_cam_id Master camera ID
 * @param sync_cam_mask Bitmask of cameras to sync with
 * @return XCAM_RETURN_NO_ERROR on success
 */
XCamReturn aiqGlobal3AInfoMgr_setMasterCamSync(uint32_t master_cam_id, uint64_t sync_cam_mask);

/**
 * @brief Set master camera sync with camera ID list
 * @param master_cam_id Master camera ID
 * @param sync_cam_ids Array of camera IDs to sync with
 * @param sync_cam_num Number of cameras in sync_cam_ids
 * @return XCAM_RETURN_NO_ERROR on success
 */
XCamReturn aiqGlobal3AInfoMgr_setMasterCamSyncList(uint32_t master_cam_id,
                                                   const uint32_t* sync_cam_ids,
                                                   uint32_t sync_cam_num);

/**
 * @brief Push 3A info block from master camera (generic interface)
 * @param camId Camera ID
 * @param frame_id Frame ID
 * @param block Pointer to the 3a block (must have valid header with type and size)
 * @note Info is stored per camera ID.
 *       The block->header.type determines which data type to update.
 *       If frame_id exists, update the corresponding data type.
 *       If frame_id not exists, create new entry and insert data.
 *
 * Example usage:
 * @code
 *      AiqGlobal3aBlockAwbStats_t block;
 *      block.header.type = AIQ_G3a_BLOCK_TYPE_AWB_STATS;
 *      block.header.flags = 0;
 *      block.header.size = sizeof(block);
 *      block.config = awb_stats;
 *      aiqGlobal3AInfoMgr_push(camId, frame_id, &block.header);
 * @endcode
 */
void aiqGlobal3AInfoMgr_push(uint32_t camId, uint32_t frame_id,
                             const AiqGlobal3aBlockHeader_t* block);

/**
 * @brief Push AWB stats info from master camera (backward compatible)
 * @param camId Camera ID
 * @param frame_id Frame ID
 * @param wrap AWB stats wrapper
 * @note Info is stored per camera ID
 * @deprecated Use aiqGlobal3AInfoMgr_push() instead
 */
void aiqGlobal3AInfoMgr_pushAwbStats(uint32_t camId, uint32_t frame_id,
                                     const aiq_awb_stats_wrapper_t* wrap);

/**
 * @brief Push AWB result info from master camera
 * @param camId Camera ID
 * @param frame_id Frame ID
 * @param awbRes AWB algorithm processing result
 * @note Info is stored per camera ID
 */
void aiqGlobal3AInfoMgr_pushAwbResult(uint32_t camId, uint32_t frame_id,
                                      const RkAiqAlgoProcResAwb* awbRes);

/**
 * @brief Get latest Global 3A Info
 * @param camId Camera ID to check against sync mask
 * @param out Output pointer to receive Global 3A Info
 * @return XCAM_RETURN_NO_ERROR on success, XCAM_RETURN_ERROR_FAILED if camId not in sync mask
 * @note Returns latest info for the specified camera ID.
 *       Caller must call aiqGlobal3AInfo_unref() to release
 */
XCamReturn aiqGlobal3AInfo_get(uint32_t camId, AiqGlobal3AInfo_t** out);

/**
 * @brief Get latest Global 3A Info for other cameras except camId
 * @param camId Current camera ID to exclude
 * @param out_infos Output array of Global 3A Info pointers
 * @param max_num Capacity of out_infos array
 * @param out_num Output number of cameras filled in out_infos
 * @return XCAM_RETURN_NO_ERROR on success, XCAM_RETURN_ERROR_FAILED if none found
 * @note Caller must call aiqGlobal3AInfo_unref() for each returned info
 */
XCamReturn aiqGlobal3AInfo_getOthers(uint32_t camId, AiqGlobal3AInfo_t** out_infos,
                                     uint32_t max_num, uint32_t* out_num);

/**
 * @brief Increase reference count for Global 3A Info
 * @param info Global 3A Info to reference
 */
void aiqGlobal3AInfo_ref(AiqGlobal3AInfo_t* info);

/**
 * @brief Decrease reference count for Global 3A Info
 * @param info Global 3A Info to unreference
 */
void aiqGlobal3AInfo_unref(AiqGlobal3AInfo_t* info);

/**
 * @brief Set required validMask for get to succeed
 * @param required_mask Bitmask of required valid bits (e.g. AIQ_G3A_VALID_AWB_RESULT)
 */
void aiqGlobal3AInfoMgr_setRequiredMask(uint32_t required_mask);

/**
 * @brief Get required validMask
 * @return Current required validMask
 */
uint32_t aiqGlobal3AInfoMgr_getRequiredMask(void);

/**
 * @brief Get Global 3A Info Manager internal pool and map for a specific camera
 * @param camId Camera ID to query
 * @param pool Output pool pointer (can be NULL)
 * @param map Output map pointer (can be NULL)
 */
void aiqGlobal3AInfoMgr_getPoolAndMap(uint32_t camId, AiqPool_t** pool, AiqMap_t** map);

/**
 * @brief Get count of times no free buffer was available for a specific camera
 * @param camId Camera ID to query
 * @return Count of no free buffer occurrences
 */
uint32_t aiqGlobal3AInfoMgr_getNoFreeBufCnt(uint32_t camId);

/**
 * @brief Get current master camera id
 * @return master camera id, or UINT32_MAX if unset
 */
uint32_t aiqGlobal3AInfoMgr_getMasterCamId(void);

XCAM_END_DECLARE

#endif /* _AIQ_GLOBAL_3A_INFO_H_ */
