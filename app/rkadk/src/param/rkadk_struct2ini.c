/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "rkadk_struct2ini.h"
#include "rkadk_common.h"
#include "rkadk_hal.h"
#include "rkadk_type.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define RKADK_MAX_INI_DICT_NUM (RKADK_MAX_SENSOR_CNT * 2 + 1) // setting_sensor ini + rkadk_setting.ini

typedef struct {
  pthread_mutex_t mutex;
  dictionary *dict[RKADK_MAX_INI_DICT_NUM];
} RKADK_INI_CTX_S;

static RKADK_INI_CTX_S gCtx;

int RKADK_IniInit() {
  int ret;

  memset(&gCtx, 0, sizeof(RKADK_INI_CTX_S));
  ret = pthread_mutex_init(&gCtx.mutex, NULL);
  if (ret) {
    RKADK_LOGE("param mutex init failed[%d]", ret);
    return -1;
  }

  return 0;
}

void RKADK_IniDeinit() {
  for (int i = 0; i < RKADK_MAX_INI_DICT_NUM; i++) {
    if (gCtx.dict[i]) {
      iniparser_freedict(gCtx.dict[i]);
      RKADK_LOGD("iniparser_freedict[%d, %p] ok", i, gCtx.dict[i]);
      gCtx.dict[i] = NULL;
    }
  }

  pthread_mutex_destroy(&gCtx.mutex);
}

RKADK_S32 RKADK_Ini2Struct(int camId, char *iniFile, void *structAddr,
                           RKADK_SI_CONFIG_MAP_S *mapTable, int mapTableSize) {
  int searchLen = 0;
  dictionary *ini = NULL;
  bool bSaveDict = false;
  char sectionKey[SI_MAX_SEARCH_STRING] = {0};

  RKADK_CHECK_POINTER(iniFile, RKADK_FAILURE);
  RKADK_CHECK_POINTER(structAddr, RKADK_FAILURE);
  RKADK_CHECK_POINTER(mapTable, RKADK_FAILURE);

  if (mapTableSize <= 0) {
    RKADK_LOGE("invalid mapTableSize[%d]", mapTableSize);
    return RKADK_FAILURE;
  }

  pthread_mutex_lock(&gCtx.mutex);
  if (camId >= 0 && camId < (RKADK_MAX_INI_DICT_NUM - 1) && strstr(iniFile, "setting_sensor_")) {
    bSaveDict = true;

    if (!gCtx.dict[camId]) {
      gCtx.dict[camId] = iniparser_load(iniFile);
      if (gCtx.dict[camId] == NULL) {
        RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
        goto EXIT;
      }
      RKADK_LOGD("iniparser_load[%s] ok, dict[%d, %p]", iniFile, camId, gCtx.dict[camId]);
    }

    ini = gCtx.dict[camId];
  } else if (camId == -1 && strstr(iniFile, "rkadk_setting.ini")) {
    bSaveDict = true;
    int index = RKADK_MAX_INI_DICT_NUM - 1;

    if (!gCtx.dict[index]) {
      gCtx.dict[index] = iniparser_load(iniFile);
      if (gCtx.dict[index] == NULL) {
        RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
        goto EXIT;
      }
      RKADK_LOGD("iniparser_load[%s] ok, dict[%d, %p]", iniFile, index, gCtx.dict[index]);
    }

    ini = gCtx.dict[index];
  } else {
    ini = iniparser_load(iniFile);
    if (ini == NULL) {
      RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
      goto EXIT;
    }
  }

  for (int i = 0; i < mapTableSize; i++) {
    searchLen = strlen(mapTable[i].structName) + strlen(mapTable[i].structMember);
    if (searchLen >= (SI_MAX_SEARCH_STRING - 1)) {
      RKADK_LOGE("searchLen[%d] is too long", searchLen);
      goto EXIT;
    }

    memset(sectionKey, 0, sizeof(sectionKey));
    strcpy(sectionKey, mapTable[i].structName);
    strcat(sectionKey, ":");
    strcat(sectionKey, mapTable[i].structMember);

    if (mapTable[i].keyVlaueType == int_e) {
      int keyInt = iniparser_getint(ini, sectionKey, -1);
      if (keyInt == -1) {
        RKADK_LOGE("int[%s] not exist", sectionKey);
        goto EXIT;
      } else {
        *(int *)((char *)structAddr + mapTable[i].offset) = keyInt;
      }
    } else if (mapTable[i].keyVlaueType == string_e) {
      char *keyString = (char *)iniparser_getstring(ini, sectionKey, NULL);
      if (keyString) {
        memset((char *)structAddr + mapTable[i].offset, 0,
               mapTable[i].stringLength);
        if (strlen(keyString)) {
          if (strlen(keyString) <= mapTable[i].stringLength) {
            memcpy((char *)structAddr + mapTable[i].offset, keyString,
                   strlen(keyString));
          } else {
            memcpy((char *)structAddr + mapTable[i].offset, keyString,
                   mapTable[i].stringLength - 1);
          }
        }
      } else {
        RKADK_LOGE("string[%s] not exist", sectionKey);
        goto EXIT;
      }
    } else if (mapTable[i].keyVlaueType == double_e) {
      double keyDouble = iniparser_getdouble(ini, sectionKey, -1.0);
      const double EPS = 1e-6;

      if (fabs(keyDouble - (-1.0)) > EPS) {
        *(double *)((char *)structAddr + mapTable[i].offset) = keyDouble;
      } else {
        RKADK_LOGE("double[%s] not exist", sectionKey);
        goto EXIT;
      }
    } else if (mapTable[i].keyVlaueType == bool_e) {
      int keyBool = iniparser_getboolean(ini, sectionKey, -1);
      if (keyBool != -1) {
        if (keyBool)
          *(bool *)((char *)structAddr + mapTable[i].offset) = true;
        else
          *(bool *)((char *)structAddr + mapTable[i].offset) = false;
      } else {
        RKADK_LOGE("bool[%s] not exist", sectionKey);
        goto EXIT;
      }
    }
  }

  if (ini && !bSaveDict) {
    iniparser_freedict(ini);
  }

  pthread_mutex_unlock(&gCtx.mutex);
  return RKADK_SUCCESS;

EXIT:
  if (bSaveDict) {
    if (gCtx.dict[camId]) {
      iniparser_freedict(gCtx.dict[camId]);
      gCtx.dict[camId] = NULL;
    }
  } else if (ini) {
    iniparser_freedict(ini);
  }

  pthread_mutex_unlock(&gCtx.mutex);
  RKADK_LOGE("iniFile[%s] to struct failed", iniFile);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_Struct2Ini(int camId, char *iniFile, void *structAddr,
                           RKADK_SI_CONFIG_MAP_S *mapTable, int mapTableSize) {
  int ret = RKADK_FAILURE;
  FILE *fp = NULL;
  bool bSaveDict = false;
  dictionary *ini = NULL;
  char temp[SI_CONFIG_MAP_STR_LENGTH_MAX] = {0};
  char sectionKey[SI_MAX_SEARCH_STRING] = {0};

  RKADK_CHECK_POINTER(iniFile, RKADK_FAILURE);
  RKADK_CHECK_POINTER(structAddr, RKADK_FAILURE);
  RKADK_CHECK_POINTER(mapTable, RKADK_FAILURE);

  if (mapTableSize <= 0) {
    RKADK_LOGE("invalid mapTableSize[%d]", mapTableSize);
    return RKADK_FAILURE;
  }

  pthread_mutex_lock(&gCtx.mutex);
  if (camId >= 0 && camId < (RKADK_MAX_INI_DICT_NUM - 1) && strstr(iniFile, "setting_sensor_")) {
    bSaveDict = true;

    if (!gCtx.dict[camId]) {
      gCtx.dict[camId] = iniparser_load(iniFile);
      if (gCtx.dict[camId] == NULL) {
        RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
        goto EXIT;
      }
      RKADK_LOGD("iniparser_load[%s] ok, dict[%d, %p]", iniFile, camId, gCtx.dict[camId]);
    }

    ini = gCtx.dict[camId];
  } else if (camId == -1 && strstr(iniFile, "rkadk_setting.ini")) {
    bSaveDict = true;
    int index = RKADK_MAX_INI_DICT_NUM - 1;

    if (!gCtx.dict[index]) {
      gCtx.dict[index] = iniparser_load(iniFile);
      if (gCtx.dict[index] == NULL) {
        RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
        goto EXIT;
      }
      RKADK_LOGD("iniparser_load[%s] ok, dict[%d, %p]", iniFile, index, gCtx.dict[index]);
    }

    ini = gCtx.dict[index];
  } else {
    ini = iniparser_load(iniFile);
    if (ini == NULL) {
      RKADK_LOGE("iniparser_load file[%s] failed", iniFile);
      goto EXIT;
    }
  }

  if (iniparser_find_entry(ini, mapTable[0].structName) == 0) {
    RKADK_LOGW("section name[%s] no exist, so create", mapTable[0].structName);
    iniparser_set(ini, mapTable[0].structName, NULL);
  }

  for (int i = 0; i < mapTableSize; i++) {
    memset(sectionKey, 0, sizeof(sectionKey));
    memset(temp, 0, sizeof(temp));
    strcpy(sectionKey, mapTable[i].structName);
    strcat(sectionKey, ":");
    strcat(sectionKey, mapTable[i].structMember);

    if (mapTable[i].keyVlaueType == int_e) {
      sprintf(temp, "%d", *(int *)((char *)structAddr + mapTable[i].offset));
    } else if (mapTable[i].keyVlaueType == string_e) {
      if (mapTable[i].stringLength >= (SI_CONFIG_MAP_STR_LENGTH_MAX - 1))
        RKADK_LOGE("mapTable[%d].string_length > SI_CONFIG_MAP_STR_LENGTH_MAX(%d)", i, SI_CONFIG_MAP_STR_LENGTH_MAX);
      else
        sprintf(temp, "%s", (char *)structAddr + mapTable[i].offset);
    } else if (mapTable[i].keyVlaueType == double_e) {
      sprintf(temp, "%.7f", *(double *)((char *)structAddr + mapTable[i].offset));
    } else if (mapTable[i].keyVlaueType == bool_e) {
      if (*((bool *)structAddr + mapTable[i].offset))
        sprintf(temp, "%s", "TRUE");
      else
        sprintf(temp, "%s", "FALSE");
    } else {
      continue;
    }

    iniparser_set(ini, sectionKey, temp);
  }

  fp = fopen(iniFile, "w");
  if (!fp) {
    RKADK_LOGE("fopen %s failed", iniFile);
    goto EXIT;
  }

  iniparser_dump_ini(ini, fp);
  fflush(fp);
  fclose(fp);
  ret = RKADK_SUCCESS;

EXIT:
  if (ini && !bSaveDict) {
    iniparser_freedict(ini);
  }

  if (ret != RKADK_SUCCESS)
    RKADK_LOGE("struct to iniFile[%s] failed", iniFile);

  pthread_mutex_unlock(&gCtx.mutex);
  return ret;
}

int RKADK_IniGetInt(int camId, const char *entry, int default_val) {
  int ret;
  int index = camId;

  if (index < 0)
    index = RKADK_MAX_INI_DICT_NUM - 1;

  if (!gCtx.dict[index]) {
    RKADK_LOGE("camId[%d] entry[%s] dict is NULL", index, entry);
    return -1;
  }

  pthread_mutex_lock(&gCtx.mutex);
  ret = iniparser_getint(gCtx.dict[index], entry, default_val);
  pthread_mutex_unlock(&gCtx.mutex);

  return ret;
}

bool RKADK_IniGetBool(int camId, const char *entry, int default_val) {
  int ret;
  int index = camId;

  if (index < 0)
    index = RKADK_MAX_INI_DICT_NUM - 1;

  if (!gCtx.dict[index]) {
    RKADK_LOGE("camId[%d] entry[%s] dict is NULL", index, entry);
    return true;
  }

  if (default_val != 0 && default_val != 1) {
    RKADK_LOGE("Bool invaild default val[%d]", default_val);
    return true;
  }

  pthread_mutex_lock(&gCtx.mutex);
  ret = iniparser_getboolean(gCtx.dict[index], entry, default_val);
  pthread_mutex_unlock(&gCtx.mutex);

  if (ret)
    return true;
  else
    return false;
}

int RKADK_IniGetDouble(int camId, const char *entry, double default_val) {
  int ret;
  int index = camId;

  if (index < 0)
    index = RKADK_MAX_INI_DICT_NUM - 1;

  if (!gCtx.dict[index]) {
    RKADK_LOGE("camId[%d] entry[%s] dict is NULL", index, entry);
    return -1.0;
  }

  pthread_mutex_lock(&gCtx.mutex);
  ret = iniparser_getdouble(gCtx.dict[index], entry, default_val);
  pthread_mutex_unlock(&gCtx.mutex);

  return ret;
}

const char *RKADK_IniGetString(int camId, const char *entry, const char *default_val) {
  const char *ret = NULL;
  int index = camId;

  if (index < 0)
    index = RKADK_MAX_INI_DICT_NUM - 1;

  if (!gCtx.dict[index]) {
    RKADK_LOGE("camId[%d] entry[%s] dict is NULL", index, entry);
    return NULL;
  }

	pthread_mutex_lock(&gCtx.mutex);
	ret = iniparser_getstring(gCtx.dict[index], entry, default_val);
	pthread_mutex_unlock(&gCtx.mutex);

	return ret;
}
