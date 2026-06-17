/*
 *  camera_test.c  --  camera test application
 *
 *  Copyright (c) 2025 Rockchip Electronics Co. Ltd.
 *  Author: Embedded Linux Expert
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
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#define LOG_TAG "camera_test"
#include "common.h"

#define CAMERA_DIR "/tmp/camera"
#define CAPTURE_SCRIPT "/data/v4l2_capture.sh"
#define CHECK_SCRIPT "/data/photo_check.sh"

static char camera_device[64] = "/dev/video0";

static int check_device_with_v4l2ctl(const char *device_path)
{
    char cmd[128];
    int ret;
    snprintf(cmd, sizeof(cmd), "v4l2-ctl --device=%s --list-formats > /dev/null 2>&1", device_path);
    ret = system(cmd);
    return (ret == 0) ? 1 : 0;
}

static int is_valid_camera_device(const char *device_path)
{
    int fd;
    struct v4l2_capability cap;
    fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        return 0;
    }
    close(fd);
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        (cap.capabilities & V4L2_CAP_STREAMING)) {
        log_info("Found valid camera: %s (%s)\n", device_path, cap.card);
        log_info("  Capabilities: 0x%08x\n", cap.capabilities);
        log_info("  Device Caps: 0x%08x\n", cap.device_caps);
        return 1;
    } else {
        log_info("Device %s not suitable: capabilities=0x%08x, device_caps=0x%08x\n",
                device_path, cap.capabilities, cap.device_caps);
        return 0;
    }
}

static int find_camera_device(void)
{
    DIR *dir;
    struct dirent *entry;
    char device_path[64];
    int found = 0;
    log_info("Scanning for camera devices...\n");
    dir = opendir("/dev");
    if (dir == NULL) {
        log_err("Cannot open /dev directory: %s\n", strerror(errno));
        return -1;
    }
    for (int i = 0; i < 10; i++) {
        snprintf(device_path, sizeof(device_path), "/dev/video%d", i);
        if (access(device_path, F_OK) == 0) {
            log_info("Testing device: %s\n", device_path);
            if (is_valid_camera_device(device_path)) {
                strncpy(camera_device, device_path, sizeof(camera_device)-1);
                found = 1;
                log_info("Selected camera device: %s\n", camera_device);
                break;
            }
        }
    }
    if (!found) {
        rewinddir(dir);
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "video", 5) == 0) {
                snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
                if (strlen(entry->d_name) == 6 && isdigit(entry->d_name[5])) {
                    continue;
                }
                log_info("Testing non-standard device: %s\n", device_path);
                if (is_valid_camera_device(device_path)) {
                    strncpy(camera_device, device_path, sizeof(camera_device)-1);
                    found = 1;
                    log_info("Selected camera device: %s\n", camera_device);
                    break;
                }
            }
        }
    }
    closedir(dir);
    if (!found) {
        log_info("Trying alternative detection method...\n");
        for (int i = 0; i < 10; i++) {
            snprintf(device_path, sizeof(device_path), "/dev/video%d", i);
            if (access(device_path, F_OK) == 0 && check_device_with_v4l2ctl(device_path)) {
                strncpy(camera_device, device_path, sizeof(camera_device)-1);
                found = 1;
                log_info("Selected camera device (alternative method): %s\n", camera_device);
                break;
            }
        }
    }
    if (!found) {
        log_err("No valid camera device found in /dev/\n");
        return -1;
    }
    return 0;
}

static int create_camera_dir(void)
{
    if (access(CAMERA_DIR, F_OK) != 0) {
        if (mkdir(CAMERA_DIR, 0755) != 0) {
            log_err("Failed to create camera directory: %s\n", strerror(errno));
            return -1;
        }
        log_info("Created camera directory: %s\n", CAMERA_DIR);
    }
    return 0;
}

static int check_camera_device(void)
{
    if (find_camera_device() != 0) {
        log_err("Failed to find any camera device\n");
        return -1;
    }
    if (system("which v4l2-ctl > /dev/null 2>&1") != 0) {
        log_err("v4l2-ctl not found\n");
        return -1;
    }
    log_info("Camera device check passed, using: %s\n", camera_device);
    return 0;
}

static int capture_image_test(void)
{
    pid_t pid;
    int status;
    pid = fork();
    if (pid < 0) {
        log_err("Fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        char *argv[] = {CAPTURE_SCRIPT, camera_device, NULL};
        char *envp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin",NULL};
        execve(CAPTURE_SCRIPT, argv, envp);
        log_err("[%s]execve failed: %s\n", __FUNCTION__, strerror(errno));
        exit(1);
    } else {
        if (waitpid(pid, &status, 0) < 0) {
            log_err("waitpid failed: %s\n", strerror(errno));
            return -1;
        }
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                log_info("Image capture test passed\n");
                return 0;
            } else {
                log_err("Image capture test failed with exit code: %d\n", exit_status);
                return -1;
            }
        } else {
            log_err("Image capture test process terminated abnormally\n");
            return -1;
        }
    }
}

static int check_photo_test(void)
{
    pid_t pid;
    int status;
    char cmd[256];
    char image_path[256] = {0};
    FILE *fp;
    snprintf(cmd, sizeof(cmd), "ls -t %s/*.jpg 2>/dev/null | head -1", CAMERA_DIR);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        log_err("Failed to find captured images\n");
        return -1;
    }
    if (fgets(image_path, sizeof(image_path), fp) == NULL) {
        pclose(fp);
        log_err("No captured images found\n");
        return -1;
    }
    pclose(fp);
    size_t len = strlen(image_path);
    if (len > 0 && image_path[len-1] == '\n') {
        image_path[len-1] = '\0';
    }
    if (strlen(image_path) == 0) {
        log_err("Invalid image path\n");
        return -1;
    }
    log_info("Testing image: %s\n", image_path);
    pid = fork();
    if (pid < 0) {
        log_err("Fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        char *argv[] = {CHECK_SCRIPT, image_path, NULL};
        char *envp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL};
        execve(CHECK_SCRIPT, argv, envp);
        log_err("[%s]execve failed: %s\n", __FUNCTION__, strerror(errno));
        exit(1);
    } else {
        if (waitpid(pid, &status, 0) < 0) {
            log_err("waitpid failed: %s\n", strerror(errno));
            return -1;
        }
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                log_info("Photo check test passed\n");
                return 0;
            } else {
                log_err("Photo check test failed with exit code: %d\n", exit_status);
                return -1;
            }
        } else {
            log_err("Photo check test process terminated abnormally\n");
            return -1;
        }
    }
}

static int remove_captured_images(void)
{
    int status;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -f %s/*.jpg", CAMERA_DIR);
    status = system(cmd);
    if (status != 0) {
        log_err("Failed to remove captured images\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int test_flag = 0, err_code = 0;
    char buf[COMMAND_VALUESIZE] = "camera_test";
    char result[COMMAND_VALUESIZE] = RESULT_FAIL;
    int ret;
    log_info("=============== camera test start...============\n");
    if (create_camera_dir() != 0) {
        err_code = CAMERA_PROC_ERR;
        goto test_end;
    }
    if (check_camera_device() != 0) {
        err_code = CAMERA_PROC_ERR;
        goto test_end;
    }
    if (capture_image_test() != 0) {
        err_code = CAMERA_PROC_ERR;
        goto test_end;
    }
    if (check_photo_test() != 0) {
        err_code = CAMERA_PROC_ERR;
        goto test_end;
    }
    test_flag = 1;
    err_code = 0;
test_end:
    if (test_flag) {
        strcpy(result, RESULT_PASS);
        log_info("=============== camera test PASSED ============\n");
    } else {
        strcpy(result, RESULT_FAIL);
        log_info("=============== camera test FAILED ============\n");
    }
    printf("######## send msg to server... ######\n");
    send_msg_to_server(buf, result, err_code);
    remove_captured_images();
    return test_flag ? 0 : 1;
}