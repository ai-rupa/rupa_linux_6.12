// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
// Copyright (c) 2025 Fuzhou Rockchip Electronics Co., Ltd

#include "bug_report.h"

static void usage(void)
{
	printf("Usage:bug_report [-w]\n");
	printf("-w read var/log/messages and write to cluster node\n");
	fprintf(stderr, "%s:%d | path: %s \n", __func__, __LINE__, BUG_REPORT_VAR_LOG);
}

int main(int argc, char* argv[])
{
	int ch, ret = -1;

	while ((ch = getopt(argc, argv, "w::")) != -1) {
		switch (ch) {
			case 'w':
				ret = bug_report_read_var_log_and_write_to_cluster();
				break;
			default:
				usage();
				break;
		}
		return ret;
	}
	usage();

	return ret;
}
