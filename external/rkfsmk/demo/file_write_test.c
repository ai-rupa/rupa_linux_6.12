#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <rkfsmk.h>

#define SIZE_1MB    (1024 * 1024)

/*
 * Rockchip App
 *
 * Copyright (C) 2017 Rockchip Electronics Co., Ltd.
 * Author: hongjinkun <jinkun.hong@rock-chips.com>
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <linux/netlink.h>
#include <linux/kd.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <sys/time.h>

#define MAX_CH     8
#define MAX_FILE   20
#define FILE_SIZE  (64 * SIZE_1MB)

static int quit = 0;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = 1;
}

void createfile_test(char *device_name)
{
    int i;
    int j;
    int ret;
    char name[128];
    struct timeval tvAllBegin;
    struct timeval tvAllEnd;
    double time_cons;
    void *fmtinfo;
    int num = 0;
    long long disk_size;
    gettimeofday(&tvAllBegin, NULL);
    ret = rkfsmk_create(&fmtinfo, device_name, NULL, 0);
    if (ret < 0) {
        if (ret == -1)
            printf("%s is not unmounted\n", device_name);
        else if (ret == -2)
            printf("%s does not exist\n", device_name);
 
        return;
    }
    disk_size = rkfsmk_disk_size_get(fmtinfo);
    printf("disk_size = %lld\n", disk_size);
    //rkfsmk_add_dir(fmtinfo, "/.hidden/", 1);
    //rkfsmk_add_dir(fmtinfo, "/video0/", 0);
    //rkfsmk_add_dir(fmtinfo, "/video1/", 0);

    for (j = 0; j < MAX_CH; j++) {
        for (i = 0; i < MAX_FILE; i++) {
            sprintf(name, "file_ch%02d_%02d.mp4", j, i);
            rkfsmk_add_file(fmtinfo, "/video/", name, 0, FILE_SIZE);
            num++;
        }
    }

    ret = rkfsmk_start(fmtinfo);
    if (ret == 0)
        printf("format %s success\n", device_name);
    else
        printf("format %s failure\n", device_name);
    rkfsmk_destroy(fmtinfo);
    gettimeofday(&tvAllEnd, NULL);
    time_cons = 1000 * (tvAllEnd.tv_sec - tvAllBegin.tv_sec) + ((tvAllEnd.tv_usec - tvAllBegin.tv_usec) / 1000.0);
    printf("format and create file num = %d time_cons = %fms\n", num, time_cons);
}

void creatfile(char *name, long size)
{
    int fd;
    int buflen = 4 * 1024;
    long size_bk = size;
    unsigned char *buf = (unsigned char *)malloc(buflen);
    printf("%s %s 1 size = %ld\n", __func__, name, size);
    fd = open(name, O_RDWR);
    if (fd) {
        while (size > 0 && !quit) {
            if (size > buflen) {
                write(fd, buf, buflen);
                size -= buflen;
            } else {
                write(fd, buf, size);
                break;
            }
            usleep(1000);
        }
        close(fd);
    }

    //printf("%s %s 2 size = %ld\n", __func__, name, size_bk - size);
    free(buf);
}

static void *creat_file_thread(void *arg)
{
    int ch = (int)arg;
    char name[128];
    int i;
    while (!quit) {
        for (i = 0; i < MAX_FILE && !quit; i++) {
            sprintf(name, "/mnt/sdcard/video/file_ch%02d_%02d.mp4", ch, i);
            creatfile(name, FILE_SIZE);
        }
    }
    //printf("%s ch = %d out\n", __func__, ch);

    //pthread_detach(pthread_self());
    //pthread_exit(NULL);
}

void creat_file_test()
{
    int i = 0;
    pthread_t tid[MAX_CH];
    for (i = 0; i < MAX_CH; i++) {
        pthread_create(&tid[i], NULL, creat_file_thread, i);
    }
    while (!quit) {
        sleep(1);
        printf("sync start\n");
        sync();
        printf("sync end\n");
    }
    for (i = 0; i < MAX_CH; i++) {
        pthread_join(tid[i], NULL);
    }
    sync();
}

int main(int argc , char **argv)
{
    printf("file_write_test in\n");
    signal(SIGINT, sigterm_handler);
    if (argc == 2) {
        createfile_test(argv[1]);
    }
    if (argc == 1) {
        creat_file_test();
    }
    printf("file_write_test out\n");

    return 0;
}