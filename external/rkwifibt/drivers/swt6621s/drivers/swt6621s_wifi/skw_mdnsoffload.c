// SPDX-License-Identifier: GPL-2.0

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
#include <generated/utsrelease.h>
#include "skw_core.h"
#include "skw_dentry.h"
#include "skw_compat.h"
#include "version.h"
#include "skw_msg.h"
#include "skw_mdnsoffload.h"

static struct mdns_offload_device g_mdns_offload_dev = { 0 };

static void skw_mdns_rsp_head_init(u8 *buff, u16 msg_len, bool append,
				   u16 rules)
{
	u8 *pos = buff;
	struct skw_spd_action_param *spd = NULL;
	struct mdns_offload_rsp_param *mdns = NULL;

	spd = (struct skw_spd_action_param *)pos;
	spd->sub_cmd = ACTION_MDNS_RSP_PARAMS;
	spd->len = msg_len - sizeof(struct skw_spd_action_param);
	pos += sizeof(struct skw_spd_action_param);

	mdns = (struct mdns_offload_rsp_param *)pos;
	mdns->enable = g_mdns_offload_dev.offload_enabled;
	mdns->append = append;
	mdns->match_rule_cnt = rules;
}

static int skw_mdns_rsp_params(struct skw_iface *iface)
{
	struct skw_core *skw = NULL;
	u8 *buff = NULL, *pos = NULL;
	u16 buff_len = MDNS_MSG_BUFF_LEN, data_len = 0;
	u16 head_len = 0, remain_len = 0, record_len = 0, rules = 0;
	struct mdns_record *record = NULL, *tmp = NULL;
	bool append = false;
	int ret = 0, index = 0;

	if (!iface || !iface->ndev)
		return -EINVAL;

	buff = SKW_ZALLOC(buff_len, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	skw = iface->skw;
	remain_len = buff_len - sizeof(struct skw_spd_action_param);
	head_len = sizeof(struct skw_spd_action_param) +
		   sizeof(struct mdns_offload_rsp_param);
	data_len = head_len;
	pos = buff + head_len;

	if (!g_mdns_offload_dev.offload_enabled) {
		skw_mdns_rsp_head_init(buff, data_len, append, rules);

		skw_hex_dump("mdns_rsp:", buff, data_len, false);

		ret = skw_msg_xmit(priv_to_wiphy(skw), iface->id,
				   SKW_CMD_SET_SPD_ACTION, buff, data_len, NULL,
				   0);
		if (ret) {
			skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u,enable=0\n",
				iface->ndev->name, ret, data_len, append,
				rules);
		}
		goto exit;
	}

	list_for_each_entry_safe(record, tmp, &g_mdns_offload_dev.record_list,
				  list) {
		if (!strcmp(record->ifname, iface->ndev->name)) {
			record_len =
				1 +
				sizeof(struct match_rule) *
					record->proto_data->criteria_count +
				2 + record->proto_data->packet_len;
			if (remain_len < record_len) {
				if (unlikely(!rules)) {
					skw_err("record len=%u exceed length of buff\n",
						record_len);
					ret = -ENOMEM;
					break;
				}

				skw_mdns_rsp_head_init(buff, data_len, append,
						       rules);

				skw_hex_dump("mdns_rsp:", buff, data_len,
					     false);

				ret = skw_msg_xmit(priv_to_wiphy(skw),
						   iface->id,
						   SKW_CMD_SET_SPD_ACTION, buff,
						   data_len, NULL, 0);
				if (ret) {
					skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u\n",
						iface->ndev->name, ret,
						data_len, append, rules);
					rules = 0;
					break;
				}

				skw_dbg("[%s] send success,data_len=%u,append=%u,rules=%u\n",
					iface->ndev->name, data_len, append,
					rules);

				append = true;
				rules = 0;
				remain_len =
					buff_len -
					sizeof(struct skw_spd_action_param);
				data_len = head_len;
				pos = buff + head_len;
			}

			rules++;
			*pos++ = record->proto_data->criteria_count;
			for (index = 0;
			     index < record->proto_data->criteria_count;
			     index++) {
				((struct match_rule *)pos)->type =
					record->proto_data->match_types[index];
				((struct match_rule *)pos)->name_offset =
					record->proto_data->name_offsets[index];
				pos += sizeof(struct match_rule);
			}

			((struct mdns_raw_data *)pos)->raw_data_len =
				record->proto_data->packet_len;
			pos += 2;
			memcpy(pos, record->proto_data->raw_packet,
			       record->proto_data->packet_len);
			pos += record->proto_data->packet_len;

			data_len = pos - buff;
			remain_len = buff_len - data_len;

			skw_dbg("[%s] record key=%d,criteria_count=%d,packet_len=%lu\n",
				record->ifname, record->record_key,
				record->proto_data->criteria_count,
				record->proto_data->packet_len);
		}
	}

	if (!append && !rules) {
		skw_dbg("[%s] exit no rules\n", iface->ndev->name);
		ret = -EINVAL;
		goto exit;
	}

	skw_mdns_rsp_head_init(buff, data_len, append, rules);

	skw_hex_dump("mdns_rsp:", buff, data_len, false);

	ret = skw_msg_xmit(priv_to_wiphy(skw), iface->id,
			   SKW_CMD_SET_SPD_ACTION, buff, data_len, NULL, 0);
	if (ret) {
		skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u\n",
			iface->ndev->name, ret, data_len, append, rules);
		goto exit;
	}

	skw_dbg("[%s] send success,data_len=%u,append=%u,rules=%u\n",
		iface->ndev->name, data_len, append, rules);

exit:
	SKW_KFREE(buff);

	return ret;
}

static void skw_mdns_filter_head_init(u8 *buff, u16 msg_len, bool append,
				      u8 type, u16 qname_cnt)
{
	u8 *pos = buff;
	struct skw_spd_action_param *spd = NULL;
	struct mdns_offload_filter_param *mdns = NULL;

	spd = (struct skw_spd_action_param *)pos;
	spd->sub_cmd = ACTION_MDNS_FILTER_PARAMS;
	spd->len = msg_len - sizeof(struct skw_spd_action_param);
	pos += sizeof(struct skw_spd_action_param);

	mdns = (struct mdns_offload_filter_param *)pos;
	mdns->enable = g_mdns_offload_dev.offload_enabled;
	mdns->append = append;
	mdns->type = type;
	mdns->qname_cnt = qname_cnt;
}

static int skw_mdns_filter_params(struct skw_iface *iface, u16 behavior)
{
	struct skw_core *skw = NULL;
	u8 *buff = NULL, *pos = NULL;
	u16 buff_len = MDNS_MSG_BUFF_LEN, data_len = 0;
	u16 head_len = 0, remain_len = 0, record_len = 0, rules = 0;
	struct passthrough_entry *pt_entry = NULL, *tmp_pt = NULL;
	bool append = false;
	int ret = 0;

	if (!iface || !iface->ndev)
		return -EINVAL;

	buff = SKW_ZALLOC(buff_len, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	skw = iface->skw;
	remain_len = buff_len - sizeof(struct skw_spd_action_param);
	head_len = sizeof(struct skw_spd_action_param) +
		   sizeof(struct mdns_offload_filter_param);
	data_len = head_len;
	pos = buff + head_len;

	if (!g_mdns_offload_dev.offload_enabled) {
		skw_mdns_filter_head_init(buff, data_len, append, behavior,
					  rules);

		skw_hex_dump("mdns_filter:", buff, data_len, false);

		ret = skw_msg_xmit(priv_to_wiphy(skw), iface->id,
				   SKW_CMD_SET_SPD_ACTION, buff, data_len, NULL,
				   0);
		if (ret) {
			skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u,enable=0\n",
				iface->ndev->name, ret, data_len, append,
				rules);
		}
		goto exit;
	}

	list_for_each_entry_safe(pt_entry, tmp_pt,
				  &g_mdns_offload_dev.passthrough_list, list) {
		if (!strcmp(pt_entry->ifname, iface->ndev->name)) {
			record_len = strlen(pt_entry->qname) + 1;
			if (remain_len < record_len) {
				if (unlikely(!rules)) {
					skw_err("qname len=%u exceed length of buff\n",
						record_len);
					ret = -ENOMEM;
					break;
				}

				skw_mdns_filter_head_init(buff, data_len,
							  append, behavior,
							  rules);

				skw_hex_dump("mdns_filter:", buff, data_len,
					     false);

				ret = skw_msg_xmit(priv_to_wiphy(skw),
						   iface->id,
						   SKW_CMD_SET_SPD_ACTION, buff,
						   data_len, NULL, 0);
				if (ret) {
					skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u,behavior=%u\n",
						iface->ndev->name, ret,
						data_len, append, rules,
						behavior);
					rules = 0;
					break;
				}

				skw_dbg("[%s] send success,data_len=%u,append=%u,rules=%u,behavior=%u\n",
					iface->ndev->name, data_len, append,
					rules, behavior);

				append = true;
				rules = 0;
				remain_len =
					buff_len -
					sizeof(struct skw_spd_action_param);
				data_len = head_len;
				pos = buff + head_len;
			}

			rules++;
			strcpy(pos, pt_entry->qname);
			pos += strlen(pt_entry->qname) + 1;

			data_len = pos - buff;
			remain_len = buff_len - data_len;

			skw_dbg("[%s] qname rules=%d\n", pt_entry->ifname,
				rules);
		}
	}

	skw_mdns_filter_head_init(buff, data_len, append, behavior, rules);

	skw_hex_dump("mdns_filter:", buff, data_len, false);

	ret = skw_msg_xmit(priv_to_wiphy(skw), iface->id,
			   SKW_CMD_SET_SPD_ACTION, buff, data_len, NULL, 0);
	if (ret) {
		skw_err("[%s] send fail %d,data_len=%u,append=%u,rules=%u,behavior=%u\n",
			iface->ndev->name, ret, data_len, append, rules,
			behavior);
		goto exit;
	}

	skw_dbg("[%s] send success,data_len=%u,append=%u,rules=%u,behavior=%u\n",
		iface->ndev->name, data_len, append, rules, behavior);

exit:
	SKW_KFREE(buff);

	return ret;
}

int skw_mdns_iface_list_add(struct skw_iface *iface)
{
	struct iface_entry *entry = NULL;

	if (!iface || !iface->ndev)
		return -EINVAL;

	if (iface->ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_STATION) {
		skw_dbg("[%s] unsupport iface iftype=%d.\n", iface->ndev->name,
			iface->ndev->ieee80211_ptr->iftype);
		return 0;
	}

	if (!iface->skw->mdns_info.mdns_offload_support) {
		skw_dbg("[%s] unsupport mdns offload.\n", iface->ndev->name);
		return 0;
	}

	entry = SKW_ZALLOC(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	strncpy(entry->ifname, iface->ndev->name, IFNAMSIZ);
	entry->passthrough_behavior = MDNS_OFFLOAD_DROP_ALL;
	entry->iface = iface;
	INIT_LIST_HEAD(&entry->list);

	mutex_lock(&g_mdns_offload_dev.lock);

	list_add_tail(&entry->list, &g_mdns_offload_dev.iface_list);
	g_mdns_offload_dev.iface_count++;

	mutex_unlock(&g_mdns_offload_dev.lock);

	skw_dbg("Added iface %s\n", entry->ifname);

	return 0;
}

int skw_mdns_iface_list_del(struct skw_iface *iface)
{
	struct iface_entry *if_entry = NULL, *tmp_if = NULL;
	struct mdns_record *record = NULL, *tmp_record = NULL;
	struct passthrough_entry *pt_entry = NULL, *tmp_pt = NULL;
	bool fond = false;

	if (!iface)
		return -EINVAL;

	mutex_lock(&g_mdns_offload_dev.lock);

	list_for_each_entry_safe(if_entry, tmp_if,
				  &g_mdns_offload_dev.iface_list, list) {
		if (!strcmp(if_entry->ifname, iface->ndev->name)) {
			list_del_init(&if_entry->list);
			if_entry->iface = NULL;
			SKW_KFREE(if_entry);
			g_mdns_offload_dev.iface_count--;
			fond = true;

			break;
		}
	}

	list_for_each_entry_safe(record, tmp_record,
				  &g_mdns_offload_dev.record_list, list) {
		if (strcmp(record->ifname, iface->ndev->name))
			continue;

		list_del_init(&record->list);
		if (record->proto_data) {
			SKW_KFREE(record->proto_data->raw_packet);
			SKW_KFREE(record->proto_data->match_types);
			SKW_KFREE(record->proto_data->name_offsets);
			SKW_KFREE(record->proto_data);
		}
		SKW_KFREE(record);
	}

	list_for_each_entry_safe(pt_entry, tmp_pt,
				  &g_mdns_offload_dev.passthrough_list, list) {
		if (strcmp(pt_entry->ifname, iface->ndev->name))
			continue;

		list_del_init(&pt_entry->list);
		SKW_KFREE(pt_entry);
	}

	mutex_unlock(&g_mdns_offload_dev.lock);

	if (!fond) {
		skw_dbg("not fond iface %s, iface_count=%u\n",
			iface->ndev->name, g_mdns_offload_dev.iface_count);
	} else {
		skw_dbg("Del iface %s, iface_count=%u\n", iface->ndev->name,
			g_mdns_offload_dev.iface_count);
	}

	return 0;
}

static struct iface_entry *mdns_iface_list_find(char *ifname)
{
	struct iface_entry *if_entry = NULL, *tmp_if = NULL;
	bool fond = false;

	if (!ifname)
		return NULL;

	list_for_each_entry_safe(if_entry, tmp_if,
				  &g_mdns_offload_dev.iface_list, list) {
		if (!strcmp(if_entry->ifname, ifname)) {
			fond = true;
			break;
		}
	}

	if (!fond) {
		skw_dbg("not fond iface %s, iface_count=%u\n", ifname,
			g_mdns_offload_dev.iface_count);
		return NULL;
	}

	skw_dbg("find iface %s, iface_count=%u\n", ifname,
		g_mdns_offload_dev.iface_count);

	return if_entry;
}

static int mdns_stats_show(struct seq_file *m, void *v)
{
	struct mdns_record *record;
	struct passthrough_entry *pt_entry;
	struct iface_entry *if_entry;
	static const char * const behavior_str[] = { "FORWARD_ALL", "DROP_ALL",
				       "PASSTHROUGH_LIST" };

	mutex_lock(&g_mdns_offload_dev.lock);

	seq_printf(m, "SKWiFi mDNS Offload Driver v%s\n", SKW_VERSION);
	seq_puts(m, "===================================\n");

	seq_puts(m, "Offload Status:\n");
	seq_printf(m, "  Offload:        %s\n",
		   g_mdns_offload_dev.offload_enabled ? "ENABLED" : "DISABLED");
	seq_printf(m, "  Records:        %u/%d\n",
		   g_mdns_offload_dev.record_count, MDNS_MAX_RECORDS);
	seq_printf(m, "  Next Key:       %d\n",
		   g_mdns_offload_dev.next_record_key);

	if (!list_empty(&g_mdns_offload_dev.record_list)) {
		seq_puts(m, "\nOffloaded Records:\n");
		seq_printf(m, "%-8s %-12s %-8s %-8s\n", "Key", "Interface",
			   "Size", "Criteria");
		seq_printf(m, "%-8s %-12s %-8s %-8s\n", "---", "---------",
			   "----", "--------");

		list_for_each_entry(record, &g_mdns_offload_dev.record_list,
				     list) {
			seq_printf(m, "%-8d %-12s %-8zu %-8d\n",
				   record->record_key, record->ifname,
				   record->proto_data ?
					   record->proto_data->packet_len :
					   0,
				   record->proto_data ?
					   record->proto_data->criteria_count :
					   0);
		}
	}

	if (!list_empty(&g_mdns_offload_dev.passthrough_list)) {
		seq_printf(m, "\nPassthrough List (%u entries):\n",
			   g_mdns_offload_dev.passthrough_count);
		seq_printf(m, "%-12s %-40s\n", "Interface", "QName");
		seq_printf(m, "%-12s %-40s\n", "---------", "-----");

		list_for_each_entry(pt_entry, &g_mdns_offload_dev.passthrough_list, list) {
			seq_printf(m, "%-12s %-40.40s\n", pt_entry->ifname,
				   pt_entry->qname);
		}
	}

	if (!list_empty(&g_mdns_offload_dev.iface_list)) {
		seq_printf(m, "\niface List (%u entries):\n",
			   g_mdns_offload_dev.iface_count);
		seq_printf(m, "%-12s %-12s %-12s %-12s %-12s\n", "Interface",
			   "Behavior", "Record MAXn", "Record MAXs",
			   "Filter size");
		seq_printf(m, "%-12s %-12s %-12s %-12s %-12s\n", "---------",
			   "---------", "---------", "---------", "---------");

		list_for_each_entry(if_entry, &g_mdns_offload_dev.iface_list,
				     list) {
			seq_printf(m, "%-12s %-12s %-12u %-12u %-12u\n",
				if_entry->ifname,
				if_entry->passthrough_behavior < 3 ?
					behavior_str
						[if_entry->passthrough_behavior] :
					"UNKNOWN",
				if_entry->iface->skw->mdns_info
					.mdns_rsp_matchrule_maxunm,
				if_entry->iface->skw->mdns_info
					.mdns_rsp_matchrule_buf_maxsize,
				if_entry->iface->skw->mdns_info
					.mdns_filter_qname_buf_maxsize);
		}
	}

	mutex_unlock(&g_mdns_offload_dev.lock);

	seq_puts(m, "\nMemory Usage:\n");
	seq_printf(m, "  iface: %u, Records: %u, Passthrough: %u\n",
		   g_mdns_offload_dev.iface_count,
		   g_mdns_offload_dev.record_count,
		   g_mdns_offload_dev.passthrough_count);

	return 0;
}

static int mdns_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdns_stats_show, NULL);
}

int skw_mdns_update_iface(struct skw_iface *iface)
{
	struct iface_entry *if_entry = NULL, *tmp_if = NULL;
	int ret = 0;

	if (iface->ndev->ieee80211_ptr->iftype != NL80211_IFTYPE_STATION) {
		skw_detail("[%s] unsupport iface iftype=%d.\n", iface->ndev->name,
			iface->ndev->ieee80211_ptr->iftype);
		return 0;
	}

	if (!iface->skw->mdns_info.mdns_offload_support) {
		skw_detail("[%s] unsupport mdns offload.\n", iface->ndev->name);
		return 0;
	}

	mutex_lock(&g_mdns_offload_dev.lock);

	list_for_each_entry_safe(if_entry, tmp_if,
				&g_mdns_offload_dev.iface_list, list) {
		if (!strcmp(iface->ndev->name, if_entry->ifname)) {
			ret = skw_mdns_rsp_params(if_entry->iface);
			if (ret) {
				skw_err("[%s]update record to fw fail %d\n",
					if_entry->ifname, ret);
			}

			ret = skw_mdns_filter_params(if_entry->iface,
				if_entry->passthrough_behavior);
			if (ret) {
				skw_err("[%s]update passthrough to fw fail %d\n",
					if_entry->ifname, ret);
			}
		}
	}

	mutex_unlock(&g_mdns_offload_dev.lock);

	return ret;
}

static int skw_mdns_update_all_iface(void)
{
	struct iface_entry *if_entry = NULL, *tmp_if = NULL;
	int ret = 0;

	list_for_each_entry_safe(if_entry, tmp_if,
				&g_mdns_offload_dev.iface_list, list) {
		ret = skw_mdns_rsp_params(if_entry->iface);
		if (ret) {
			skw_err("[%s]update record to fw fail %d\n",
				if_entry->ifname, ret);
			continue;
		}

		ret = skw_mdns_filter_params(if_entry->iface,
					     if_entry->passthrough_behavior);
		if (ret) {
			skw_err("[%s]update passthrough to fw fail %d\n",
				if_entry->ifname, ret);
		}
	}

	return ret;
}

static ssize_t mdns_control_write(struct file *file, const char __user *buffer,
				  size_t count, loff_t *ppos)
{
	char *cmd_buff = NULL, *cmd = NULL;
	char *token = NULL;
	struct mdns_record *record = NULL, *tmp_record = NULL;
	struct passthrough_entry *pt_entry = NULL, *tmp_pt = NULL;
	struct iface_entry *if_entry = NULL, *tmp_if = NULL;
	int ret = count;
	long key = 0;

	if (!count || count >= 256)
		return -EINVAL;

	cmd_buff = SKW_ZALLOC(count + 1, GFP_KERNEL);
	if (!cmd_buff)
		return -ENOMEM;

	if (copy_from_user(cmd_buff, buffer, count)) {
		SKW_KFREE(cmd_buff);
		return -EFAULT;
	}

	cmd = cmd_buff;

	cmd[count] = '\0';

	if (cmd[count - 1] == '\n')
		cmd[count - 1] = '\0';

	token = strsep(&cmd, " ");

	if (!token) {
		SKW_KFREE(cmd_buff);
		return -EINVAL;
	}

	mutex_lock(&g_mdns_offload_dev.lock);

	if (strcmp(token, "enable") == 0) {
		g_mdns_offload_dev.offload_enabled = true;
		skw_info("Offload enabled via control\n");
	} else if (strcmp(token, "disable") == 0) {
		g_mdns_offload_dev.offload_enabled = false;
		skw_info("Offload disabled via control\n");
	} else if (strcmp(token, "reset") == 0) {
		token = strsep(&cmd, " ");
		if (token && strcmp(token, "all") == 0) {
			list_for_each_entry_safe(record, tmp_record,
				&g_mdns_offload_dev.record_list, list) {
				list_del_init(&record->list);
				if (record->proto_data) {
					SKW_KFREE(record->proto_data->raw_packet);
					SKW_KFREE(record->proto_data->match_types);
					SKW_KFREE(record->proto_data->name_offsets);
					SKW_KFREE(record->proto_data);
				}
				SKW_KFREE(record);
			}

			list_for_each_entry_safe(pt_entry, tmp_pt,
				&g_mdns_offload_dev.passthrough_list, list) {
				list_del_init(&pt_entry->list);
				SKW_KFREE(pt_entry);
			}

			list_for_each_entry_safe(if_entry, tmp_if,
				&g_mdns_offload_dev.iface_list, list) {
				if_entry->passthrough_behavior =
					MDNS_OFFLOAD_DROP_ALL;
			}

			g_mdns_offload_dev.record_count = 0;
			g_mdns_offload_dev.passthrough_count = 0;
			g_mdns_offload_dev.next_record_key = 1;

			skw_dbg("Reset all via control\n");
		} else if (token) {
			ret = kstrtol(token, 10, &key);
			if (ret) {
				ret = -EINVAL;
				return ret;
			}

			list_for_each_entry(record, &g_mdns_offload_dev.record_list, list) {
				if (record->record_key == key) {
					skw_dbg("Reset hits for record %ld\n",
						key);
					break;
				}
			}
		}
	} else if (strcmp(token, "reset_misses") == 0) {
		skw_dbg("Reset miss counter\n");
	}

	ret = skw_mdns_update_all_iface();
	if (ret)
		skw_err("update all iface fail %d\n", ret);

	ret = count;

	mutex_unlock(&g_mdns_offload_dev.lock);

	SKW_KFREE(cmd_buff);

	return ret;
}

static int mdns_control_show(struct seq_file *m, void *v)
{
	seq_puts(m, "mDNS Offload Control Interface\n");
	seq_puts(m, "===============================\n");
	seq_puts(m, "Commands:\n");
	seq_puts(m, "  enable          - Enable mDNS offload\n");
	seq_puts(m, "  disable         - Disable mDNS offload\n");
	seq_puts(m, "  reset all       - Reset all records and counters\n");
	seq_puts(m, "  reset <key>     - Reset hit counter for a record\n");
	seq_puts(m, "  reset_misses    - Reset miss counter\n");

	return 0;
}

static int mdns_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdns_control_show, NULL);
}

static bool mdns_check_record_key_ok(int record_key)
{
	struct mdns_record *record = NULL;

	list_for_each_entry(record, &g_mdns_offload_dev.record_list, list) {
		if (record->record_key == record_key)
			return false;
	}

	return true;
}

static bool mdns_check_record_buffer(struct skw_iface *iface,
				     u16 criteria_count, u16 packet_size)
{
	struct mdns_record *record = NULL;
	u16 record_count = 0, record_len = 0;

	skw_detail("mdns_info:%u,%u,%u,%u\n",
		   iface->skw->mdns_info.mdns_offload_support,
		   iface->skw->mdns_info.mdns_rsp_matchrule_maxunm,
		   iface->skw->mdns_info.mdns_rsp_matchrule_buf_maxsize,
		   iface->skw->mdns_info.mdns_filter_qname_buf_maxsize);

	record_count++;
	record_len += 1 + sizeof(struct match_rule) * criteria_count + 2 +
		      packet_size;

	if (record_count > iface->skw->mdns_info.mdns_rsp_matchrule_maxunm) {
		skw_warn("[%s] reach rules max num=%u,size=%u\n",
			 iface->ndev->name, record_count, record_len);
		return false;
	}

	if (record_len > iface->skw->mdns_info.mdns_rsp_matchrule_buf_maxsize) {
		skw_warn("[%s] reach rules max size=%u,num=%u\n",
			 iface->ndev->name, record_len, record_count);
		return false;
	}

	list_for_each_entry(record, &g_mdns_offload_dev.record_list, list) {
		if (!strcmp(iface->ndev->name, record->ifname)) {
			record_count++;
			record_len +=
				1 +
				sizeof(struct match_rule) *
					record->proto_data->criteria_count +
				2 + record->proto_data->packet_len;

			if (record_count >
			    iface->skw->mdns_info.mdns_rsp_matchrule_maxunm) {
				skw_warn("[%s] reach rules max num=%u,size=%u\n",
					record->ifname, record_count,
					record_len);
				return false;
			}

			if (record_len >
			    iface->skw->mdns_info
				    .mdns_rsp_matchrule_buf_maxsize) {
				skw_warn("[%s] reach rules max size=%u,num=%u\n",
					record->ifname, record_len,
					record_count);
				return false;
			}
		}
	}

	return true;
}

static bool mdns_check_filter_buffer(struct skw_iface *iface, char *qname)
{
	struct passthrough_entry *pt_entry = NULL;
	u16 pt_count = 0, pt_len = 0;

	skw_detail("mdns_info:%u,%u,%u,%u\n",
		   iface->skw->mdns_info.mdns_offload_support,
		   iface->skw->mdns_info.mdns_rsp_matchrule_maxunm,
		   iface->skw->mdns_info.mdns_rsp_matchrule_buf_maxsize,
		   iface->skw->mdns_info.mdns_filter_qname_buf_maxsize);

	pt_count++;
	pt_len += strlen(qname) + 1;
	if (pt_len > iface->skw->mdns_info.mdns_filter_qname_buf_maxsize) {
		skw_warn("[%s] reach filter max size=%u,cnt=%u\n",
			 iface->ndev->name, pt_len, pt_count);
		return false;
	}

	list_for_each_entry(pt_entry, &g_mdns_offload_dev.passthrough_list,
			    list) {
		if (!strcmp(iface->ndev->name, pt_entry->ifname)) {
			pt_count++;
			pt_len += strlen(pt_entry->qname) + 1;

			if (pt_len > iface->skw->mdns_info
					     .mdns_filter_qname_buf_maxsize) {
				skw_warn("[%s] reach filter max size=%u,cnt=%u\n",
					pt_entry->ifname, pt_len, pt_count);
				return false;
			}
		}
	}

	return true;
}

static ssize_t mdns_config_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *ppos)
{
	char *cmd_buff = NULL, *cmd = NULL, *token = NULL;
	int ret = count;
	long packet_size = 0, criteria_count = 0;
	char *iface = NULL, *size_str = NULL, *criteria_str = NULL;
	struct mdns_record *record = NULL, *tmp = NULL;
	char *key_str = NULL, *qname = NULL, *behavior_str = NULL;
	int remain_len = 0;
	long key = 0;
	bool found = false;
	struct passthrough_entry *entry = NULL, *pass_tmp = NULL;
	char *rawdata = NULL;
	struct iface_entry *if_entry = NULL;
	//u16 pre_behavior = 0;

	if (!count || count >= MDNS_MAX_PACKET_SIZE)
		return -EINVAL;

	cmd_buff = SKW_ZALLOC(count + 1, GFP_KERNEL);
	if (!cmd_buff)
		return -ENOMEM;

	if (copy_from_user(cmd_buff, buffer, count)) {
		SKW_KFREE(cmd_buff);
		return -EFAULT;
	}

	cmd = cmd_buff;
	cmd[count] = '\0';

	if (cmd[count - 1] == '\n')
		cmd[count - 1] = '\0';

	mutex_lock(&g_mdns_offload_dev.lock);

	token = strsep(&cmd, " ");
	if (!token) {
		ret = -EINVAL;
		goto out;
	}

	if (strcmp(token, "add") == 0) {
		iface = strsep(&cmd, " ");
		size_str = strsep(&cmd, " ");
		criteria_str = strsep(&cmd, " ");
		rawdata = cmd;

		if (!iface || !size_str || !criteria_str) {
			ret = -EINVAL;
			goto out;
		}

		if (g_mdns_offload_dev.record_count >= MDNS_MAX_RECORDS) {
			skw_err("Max records reached %d\n",
				g_mdns_offload_dev.record_count);
			ret = -ENOSPC;
			goto out;
		}

		ret = kstrtol(size_str, 10, &packet_size);
		if (ret) {
			ret = -EINVAL;
			goto out;
		}
		ret = kstrtol(criteria_str, 10, &criteria_count);
		if (ret) {
			ret = -EINVAL;
			goto out;
		}

		if (packet_size <= 0 || packet_size > MDNS_MAX_PACKET_SIZE) {
			skw_err("Packet too large: %ld\n", packet_size);
			ret = -EINVAL;
			goto out;
		}

		if (criteria_count <= 0 ||
		    criteria_count > MDNS_MAX_PACKET_SIZE / (2 * sizeof(int))) {
			skw_err("error criteria_count: %ld\n", criteria_count);
			ret = -EINVAL;
			goto out;
		}

		remain_len = count - (rawdata - cmd_buff);
		if (remain_len <
		    packet_size + criteria_count * 2 * sizeof(int)) {
			skw_err("error param remain_len=%d, criteria_count=%ld, packet_size=%ld\n",
				remain_len, criteria_count, packet_size);
			ret = -EINVAL;
			goto out;
		}

		if_entry = mdns_iface_list_find(iface);
		if (!if_entry) {
			skw_err("[%s]error iface not find\n", iface);
			ret = -EINVAL;
			goto out;
		}

		if (!mdns_check_record_buffer(if_entry->iface, criteria_count,
					      packet_size)) {
			ret = -ENOMEM;
			goto out;
		}

		record = SKW_ZALLOC(sizeof(*record), GFP_KERNEL);
		if (!record) {
			ret = -ENOMEM;
			goto out;
		}

		while (!mdns_check_record_key_ok(g_mdns_offload_dev.next_record_key)) {
			if (++g_mdns_offload_dev.next_record_key <= 0)
				g_mdns_offload_dev.next_record_key = 1;
		}

		record->record_key = g_mdns_offload_dev.next_record_key++;
		strncpy(record->ifname, iface, IFNAMSIZ);

		record->proto_data = SKW_ZALLOC(sizeof(struct mdns_protocol_data), GFP_KERNEL);
		if (!record->proto_data) {
			SKW_KFREE(record);
			ret = -ENOMEM;
			goto out;
		}

		record->proto_data->packet_len = packet_size;
		record->proto_data->criteria_count = criteria_count;
		record->proto_data->record_key = record->record_key;

		record->proto_data->raw_packet =
			kmalloc(packet_size, GFP_KERNEL);
		if (!record->proto_data->raw_packet) {
			SKW_KFREE(record->proto_data);
			SKW_KFREE(record);
			ret = -ENOMEM;
			goto out;
		}

		record->proto_data->match_types =
			kmalloc_array(criteria_count, sizeof(int), GFP_KERNEL);
		record->proto_data->name_offsets =
			kmalloc_array(criteria_count, sizeof(int), GFP_KERNEL);
		if (!record->proto_data->match_types ||
		    !record->proto_data->name_offsets) {
			SKW_KFREE(record->proto_data->raw_packet);
			SKW_KFREE(record->proto_data->match_types);
			SKW_KFREE(record->proto_data->name_offsets);
			SKW_KFREE(record->proto_data);
			SKW_KFREE(record);
			ret = -ENOMEM;
			goto out;
		}

		memcpy(record->proto_data->match_types, rawdata,
		       criteria_count * sizeof(int));
		rawdata += criteria_count * sizeof(int);
		memcpy(record->proto_data->name_offsets, rawdata,
		       criteria_count * sizeof(int));
		rawdata += criteria_count * sizeof(int);
		memcpy(record->proto_data->raw_packet, rawdata, packet_size);

		INIT_LIST_HEAD(&record->list);
		list_add_tail(&record->list, &g_mdns_offload_dev.record_list);
		g_mdns_offload_dev.record_count++;

		ret = skw_mdns_rsp_params(if_entry->iface);
		if (ret) {
			skw_err("[%s]add record to fw fail %d\n", iface, ret);
			//g_mdns_offload_dev.record_count--;
			//list_del_init(&record->list);
			//SKW_KFREE(record->proto_data->raw_packet);
			//SKW_KFREE(record->proto_data->match_types);
			//SKW_KFREE(record->proto_data->name_offsets);
			//SKW_KFREE(record->proto_data);
			//SKW_KFREE(record);
			//goto out;
		}

		ret = record->record_key;

		skw_dbg("[%s]Added record %d,criteria_count=%ld,packet_size=%ld\n",
			iface, record->record_key, criteria_count, packet_size);
		skw_hex_dump("match_types", record->proto_data->match_types,
			     criteria_count * sizeof(int), false);
		skw_hex_dump("name_offsets", record->proto_data->name_offsets,
			     criteria_count * sizeof(int), false);
		skw_hex_dump("raw_packet", record->proto_data->raw_packet,
			     packet_size, false);
	} else if (strcmp(token, "remove") == 0) {
		key_str = strsep(&cmd, " ");
		if (!key_str) {
			ret = -EINVAL;
			goto out;
		}

		ret = kstrtol(key_str, 10, &key);
		if (ret) {
			ret = -EINVAL;
			goto out;
		}
		found = false;

		list_for_each_entry_safe(record,
					tmp, &g_mdns_offload_dev.record_list, list) {
			if (record->record_key == key) {
				found = true;
				break;
			}
		}

		if (!found) {
			skw_warn("Record %ld not found\n", key);
			goto out;
		}

		list_del_init(&record->list);
		g_mdns_offload_dev.record_count--;

		if_entry = mdns_iface_list_find(record->ifname);
		if (!if_entry) {
			skw_err("[%s]error iface not find\n", record->ifname);
		} else {
			ret = skw_mdns_rsp_params(if_entry->iface);
			if (ret) {
				//list_add_tail(&record->list, &g_mdns_offload_dev.record_list);
				//g_mdns_offload_dev.record_count++;
				skw_err("remove record to fw fail %d\n", ret);
				//goto out;
			}
		}

		if (record->proto_data) {
			SKW_KFREE(record->proto_data->raw_packet);
			SKW_KFREE(record->proto_data->match_types);
			SKW_KFREE(record->proto_data->name_offsets);
			SKW_KFREE(record->proto_data);
		}
		SKW_KFREE(record);

		ret = count;
		skw_dbg("Removed record %ld\n", key);

	} else if (strcmp(token, "passthrough") == 0) {
		token = strsep(&cmd, " ");
		if (!token) {
			ret = -EINVAL;
			goto out;
		}

		if (strcmp(token, "add") == 0) {
			iface = strsep(&cmd, " ");
			qname = strsep(&cmd, " ");

			if (!iface || !qname ||
			    strlen(qname) >= MDNS_MAX_QNAME_LEN) {
				ret = -EINVAL;
				goto out;
			}

			if_entry = mdns_iface_list_find(iface);
			if (!if_entry) {
				skw_err("[%s]error iface not find\n", iface);
				ret = -EINVAL;
				goto out;
			}

			if (!mdns_check_filter_buffer(if_entry->iface, qname)) {
				ret = -ENOMEM;
				goto out;
			}

			entry = SKW_ZALLOC(sizeof(*entry) + strlen(qname) + 1,
					   GFP_KERNEL);
			if (!entry) {
				ret = -ENOMEM;
				goto out;
			}

			strncpy(entry->ifname, iface, IFNAMSIZ);
			strcpy(entry->qname, qname);

			INIT_LIST_HEAD(&entry->list);
			list_add_tail(&entry->list,
				      &g_mdns_offload_dev.passthrough_list);
			g_mdns_offload_dev.passthrough_count++;

			ret = skw_mdns_filter_params(if_entry->iface,
				if_entry->passthrough_behavior);
			if (ret) {
				//g_mdns_offload_dev.passthrough_count--;
				//list_del_init(&entry->list);
				//SKW_KFREE(entry);
				skw_err("[%s]add passthrough to fw fail %d\n",
					iface, ret);
				//goto out;
			}

			ret = count;
			skw_dbg("[%s]Added passthrough count=%u\n", iface,
				g_mdns_offload_dev.passthrough_count);

		} else if (strcmp(token, "remove") == 0) {
			iface = strsep(&cmd, " ");
			qname = strsep(&cmd, " ");

			if (!iface || !qname) {
				ret = -EINVAL;
				goto out;
			}

			if_entry = mdns_iface_list_find(iface);
			if (!if_entry) {
				skw_err("[%s]error iface not find\n", iface);
				ret = -EINVAL;
				goto out;
			}

			found = false;

			list_for_each_entry_safe(entry, pass_tmp,
				&g_mdns_offload_dev.passthrough_list, list) {
				if (strcmp(entry->ifname, iface) == 0 &&
				    strcmp(entry->qname, qname) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				skw_warn("[%s]Passthrough entry not found count=%u\n",
					iface,
					g_mdns_offload_dev.passthrough_count);
				goto out;
			}

			list_del_init(&entry->list);
			g_mdns_offload_dev.passthrough_count--;

			ret = skw_mdns_filter_params(if_entry->iface,
				if_entry->passthrough_behavior);
			if (ret) {
				//list_add_tail(&entry->list, &g_mdns_offload_dev.passthrough_list);
				//g_mdns_offload_dev.passthrough_count++;
				skw_err("[%s]Removed passthrough to fw fail %d\n",
					iface, ret);
				//goto out;
			}

			SKW_KFREE(entry);
			ret = count;
			skw_dbg("[%s]Removed passthrough count=%u\n", iface,
				g_mdns_offload_dev.passthrough_count);
		}

	} else if (strcmp(token, "behavior") == 0) {
		iface = strsep(&cmd, " ");
		behavior_str = strsep(&cmd, " ");

		if (!behavior_str) {
			ret = -EINVAL;
			goto out;
		}

		if_entry = mdns_iface_list_find(iface);
		if (!if_entry) {
			skw_err("error iface=%s\n", iface);
			ret = -EINVAL;
			goto out;
		}
		//pre_behavior = if_entry->passthrough_behavior;
		if (strcmp(behavior_str, "forward_all") == 0) {
			if_entry->passthrough_behavior =
				MDNS_OFFLOAD_FORWARD_ALL;
		} else if (strcmp(behavior_str, "drop_all") == 0) {
			if_entry->passthrough_behavior = MDNS_OFFLOAD_DROP_ALL;
		} else if (strcmp(behavior_str, "passthrough_list") == 0) {
			if_entry->passthrough_behavior =
				MDNS_OFFLOAD_PASSTHROUGH_LIST;
		} else {
			skw_err("[%s]unknown behavior %s\n", iface,
				behavior_str);
			ret = -EINVAL;
			goto out;
		}

		ret = skw_mdns_filter_params(if_entry->iface,
					     if_entry->passthrough_behavior);
		if (ret) {
			//if_entry->passthrough_behavior = pre_behavior;
			skw_err("[%s]Set behavior %s to fw fail %d\n", iface,
				behavior_str, ret);
			//goto out;
		}

		ret = count;
		skw_dbg("[%s]Set behavior to %s\n", iface, behavior_str);

	} else {
		skw_warn("Unknown config command: %s\n", token);
		ret = -EINVAL;
	}

out:
	mutex_unlock(&g_mdns_offload_dev.lock);
	SKW_KFREE(cmd_buff);

	return ret;
}

static int mdns_config_show(struct seq_file *m, void *v)
{
	mutex_lock(&g_mdns_offload_dev.lock);

	seq_puts(m, "mDNS Offload Configuration Interface\n");
	seq_puts(m, "====================================\n\n");

	seq_puts(m, "Current Limits:\n");
	seq_printf(m, "  Max Records:      %d\n", MDNS_MAX_RECORDS);
	seq_printf(m, "  Max Packet Size:  %d bytes\n", MDNS_MAX_PACKET_SIZE);
	seq_printf(m, "  Max QName Length: %d chars\n", MDNS_MAX_QNAME_LEN);

	seq_puts(m, "\nConfiguration Commands:\n");
	seq_printf(m,
		"  add <iface> <size> <criteria> <match criter><rawdata> - Add offload record\n");
	seq_puts(m, "  remove <key>                      - Remove record\n");
	seq_printf(m,
		"  passthrough add <iface> <qname>   - Add to passthrough list\n");
	seq_printf(m,
		"  passthrough remove <iface> <qname>- Remove from passthrough list\n");
	seq_printf(m,
		   "  behavior <iface> <mode>           - Set behavior mode\n");
	seq_puts(m, "    Modes: forward_all, drop_all, passthrough_list\n");

	mutex_unlock(&g_mdns_offload_dev.lock);
	return 0;
}

static int mdns_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdns_config_show, NULL);
}

#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE
static const struct proc_ops mdns_control_fops = {
	.proc_open = mdns_control_open,
	.proc_read = seq_read,
	.proc_write = mdns_control_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops mdns_config_fops = {
	.proc_open = mdns_config_open,
	.proc_read = seq_read,
	.proc_write = mdns_config_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops mdns_stats_fops = {
	.proc_open = mdns_stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations mdns_control_fops = {
	.owner = THIS_MODULE,
	.open = mdns_control_open,
	.read = seq_read,
	.write = mdns_control_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mdns_config_fops = {
	.owner = THIS_MODULE,
	.open = mdns_config_open,
	.read = seq_read,
	.write = mdns_config_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mdns_stats_fops = {
	.owner = THIS_MODULE,
	.open = mdns_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int skw_mdns_offload_init(struct proc_dir_entry *skw_proc_root,
			  char *parent_name)
{
	memset(&g_mdns_offload_dev, 0, sizeof(g_mdns_offload_dev));
	mutex_init(&g_mdns_offload_dev.lock);
	INIT_LIST_HEAD(&g_mdns_offload_dev.record_list);
	INIT_LIST_HEAD(&g_mdns_offload_dev.passthrough_list);
	INIT_LIST_HEAD(&g_mdns_offload_dev.iface_list);
	g_mdns_offload_dev.next_record_key = 1;
	g_mdns_offload_dev.offload_enabled = false;

	g_mdns_offload_dev.proc_main =
		proc_mkdir(MDNS_OFFLOAD_PROC_NAME, skw_proc_root);

	g_mdns_offload_dev.proc_control =
		skw_procfs_file(g_mdns_offload_dev.proc_main, "control", 0644,
				&mdns_control_fops, NULL);

	g_mdns_offload_dev.proc_config =
		skw_procfs_file(g_mdns_offload_dev.proc_main, "config", 0644,
				&mdns_config_fops, NULL);

	g_mdns_offload_dev.proc_stats =
		skw_procfs_file(g_mdns_offload_dev.proc_main, "stats", 0444,
				&mdns_stats_fops, NULL);

	pr_info("[%s] Created proc nodes /proc/%s/%s\n", SKW_TAG_INFO,
		parent_name, MDNS_OFFLOAD_PROC_NAME);

	return 0;
}

int skw_mdns_offload_deinit(void)
{
	struct mdns_record *record, *tmp_record;
	struct passthrough_entry *pt_entry, *tmp_pt;
	struct iface_entry *if_entry, *tmp_if;

	if (g_mdns_offload_dev.proc_config)
		proc_remove(g_mdns_offload_dev.proc_config);

	if (g_mdns_offload_dev.proc_control)
		proc_remove(g_mdns_offload_dev.proc_control);

	if (g_mdns_offload_dev.proc_main)
		proc_remove(g_mdns_offload_dev.proc_main);

	mutex_lock(&g_mdns_offload_dev.lock);

	list_for_each_entry_safe(record, tmp_record,
				&g_mdns_offload_dev.record_list, list) {
		list_del_init(&record->list);
		if (record->proto_data) {
			SKW_KFREE(record->proto_data->raw_packet);
			SKW_KFREE(record->proto_data->match_types);
			SKW_KFREE(record->proto_data->name_offsets);
			SKW_KFREE(record->proto_data);
		}
		SKW_KFREE(record);
	}

	list_for_each_entry_safe(pt_entry, tmp_pt,
				&g_mdns_offload_dev.passthrough_list, list) {
		list_del_init(&pt_entry->list);
		SKW_KFREE(pt_entry);
	}

	list_for_each_entry_safe(if_entry, tmp_if,
				&g_mdns_offload_dev.iface_list, list) {
		list_del_init(&if_entry->list);
		SKW_KFREE(if_entry);
	}

	mutex_unlock(&g_mdns_offload_dev.lock);

	return 0;
}
