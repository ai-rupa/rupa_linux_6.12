/*
 * Copyright 2022 Rockchip Electronics Co. LTD
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

#include "uac_amixer.h"
#include "rk_mpi_amix.h"
#include "uac_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "amixer"
#endif

RK_S32 UACAmixer::set(AUDIO_DEV AmixDevId, const char *ctrlName,
                      const char *value) {
  ALOGD("AmixDevId = %d, ctrl = %s, value = %s\n", AmixDevId, ctrlName, value);
  return RK_MPI_AMIX_SetControl(AmixDevId, ctrlName, (char *)value);
}

RK_S32 UACAmixer::get(AUDIO_DEV AmixDevId, const char *ctrlName) {
  return RK_MPI_AMIX_GetControl(AmixDevId, ctrlName, NULL);
}

void UACAmixer::dumpContents(AUDIO_DEV AmixDevId) {
  RK_MPI_AMIX_ListContents(AmixDevId);
}

RK_S32 amix_set_ref_mode(AUDIO_DEV AoDevId) {
  RK_S32 result = RK_SUCCESS;
#if defined(RV1106)

	/*
	 * This is the RV1106 feedback setting, suitable for the left mic, right feedback
	 * RV1126 setting is invalid, can be not set,
	 * RV1126 needs to configure asound.conf file or kernel driver configuration software feedback.
	 */
	result =
	    UACAmixer::set(AoDevId, "I2STDM Digital Loopback Mode", (char *)"Mode2");
	if (result != RK_SUCCESS) {
		ALOGE("ai set I2STDM Digital Loopback Mode fail, reason = %x\n", result);
    return RK_FAILURE;
	}

#elif defined(RV1103B)
	result = UACAmixer::set(AoDevId, "SAI SDI0 Loopback Src Select",
	                                (char *)"From SDO0");
	if (result != RK_SUCCESS) {
		ALOGE("ai set SAI SDI0 Loopback Src Select, reason = %x\n", result);
    return RK_FAILURE;
	}

	result = UACAmixer::set(AoDevId, "SAI SDI0 Loopback I2S LR Switch",
	                                (char *)"Enable");
	if (result != RK_SUCCESS) {
		ALOGE("ai set SAI SAI SDI0 Loopback I2S LR Switch, reason = %x\n", result);
		return RK_FAILURE;
	}

	result = UACAmixer::set(AoDevId, "SAI SDI0 Loopback Switch",
	                               (char *)"Enable");
	if (result != RK_SUCCESS) {
		ALOGE("ai set SAI SDI0 Loopback Switch, reason = %x\n", result);
    return RK_FAILURE;
	}
#elif defined(RV1126B)
	result = UACAmixer::set(AoDevId, "SAI2 Receive PATH0 Source Select", (char *)"From SDI1");
	if (result != RK_SUCCESS) {
		ALOGE("ai set SAI2 Receive PATH0 Source Select, reason = %x\n", result);
		return RK_FAILURE;
	}
	result = UACAmixer::set(AoDevId, "SAI2 Receive PATH1 Source Select", (char *)"From SDI0");
	if (result != RK_SUCCESS) {
		ALOGE("ai set SAI2 Receive PATH1 Source Select, reason = %x\n", result);
		return RK_FAILURE;
	}
#endif
  return result;
}

RK_S32 amix_set_ppm(AUDIO_DEV AoDevId, RK_S32 ppm) {
  char str[64];
  snprintf(str, sizeof(str), "%d", ppm);

#if defined(RV1103B)
	const static char *sPpmCtrlName = "SAI PCM Clk Compensation In PPM";
#elif defined(RV1126B)
	const static char *sPpmCtrlName = "SAI2 PCM Clk Compensation In PPM";
#else
	const static char *sPpmCtrlName = "PCM Clk Compensation In PPM";
#endif

  return UACAmixer::set(AoDevId, sPpmCtrlName, str);
}
