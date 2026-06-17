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

void createfile_test(char *device_name, char *volume_name)
{
    int i;
    int ret;
    char name[128];
    struct timeval tvAllBegin;
    struct timeval tvAllEnd;
    double time_cons;
    void *fmtinfo;
    int num = 0;
    long long disk_size;
    gettimeofday(&tvAllBegin, NULL);
    ret = rkfsmk_create(&fmtinfo, device_name, volume_name, 0);
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
#if 1
    for (i = 0; i < 500; i++) {
        sprintf(name, "fileadbcederf%d.mp4", i);
        rkfsmk_add_file(fmtinfo, "/pic0/", name, 0, 3 * SIZE_1MB);
        num++;
    }
    for (i = 0; i < 500; i++) {
        sprintf(name, "fileadbcederf%d.mp4", i);
        rkfsmk_add_file(fmtinfo, "/pic1/", name, 0, 3 * SIZE_1MB);
        num++;
    }
    for (i = 0; i < 500; i++) {
        sprintf(name, "fileadbcederf%d.mp4", i);
        rkfsmk_add_file(fmtinfo, "/pic2/", name, 0, 3 * SIZE_1MB);
        num++;
    }
    for (i = 0; i < 500; i++) {
        sprintf(name, "fileadbcederf%d.mp4", i);
        rkfsmk_add_file(fmtinfo, "/pic3/", name, 0, 3 * SIZE_1MB);
        num++;
    }
#endif
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

void only_format(char *device_name, char *volume_name)
{
    int i = 0;
    for (i = 0; i < 1; i++) {
        int ret = rkfsmk_format(device_name, volume_name);
        printf("%s ret = %d\n", __func__, ret);
    }
}

void check_format(char *path)
{
    int ret = kernel_chk_format(path);
    printf("%s %d\n", __func__, ret);
}

int main(int argc , char **argv)
{
    if (argc == 2) {
        check_format(argv[1]);
    } else if (argc == 3) {
        only_format(argv[1], argv[2]);
    } else {
    	
    }

    return 0;
}