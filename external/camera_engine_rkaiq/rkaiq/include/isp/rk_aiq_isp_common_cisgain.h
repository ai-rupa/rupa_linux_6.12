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

#ifndef _RK_AIQ_ISP_COMMON_CISGAIN_H_
#define _RK_AIQ_ISP_COMMON_CISGAIN_H_


typedef enum rk_aiq_cisGain_e {
    rk_aiq_cisGain_1x = 0,
    rk_aiq_cisGain_2x = 1,
    rk_aiq_cisGain_4x = 2,
    rk_aiq_cisGain_8x = 3,
    rk_aiq_cisGain_16x = 4,
    rk_aiq_cisGain_32x = 5,
    rk_aiq_cisGain_64x = 6,
    rk_aiq_cisGain_128x = 7,
    rk_aiq_cisGain_256x = 8,
    rk_aiq_cisGain_512x = 9,
    rk_aiq_cisGain_1024x = 10,
    rk_aiq_cisGain_2048x = 11,
    rk_aiq_cisGain_4096x = 12
} rk_aiq_cisGain_t;

#endif

