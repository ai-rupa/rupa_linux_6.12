/*
 *  emmc_test.c  --  emmc test application
 *
 *  Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *  Author: Panzhenzhuan Wang <randy.wang@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "unistd.h"
#include <dirent.h>

#include "emmc_test.h"

#define LOG_TAG "emmc_test"
#include "common.h"

#define READ_DDR_COMMAND "cat /proc/zoneinfo | busybox grep present | \
                        busybox awk 'BEGIN{a=0}{a+=$2}END{print a}'"

#define RKNAND_TEST_CMD "cat /proc/rknand > %s"
#define RKNAND_TEST_FILE "/tmp/rknand_test.txt"

int is_valid_emmc_path(const char *path)
{
    char size_file[512];
    char dev_file[512];
    FILE *fp;
    if (access(path, F_OK) != 0) {
        return 0;
    }
    snprintf(size_file, sizeof(size_file), "%s/size", path);
    if (access(size_file, R_OK) != 0) {
        return 0;
    }
    snprintf(dev_file, sizeof(dev_file), "%s/dev", path);
    if (access(dev_file, R_OK) != 0) {
        return 0;
    }
    fp = fopen(size_file, "r");
    if (fp) {
        char buffer[64];
        if (fgets(buffer, sizeof(buffer), fp)) {
            unsigned long size = strtoul(buffer, NULL, 10);
            fclose(fp);
            return (size > 0);
        }
        fclose(fp);
    }
    return 0;
}

char* get_emmc_device_node(const char *sysfs_path)
{
    static char device_node[64];
    const char *block_name = strrchr(sysfs_path, '/');
    if (block_name) {
        block_name++; // skip '/'
        snprintf(device_node, sizeof(device_node), "/dev/%s", block_name);
        return device_node;
    }
    return NULL;
}

char* scan_sysfs_for_emmc(void)
{
    static char emmc_path[512];
    DIR *dir;
    struct dirent *entry;
    char path[512];
    dir = opendir("/sys/bus/mmc/devices");
    if (!dir) {
        return NULL;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/mmc/devices/%s/block", entry->d_name);
        DIR *block_dir = opendir(path);
        if (block_dir) {
            struct dirent *block_entry;
            while ((block_entry = readdir(block_dir)) != NULL) {
                if (strcmp(block_entry->d_name, ".") == 0 ||
                    strcmp(block_entry->d_name, "..") == 0) {
                    continue;
                }
                if (strncmp(block_entry->d_name, "mmcblk", 6) == 0) {
                    snprintf(emmc_path, sizeof(emmc_path),
                            "/sys/bus/mmc/devices/%s/block/%s",
                            entry->d_name, block_entry->d_name);
                    if (is_valid_emmc_path(emmc_path)) {
                        closedir(block_dir);
                        closedir(dir);
                        printf("Found EMMC by scanning: %s\n", emmc_path);
                        return emmc_path;
                    }
                }
            }
            closedir(block_dir);
        }
    }
    closedir(dir);
    return NULL;
}

char* find_emmc_by_type(void)
{
    char path[512];
    FILE *fp;
    DIR *dir = opendir("/sys/block");
    if (!dir) {
        return NULL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "mmcblk", 6) == 0) {
            snprintf(path, sizeof(path), "/sys/block/%s/device/type", entry->d_name);
            fp = fopen(path, "r");
            if (fp) {
                char type[16];
                if (fgets(type, sizeof(type), fp)) {
                    if (strstr(type, "MMC")) {
                        snprintf(path, sizeof(path), "/sys/block/%s", entry->d_name);
                        fclose(fp);
                        closedir(dir);
                        char real_path[512];
                        if (realpath(path, real_path) != NULL) {
                            printf("Found EMMC by type: %s\n", real_path);
                            return strdup(real_path);
                        }
                    }
                }
                fclose(fp);
            }
        }
    }
    closedir(dir);
    return NULL;
}

char* get_emmc_path(void)
{
    char *path = NULL;
    path = scan_sysfs_for_emmc();
    if (path) {
        return path;
    }
    path = find_emmc_by_type();
    if (path) {
        return path;
    }
    printf("No EMMC path found\n");
    return NULL;
}

/* for emmc  */
static int readFromFile(const char *path, char *emmcsize_char, size_t size)
{
    if (!path)
        return -1;
    char sizepath[256];
    snprintf(sizepath, sizeof(sizepath), "%s/size", path);
    int fd = open(sizepath, O_RDONLY|O_NOCTTY|O_NDELAY, 0);
    if (fd == -1) {
        printf("open '%s' failed!\n", sizepath);
        return -1;
    }
    ssize_t count = read(fd, emmcsize_char, size);
    if (count > 0) {
        while (count > 0 && emmcsize_char[count-1] == '\n')
            count--;
            emmcsize_char[count] = '\0';
    } else {
        emmcsize_char[0] = '\0';
    }
    close(fd);
    return count;
}

int get_emmc_size(char *size_data)
{
    int count;
    int emmc_size=1;
    double size = (double)(atoi(size_data))/2/1024/1024;
    if (size > 0 && size <= 1)  /*1 GB */
        return 1;
    for (count = 1; count < 10; count++)
    {
        if ((size > emmc_size) && (size <= emmc_size*2))   /*2 - 512 GB*/
            return emmc_size*2;

        emmc_size *=2;
    }
    return -1;
}

void *emmc_test(void *argv)
{
    int emmc_ret = 0;
    char emmcsize_char[20];
    int emmc_size = 0;
    int ddr_ret = 0;
    char ddrsize_char[20];
    int ddr_size = 0;
    char cmd[128];
    char *emmcpath = get_emmc_path();
    if (!emmcpath) {
        printf("No EMMC path found\n");
        goto fail;
    }
    printf("=======  emmc test starting   ========\n");
    //sprintf(cmd,"aplay %s/emmc_test_start.wav",AUDIO_PATH);
    //system(cmd);
    //system("aplay /data/test/emmc_test_start.wav");
    memset(emmcsize_char, 0, sizeof(emmcsize_char));
    emmc_ret = readFromFile(emmcpath, emmcsize_char, sizeof(emmcsize_char));
    printf("readFromFile effective bytes is %d \n",emmc_ret);
    if (emmc_ret >= 0) {  /*read back normal*/
        emmc_size = get_emmc_size(emmcsize_char);
        if(emmc_size < 0){
            emmc_ret = -1;
            goto fail;
        }
        printf("=======  emmc_size is: %d GB ========\n",emmc_size);
    } else {
        goto fail;
    }
    printf("=======  emmc_test success  ========\n");
    return (void*)emmc_ret;
fail:
    printf("=======  emmc_test failed  ========\n");
    return (void*)emmc_ret;
}

int main(int argc, char *argv[])
{
    int test_flag = 0,err_code = 0;
    char buf[COMMAND_VALUESIZE] = "emmc_test";
    char result[COMMAND_VALUESIZE] = RESULT_PASS;
    test_flag = (int)emmc_test(argv[0]);
    if (test_flag < 0) {
        strcpy(result,RESULT_FAIL);
        err_code = EMMC_PROC_ERR;
    }
    send_msg_to_server(buf, result, err_code);
}
