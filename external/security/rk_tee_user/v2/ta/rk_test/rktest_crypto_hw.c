// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2023 Rockchip Electronics Co. Ltd.
 */
#include <tee_internal_api.h>
#include "rktest_handle.h"
#include "../rk_public_api/rk_crypto_api.h"

TEE_Result handle_crypto_hw(uint32_t param_types, TEE_Param params[4])
{
    uint32_t res;
    TEE_Time t1;
    TEE_Time t2;
    rk_cipher_config config;
    uint32_t algo = RK_ALGO_AES;
    uint32_t mode = RK_CIPHER_MODE_CBC;
    uint8_t key[16] = {
        0x1a, 0x0c, 0x67, 0xa4, 0xff, 0x33, 0x07, 0x36,
        0xf6, 0x48, 0x08, 0xc8, 0x22, 0x00, 0xb2, 0xc0,
    };
    uint32_t key_len = sizeof(key);
    uint8_t iv[16] = {
        0x10, 0x44, 0x80, 0xb3, 0x88, 0x5f, 0x02, 0x03,
        0x05, 0x21, 0x07, 0xc9, 0x44, 0x00, 0x1b, 0x80,
    };

    ASSERT_PARAM_TYPES(TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
		       TEE_PARAM_TYPE_NONE,
		       TEE_PARAM_TYPE_NONE,
		       TEE_PARAM_TYPE_NONE));

    config.algo = algo;
    config.mode = mode;
    config.operation = RK_MODE_ENCRYPT;
    config.key_len = key_len;
    config.reserved = NULL;
    TEE_MemMove(config.iv, iv, sizeof(iv));
    TEE_MemMove(config.key, key, sizeof(key));

    TEE_GetSystemTime(&t1);
    res = rk_user_ta_cipher(&config, params[0].memref.buffer, params[0].memref.size);
    if (res)
        EMSG("test rk_user_ta_cipher fail! 0x%08x", res);
    TEE_GetSystemTime(&t2);
    IMSG("rk_user_ta_cipher encrypt use time: %d", get_ms_diff(t1, t2));

    config.algo = algo;
    config.mode = mode;
    config.operation = RK_MODE_DECRYPT;
    config.key_len = key_len;
    config.reserved = NULL;
    TEE_MemMove(config.iv, iv, sizeof(iv));
    TEE_MemMove(config.key, key, sizeof(key));

    TEE_GetSystemTime(&t1);
    res = rk_user_ta_cipher(&config, params[0].memref.buffer, params[0].memref.size);
    if (res)
        EMSG("test rk_user_ta_cipher fail! 0x%08x", res);
    TEE_GetSystemTime(&t2);
    IMSG("rk_user_ta_cipher decrypt use time: %d", get_ms_diff(t1, t2));

    return res;
}
