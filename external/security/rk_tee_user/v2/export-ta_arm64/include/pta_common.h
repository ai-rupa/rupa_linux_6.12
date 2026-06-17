/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 */
#ifndef __PTA_COMMON_H
#define __PTA_COMMON_H

#include <util.h>

/*
 * Interface to the pseudo TA, check the caller is from uboot.
 */
TEE_Result rk_check_call_by_uboot(void);

#endif /* __PTA_COMMON_H */
