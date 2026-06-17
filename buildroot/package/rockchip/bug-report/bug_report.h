/*
 *  Copyright (c) 2025, Luo Wei <lw@rock-chips.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __BUG_REPORT_H_
#define __BUG_REPORT_H_

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUG_REPORT_CLUSTER "/sys/devices/platform/vehicle-bug-report/dmesg/cluster"
#define BUG_REPORT_VAR_LOG "/var/log/messages"
#define SIZE_OF_VAR_LOG 0x040000

int bug_report_read_var_log_and_write_to_cluster(void);

#ifdef __cplusplus
}
#endif

#endif // __BUG_REPORT_H_
