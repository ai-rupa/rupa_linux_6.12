/* SPDX-License-Identifier: GPL-2.0 */

/******************************************************************************
 *
 * Copyright (C) 2020 SeekWave Technology Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ******************************************************************************/
#ifndef __SKW_MDNSOFFLOAD_H__
#define __SKW_MDNSOFFLOAD_H__

#define MDNS_OFFLOAD_PROC_NAME "mdns_offload"
#define MDNS_MAX_RECORDS 512
#define MDNS_MAX_QNAME_LEN 512
#define MDNS_MAX_PACKET_SIZE 4096
#define MDNS_PORT 5353

#define MDNS_MSG_BUFF_LEN 1500

enum mdnsoffload_passthrough_behavior {
	// All the queries are forwarded to the system without any modification.
	MDNS_OFFLOAD_FORWARD_ALL,
	// All the queries are dropped.
	MDNS_OFFLOAD_DROP_ALL,
	// Only the queries present in the passthrough list are forwarded
	// to the system without any modification.
	MDNS_OFFLOAD_PASSTHROUGH_LIST,
};

struct mdns_protocol_data {
	uint8_t *raw_packet;
	size_t packet_len;
	int *match_types;
	int *name_offsets;
	int criteria_count;
	int record_key;
};

struct mdns_record {
	int record_key;
	char ifname[IFNAMSIZ];
	struct mdns_protocol_data *proto_data;
	unsigned long hit_counter;
	struct list_head list;
};

struct passthrough_entry {
	char ifname[IFNAMSIZ];
	struct list_head list;
	char qname[0];
};

struct iface_entry {
	char ifname[IFNAMSIZ];
	u16 passthrough_behavior; // 0: FORWARD_ALL, 1: DROP_ALL, 2: PASSTHROUGH_LIST
	struct skw_iface *iface;
	struct list_head list;
};

struct mdns_offload_device {
	struct proc_dir_entry *proc_main;
	struct proc_dir_entry *proc_config;
	struct proc_dir_entry *proc_control;
	struct proc_dir_entry *proc_stats;

	struct mutex lock;

	bool offload_enabled;
	int next_record_key;

	struct list_head record_list;
	u16 record_count;

	struct list_head passthrough_list;
	u16 passthrough_count;

	u16 iface_count;
	struct list_head iface_list;
};

struct match_rule {
	u16 type;
	u16 name_offset;
} __packed;

struct mdns_match_rules {
	u8 match_cnt;
	struct match_rule rules[0];
} __packed;

struct mdns_raw_data {
	u16 raw_data_len;
	u8 raw_data[0];
} __packed;

struct mdns_offload_rsp_param {
	u8 enable;
	u8 append;
	u8 match_rule_cnt;
} __packed;

struct mdns_offload_filter_param {
	u8 enable;
	u8 append;
	u8 type;
	u16 qname_cnt;
	u8 name[0];
} __packed;

struct skw_mdns_info {
	bool mdns_offload_support;
	u8 mdns_rsp_matchrule_maxunm;
	u16 mdns_rsp_matchrule_buf_maxsize;
	u16 mdns_filter_qname_buf_maxsize;
};

int skw_mdns_update_iface(struct skw_iface *iface);
int skw_mdns_iface_list_add(struct skw_iface *iface);
int skw_mdns_iface_list_del(struct skw_iface *iface);
int skw_mdns_offload_init(struct proc_dir_entry *skw_proc_root,
			  char *parent_name);
int skw_mdns_offload_deinit(void);

#endif
