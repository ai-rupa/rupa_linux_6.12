// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
// Copyright (c) 2025 Fuzhou Rockchip Electronics Co., Ltd

#include "bug_report.h"

static int sysfs_write(char *path, char *val, unsigned int size)
{
	int len;
	int i = 0, num = 0;
	int fd = 0;

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		//fprintf(stderr, "%s:%d | path: %s %s\n", __func__, __LINE__, path, strerror(errno));
		return -1;
	}

	size = lseek(fd, 0, SEEK_END);
	if (size < 0) {
		//printf("%s lseek failed\n", __func__);
		close(fd);
		return -1;
	}

	num = size / 4096;
	//printf("%s: size=%d num=%d\n", __func__, size, num);

	for (i = 0; i < num; i++) {
		lseek(fd, 4096 * i, SEEK_SET);
		//printf("%s: offset=%d\n", __func__, i*4096);
		len = write(fd, val + (4096 * i), 4096);
		if (len < 0) {
			//fprintf(stderr, "%s:%d | path: %s %s\n", __func__, __LINE__, path, strerror(errno));
			close(fd);
			return -1;
		}
	}
	close(fd);

	return 0;
}

static void sysfs_read(char *path, char *val)
{
	int len;
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		//fprintf(stderr, "%s:%d | path: %s %s\n", __func__, __LINE__, path, strerror(errno));
		return;
	}

	len = read(fd, val, 1);
	if (len < 0) {
		//fprintf(stderr, "%s:%d | path: %s %s\n", __func__, __LINE__, path, strerror(errno));
	}
	//printf("%s: %s\n", __func__, val);
	close(fd);
}

int bug_report_read_var_log_and_write_to_cluster(void)
{
	int is_true;
	FILE *stream;
	void *data;

	int fd = open(BUG_REPORT_VAR_LOG, O_RDWR);
	if (fd < 0) {
                //fprintf(stderr, "%s:%d | var_log %s\n", __func__, __LINE__, strerror(errno));
                return -1;
        }

	data = mmap(NULL, SIZE_OF_VAR_LOG, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		//fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	is_true = access(BUG_REPORT_VAR_LOG, R_OK);
        if (!is_true) {
		//fprintf(stderr, "%s:%d | path: %s \n", __func__, __LINE__, BUG_REPORT_VAR_LOG);
		sysfs_read(BUG_REPORT_VAR_LOG, (char *)data);
        }

        is_true = access(BUG_REPORT_CLUSTER, R_OK);
        if (!is_true) {
		//fprintf(stderr, "%s:%d | path: %s \n", __func__, __LINE__, BUG_REPORT_CLUSTER);
                sysfs_write(BUG_REPORT_CLUSTER, (char *)data, SIZE_OF_VAR_LOG);
        }

	close(fd);
        return 0;
}
