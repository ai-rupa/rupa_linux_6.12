/*
 *  Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *  Author: Bin Yang <yangbin@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

#define CHIPNAME_CONFIG_FILE "/data/rk-chipname.conf"

static const chip_info_t chip_mappings[] = {
#define CHIP_ENTRY(id, name) {CHIP_##id, name},
    CHIP_LIST
#undef CHIP_ENTRY
    {CHIP_UNKNOWN, "unknown"}
};

chip_id_t search_chip_name_to_id(const char* name)
{
    if (!name) return CHIP_UNKNOWN;
    for (int i = 0; chip_mappings[i].name != NULL; i++) {
        if (strcmp(name, chip_mappings[i].name) == 0) {
            return chip_mappings[i].id;
        }
    }
    return CHIP_UNKNOWN;
}

const char* search_chip_id_to_name(chip_id_t id)
{
    for (int i = 0; chip_mappings[i].name != NULL; i++) {
        if (chip_mappings[i].id == id) {
            return chip_mappings[i].name;
        }
    }
    return "unknown";
}

chip_id_t get_chipid(void)
{
    char env_chipname[PCBA_MAX_CHIP_NAME] = {0};
    if (get_chipname_from_file(env_chipname, PCBA_MAX_CHIP_NAME) < 0) {
        fprintf(stderr, "Error: RK_CHIPNAME environment variable not set!\n");
        return CHIP_UNKNOWN;
    }
    return search_chip_name_to_id(env_chipname);
}

int get_chipname_from_file(char* chipname, int len)
{
    FILE* file = fopen(CHIPNAME_CONFIG_FILE, "r");
    if (!file) {
        return -1;
    }
    char line[256];
    char* value = NULL;
    int found = 0;
    int value_len = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "RK_CHIPNAME=", 12) == 0) {
            value = line + 12;
            while (*value == ' ') value++;
            value_len = strlen(value);
            found = 1;
            break;
        }
    }
    fclose(file);
    if (!found || value_len == 0) {
        return -1;
    }
    if (chipname != NULL) {
        if (len > 0) {
            int copy_len = (value_len < len - 1) ? value_len : len - 1;
            strncpy(chipname, value, copy_len);
            chipname[copy_len] = '\0';
        } else {
            return -1;
        }
    }
    return value_len;
}