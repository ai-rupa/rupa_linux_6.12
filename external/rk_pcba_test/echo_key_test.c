/*
 *  key_test.c  --  key test application
 *
 *  Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *  Author: Bin Yang <yangbin@rock-chips.com>
 *  Author: Panzhenzhuan Wang <randy.wang@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
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
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>

#define LOG_TAG "key_test"
#include "common.h"

#define KEY_TIMEOUT_DOWN    60
#define KEY_TIMEOUT_UP      5
#define KEY_SCAN_FILE_PERMISSION 0766
#define FD_MAX_INPUT_COUNT (8)

typedef struct KEY_BOARD_st {
    char *key_name;
    int key_value;
    int key_press_flag;
    int key_cnt;
} KEY_st;

KEY_st *gkey_test = NULL;
int gkey_test_count = 0;
int gdevice_fds[FD_MAX_INPUT_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1};
int gdevice_count = 0;

static char result[COMMAND_VALUESIZE] = RESULT_FAIL;
static const char* get_key_name(int key_code)
{
    switch (key_code) {
        case KEY_VOLUMEUP:      return "VOLUME_UP";
        case KEY_VOLUMEDOWN:    return "VOLUME_DOWN";
        case KEY_MUTE:          return "MUTE";
        case KEY_MICMUTE:       return "MIC_MUTE";
        case KEY_POWER:         return "POWER";
        case KEY_PLAYPAUSE:     return "PLAY_PAUSE";
        case KEY_PLAY:          return "PLAY";
        case KEY_MODE:          return "MODE";
        case KEY_ESC:           return "ESC";
        case KEY_BACK:          return "BACK";
        case KEY_MENU:          return "MENU";
        case KEY_HOMEPAGE:      return "HOME";
        case KEY_HOME:          return "HOME";
        default:                return NULL;
    }
}

static char* get_device_name(int fd)
{
    static char name[256] = "Unknown";
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        strcpy(name, "Unknown");
    }
    return name;
}

static int get_supported_keys(int fd, unsigned long *bitmask, int bitmask_size)
{
    if (ioctl(fd, EVIOCGBIT(EV_KEY, bitmask_size), bitmask) < 0) {
        log_err("Failed to get supported keys: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int is_key_already_added(int key_code)
{
    int i;
    for (i = 0; i < gkey_test_count; i++) {
        if (gkey_test[i].key_value == key_code) {
            return 1;
        }
    }
    return 0;
}

static int add_key_to_test(int key_code, const char *device_name)
{
    if (is_key_already_added(key_code)) {
        log_info("Key %d already added, skipping\n", key_code);
        return 0;
    }
    KEY_st *new_array = realloc(gkey_test, (gkey_test_count + 1) * sizeof(KEY_st));
    if (!new_array) {
        log_err("Failed to reallocate memory for key array\n");
        return -1;
    }
    gkey_test = new_array;
    const char *standard_name = get_key_name(key_code);
    char *key_name;
    if (standard_name) {
        key_name = strdup(standard_name);
    } else {
        char unknown_name[32];
        snprintf(unknown_name, sizeof(unknown_name), "KEY_%d", key_code);
        key_name = strdup(unknown_name);
    }
    if (!key_name) {
        log_err("Failed to allocate memory for key name\n");
        return -1;
    }
    gkey_test[gkey_test_count].key_name = key_name;
    gkey_test[gkey_test_count].key_value = key_code;
    gkey_test[gkey_test_count].key_press_flag = 0;
    gkey_test[gkey_test_count].key_cnt = 0;
    log_info("Added key: %s (code: %d) from device: %s\n",
             key_name, key_code, device_name);
    gkey_test_count++;
    return 0;
}

static int init_dynamic_keys(void)
{
    unsigned long bitmask[KEY_MAX/8/sizeof(unsigned long) + 1] = {0};
    int bitmask_size = sizeof(bitmask);
    int i, j;
    gkey_test = NULL;
    gkey_test_count = 0;
    for (i = 0; i < gdevice_count; i++) {
        int fd = gdevice_fds[i];
        if (fd < 0) continue;
        char *device_name = get_device_name(fd);
        if (get_supported_keys(fd, bitmask, bitmask_size) < 0) {
            continue;
        }
        for (j = 0; j < KEY_MAX; j++) {
            if (j < KEY_RESERVED) continue;
            if (bitmask[j/8/sizeof(unsigned long)] & (1UL << (j % (8 * sizeof(unsigned long))))) {
                if (add_key_to_test(j, device_name) < 0) {
                    log_err("Failed to add key %d from device %s\n", j, device_name);
                }
            }
        }
    }
    if (gkey_test_count == 0) {
        log_err("No supported keys found in any input device\n");
        return -1;
    }
    log_info("Total dynamic keys: %d\n", gkey_test_count);
    return 0;
}

static int find_input_devices(const char **target_devices, int target_count)
{
    DIR *dir;
    struct dirent *entry;
    char path[64];
    int found_count = 0;
    dir = opendir("/dev/input");
    if (!dir) {
        log_err("Cannot open /dev/input directory\n");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL && found_count < target_count) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            int fd = open(path, O_RDONLY | O_NOCTTY | O_NONBLOCK);
            if (fd >= 0) {
                char name[256];
                if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                    log_info("Found device %s: %s\n", path, name);
                    for (int i = 0; i < target_count; i++) {
                        if (strstr(name, target_devices[i]) != NULL) {
                            gdevice_fds[gdevice_count++] = fd;
                            found_count++;
                            log_info("Added target device: %s (%s)\n", name, path);
                            break;
                        }
                    }
                    if (gdevice_count < found_count) {
                        close(fd);
                    }
                } else {
                    close(fd);
                }
            }
        }
    }
    closedir(dir);
    return found_count;
}

static int isAllKeyPressed(KEY_st *key_array)
{
    int i = 0;
    if (key_array == NULL)
        return 0;
    for (i = 0; i < gkey_test_count; i++) {
        if (key_array[i].key_press_flag)
            continue;
        else
            break;
    }
    if (i == gkey_test_count) {
        log_info("########### All key had pressed!!! ######### \n");
        return 1;
    } else
        return 0;
}

static void dumpKeyPressInfo(KEY_st *key_array)
{
    int i = 0;
    if (key_array == NULL)
        return;
    for (i = 0; i < gkey_test_count; i++) {
        log_info("KEYNAME : %s: \t KEY_press: %d \n", key_array[i].key_name,
                 key_array[i].key_press_flag);
    }
}

static char *assemble_info(char *key_info_buf, int buf_size, int key_index)
{
    int error_code = 0;
    char msg[64] = {0};
    if (key_info_buf == NULL)
        return NULL;
    memset(key_info_buf, 0, buf_size);
    memset(msg, 0, sizeof(msg));
    snprintf(msg, sizeof(msg), "key name:%s(code:%d)",
             gkey_test[key_index].key_name, gkey_test[key_index].key_value);
    snprintf(key_info_buf, buf_size, "x<%s>,<%s>,<%d>", msg, RESULT_KEY_PRESS, error_code);
    return key_info_buf;
}

static int save_scan_result(char *result_buf)
{
    int fd = -1;
    int ret = 0;
    char *bin_name = "echo_key_test";
    char result_filename[COMMAND_VALUESIZE] = {0};
    snprintf(result_filename, sizeof(result_filename),
             "%s/%s_result",
             TEST_RESULT_SAVE_PATH,
             bin_name);
    fd = open(result_filename, O_CREAT | O_WRONLY | O_TRUNC, KEY_SCAN_FILE_PERMISSION);
    if (fd < 0) {
        log_err("open %s fail, errno = %d\n", result_filename, errno);
        ret = errno;
        goto out;
    }
    int w_len = write(fd, result_buf, strlen(result_buf));
    if (w_len <= 0) {
        log_err("Write %s fail, errno = %d\n", result_filename, errno);
        ret = errno;
    }
    log_info("\t -------key_test_result: %s -------\n", result_buf);
    log_info("########## write to %s len %d ##########\n", result_filename, w_len);
out:
    if (fd >= 0)
        close(fd);
    return ret;
}

static int key_event_read(int fd, struct input_event *buf)
{
    int read_len = 0;
    int retry_count = 3;
    while (retry_count-- > 0) {
        read_len = read(fd, buf, sizeof(*buf));
        if (read_len < 0) {
            if (errno == EINTR) {
                usleep(10000); // 10ms
                continue;
            }
            if (errno != EAGAIN) {
                log_err("read error: %s\n", strerror(errno));
                return -1;
            }
        }
        break;
    }
    if (read_len != sizeof(*buf)) {
        log_err("read incomplete: %d != %zu\n", read_len, sizeof(*buf));
        return -1;
    }
    return 1;
}

static int key_result_send(int sign_no)
{
    int err_code = 0;
    printf("====================function : %s start =================\n",__func__);
    strcpy(result, RESULT_VERIFY);
    send_msg_to_server("key_test", result, err_code);
    printf("====================function : %s finished =================\n",__func__);
    exit(0);
}

int main(int argc, char **argv)
{
    int ret = 0;
    int err_code = 0;
    int time = KEY_TIMEOUT_DOWN;
    fd_set rdfds;
    struct input_event key_event;
    char buf[COMMAND_VALUESIZE] = "key_test";
    int idx = 0;
    int max_fd = 0;
    int i, k;
    struct timeval sel_timeout_tv;
    log_info("key test process start...\n");
    signal(SIGTERM, (__sighandler_t)key_result_send);
    const char *target_devices[] = {"adc-keys", "pwrkey"};
    int target_count = sizeof(target_devices) / sizeof(target_devices[0]);
    int found_devices = find_input_devices(target_devices, target_count);
    if (found_devices < 1) {
        log_err("Failed to find any target input devices\n");
        err_code = KEY_OPEN_FAIL;
        goto EXIT;
    }
    log_info("Found %d target devices\n", found_devices);
    if (init_dynamic_keys() < 0) {
        log_err("Failed to initialize dynamic keys\n");
        err_code = KEY_OPEN_FAIL;
        goto EXIT;
    }
    for (i = 0; i < gdevice_count; i++) {
        if (gdevice_fds[i] > max_fd)
            max_fd = gdevice_fds[i];
    }
    log_info("Starting key test with %d dynamic keys on %d devices\n",
             gkey_test_count, gdevice_count);
    while (1) {
        FD_ZERO(&rdfds);
        sel_timeout_tv.tv_sec = time;
        sel_timeout_tv.tv_usec = 0;
        for (i = 0; i < gdevice_count; i++) {
            if (gdevice_fds[i] >= 0) {
                FD_SET(gdevice_fds[i], &rdfds);
            }
        }
        if (isAllKeyPressed(gkey_test)) {
            log_info("All keys pressed, test completed\n");
            break;
        }
        ret = select(max_fd + 1, &rdfds, NULL, NULL, &sel_timeout_tv);
        if (ret < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by a signal; continue waiting.
            }
            log_err("select error: %s\n", strerror(errno));
            err_code = KEY_EVENT_TIMEOUT;
            break;
        } else if (ret == 0) {
            log_err("wait key event timeout\n");
            memset(buf, 0, sizeof(buf));
            strcpy(buf, "Not pressed: ");
            int first = 1;
            for (idx = 0; idx < gkey_test_count; idx++) {
                if (gkey_test[idx].key_press_flag == 0) {
                    if (!first) {
                        strcat(buf, ", ");
                    }
                    strcat(buf, gkey_test[idx].key_name);
                    first = 0;
                }
            }
            err_code = KEY_EVENT_TIMEOUT;
            goto EXIT;
        } else {
            for (k = 0; k < gdevice_count; k++) {
                int fd = gdevice_fds[k];
                if (fd < 0 || !FD_ISSET(fd, &rdfds)) {
                    continue;
                }
                ret = key_event_read(fd, &key_event);
                if (ret > 0) {
                    char *device_name = get_device_name(fd);
                    log_info("Event from %s: type=%d, code=%d, value=%d\n",
                            device_name, key_event.type, key_event.code, key_event.value);
                    if (key_event.type != EV_KEY) {
                        continue;
                    }
                    if (key_event.value) { // Button pressed
                        log_info("key(%d) is down from %s\n", key_event.code, device_name);
                        time = KEY_TIMEOUT_UP;
                    } else { // Button release
                        log_info("key(%d) is up from %s\n", key_event.code, device_name);
                        time = KEY_TIMEOUT_DOWN;
                        int key_processed = 0;
                        for (idx = 0; idx < gkey_test_count; idx++) {
                            if (key_event.code == gkey_test[idx].key_value) {
                                if (gkey_test[idx].key_press_flag == 0) {
                                    gkey_test[idx].key_press_flag = 1;
                                    gkey_test[idx].key_cnt = 1;
                                    log_info("Key %s pressed for the first time from %s\n",
                                            gkey_test[idx].key_name, device_name);
                                } else {
                                    gkey_test[idx].key_cnt++;
                                    log_info("Key %s pressed %d times\n",
                                            gkey_test[idx].key_name, gkey_test[idx].key_cnt);
                                }
                                assemble_info(buf, sizeof(buf), idx);
                                save_scan_result(buf);
                                key_processed = 1;
                                break;
                            }
                        }
                        if (!key_processed) {
                            log_info("key(%d) from %s is not in test key list\n",
                                    key_event.code, device_name);
                        }
                        if (isAllKeyPressed(gkey_test)) {
                            log_info(" ======== key test is over ========= \n");
                            strcpy(result, RESULT_VERIFY);
                            goto EXIT;
                        }
                    }
                } else if (ret < 0) {
                    log_err("key event read fail from fd %d, errno=%d\n", fd, errno);
                }
            }
        }
    }
    dumpKeyPressInfo(gkey_test);
EXIT:
    sleep(1);
    for (i = 0; i < gdevice_count; i++) {
        if (gdevice_fds[i] >= 0) {
            close(gdevice_fds[i]);
            gdevice_fds[i] = -1;
        }
    }
    if (gkey_test) {
        for (idx = 0; idx < gkey_test_count; idx++) {
            if (gkey_test[idx].key_name) {
                free(gkey_test[idx].key_name);
            }
        }
        free(gkey_test);
        gkey_test = NULL;
    }
    memset(buf, 0, sizeof(buf));
    if (!err_code) {
        strcpy(result, RESULT_VERIFY);
        snprintf(buf, sizeof(buf), "All %d keys tested successfully", gkey_test_count);
    } else {
        strcpy(result, RESULT_FAIL);
    }
    send_msg_to_server(buf, result, err_code);
    log_info("key test process exit with code: %d\n", err_code);
    return err_code;
}