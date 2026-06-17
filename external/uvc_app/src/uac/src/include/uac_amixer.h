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

#ifndef SRC_MPI_MPI_UAC_AMIXER_H_
#define SRC_MPI_MPI_UAC_AMIXER_H_

#include "rk_common.h"

class UACAmixer {
public:
  static RK_S32 set(AUDIO_DEV AmixDevId, const char *ctrlName,
                    const char *value);
  static RK_S32 get(AUDIO_DEV AmixDevId, const char *ctrlName);
  static void dumpContents(AUDIO_DEV AmixDevId);
};

RK_S32 amix_set_ref_mode(AUDIO_DEV AoDevId);
RK_S32 amix_set_ppm(AUDIO_DEV AoDevId, RK_S32 ppm);

#endif // SRC_MPI_MPI_UAC_AMIXER_H_
