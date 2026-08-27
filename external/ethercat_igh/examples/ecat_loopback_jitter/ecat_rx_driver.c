// SPDX-License-Identifier: GPL-2.0
/*
 * EtherCAT Slave Frame Jitter Monitor
 *
 * Registers a raw packet handler for EtherType 0x88A4 and measures
 * inter-frame arrival jitter on received EtherCAT frames.
 *
 * In the loopback test setup (GMAC0 -> GMAC1), this captures the
 * wire-level timing without needing a hardware ESC.
 *
 * Statistics exported via debugfs: /sys/kernel/debug/ec_perf_slave/stats
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/******************************************************************************
 * Jitter statistics
 ******************************************************************************/

#define WINDOW_SIZE	20000
#define CYCLE_NS	500000

static struct {
	u64 frame_count;
	u64 prev_ts_ns;
	u64 min_jitter;
	u64 max_jitter;
	u64 g_max_jitter;
	u64 total_jitter;
	u32 count;
} stats;

/******************************************************************************
 * Packet callback
 ******************************************************************************/

static int ecat_rcv(struct sk_buff *skb, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev)
{
	u64 now_ns;
	s64 delta_ns;
	u64 jitter_ns;

	now_ns = ktime_get_ns();

	if (!stats.prev_ts_ns) {
		stats.prev_ts_ns = now_ns;
		goto done;
	}

	delta_ns = now_ns - stats.prev_ts_ns;
	stats.prev_ts_ns = now_ns;

	/* jitter = |inter-frame gap - expected 500 us| */
	jitter_ns = (delta_ns > (s64)CYCLE_NS)
		? (u64)(delta_ns - CYCLE_NS)
		: (u64)(CYCLE_NS - delta_ns);

	stats.frame_count++;

	if (jitter_ns < stats.min_jitter)
		stats.min_jitter = jitter_ns;
	if (jitter_ns > stats.max_jitter)
		stats.max_jitter = jitter_ns;
	if (jitter_ns > stats.g_max_jitter)
		stats.g_max_jitter = jitter_ns;

	stats.total_jitter += jitter_ns;
	stats.count++;

	if (stats.count >= WINDOW_SIZE) {
		u64 avg = stats.total_jitter / stats.count;
		pr_info("ecat_rx_driver: frame jitter (ns)"
			" min=%llu max=%llu avg=%llu gmax=%llu"
			" frames=%llu\n",
			stats.min_jitter, stats.max_jitter,
			avg, stats.g_max_jitter,
			stats.frame_count);
		stats.min_jitter = U64_MAX;
		stats.max_jitter = 0;
		stats.total_jitter = 0;
		stats.count = 0;
	}

done:
	consume_skb(skb);
	return NET_RX_SUCCESS;
}

static struct packet_type ecat_pt = {
	.type = htons(0x88A4),
	.dev  = NULL, /* all devices */
	.func = ecat_rcv,
};

/******************************************************************************
 * debugfs
 ******************************************************************************/

static struct dentry *debugfs_dir;

static void reset_stats(void)
{
	stats.frame_count = 0;
	stats.prev_ts_ns = 0;
	stats.min_jitter = U64_MAX;
	stats.max_jitter = 0;
	stats.g_max_jitter = 0;
	stats.total_jitter = 0;
	stats.count = 0;
}

static int stats_show(struct seq_file *m, void *v)
{
	u64 avg = stats.count ? stats.total_jitter / stats.count : 0;
	seq_printf(m, "frame_count: %llu\n", stats.frame_count);
	seq_printf(m, "window_samples: %u\n", stats.count);
	seq_printf(m, "min_jitter: %llu ns\n",
		   stats.min_jitter == U64_MAX ? 0ULL : stats.min_jitter);
	seq_printf(m, "max_jitter: %llu ns\n", stats.max_jitter);
	seq_printf(m, "avg_jitter: %llu ns\n", avg);
	seq_printf(m, "g_max_jitter: %llu ns\n", stats.g_max_jitter);
	return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stats_show, NULL);
}

static const struct file_operations stats_fops = {
	.owner		= THIS_MODULE,
	.open		= stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t reset_write(struct file *file, const char __user *ubuf,
			   size_t len, loff_t *ppos)
{
	reset_stats();
	return len;
}

static const struct file_operations reset_fops = {
	.owner		= THIS_MODULE,
	.write		= reset_write,
};

/******************************************************************************
 * Module init / exit
 ******************************************************************************/

static int __init ecat_rx_driver_init(void)
{
	/* register raw packet handler for EtherCAT EtherType (0x88A4) */
	dev_add_pack(&ecat_pt);

	/* debugfs */
	debugfs_dir = debugfs_create_dir("ethercat_slave", NULL);
	debugfs_create_file("stats", 0444, debugfs_dir, NULL, &stats_fops);
	debugfs_create_file("reset", 0200, debugfs_dir, NULL, &reset_fops);

	reset_stats();

	pr_info("ecat_rx_driver: loaded, monitoring EtherCAT frames\n");
	return 0;
}

static void __exit ecat_rx_driver_exit(void)
{
	dev_remove_pack(&ecat_pt);
	debugfs_remove_recursive(debugfs_dir);

	pr_info("ecat_rx_driver: unloaded, total frames=%llu\n",
		stats.frame_count);
}

module_init(ecat_rx_driver_init);
module_exit(ecat_rx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rockchip");
MODULE_DESCRIPTION("EtherCAT Slave Frame Jitter Monitor");
MODULE_VERSION("1.0");
