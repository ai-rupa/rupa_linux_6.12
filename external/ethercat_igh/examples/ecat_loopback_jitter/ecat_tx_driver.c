// SPDX-License-Identifier: GPL-2.0
/*
 * EtherCAT Master Kernel Performance Test
 *
 * Measures cycle jitter by running the full EtherCAT master cycle
 * (send -> hardware -> receive -> process) from an hrtimer callback
 * in kernel space, bypassing userspace entirely.
 *
 * This provides the "pure kernel" baseline for comparison with the
 * userspace rkros_ethercat_perf demo.
 *
 * PDO configuration matches the userspace version (SM1 -> 0x3101:1, 32bit)
 * for direct comparison.
 *
 * Statistics exported via debugfs: /sys/kernel/debug/ec_perf_master/stats
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched/rt.h>

#include "ecrt.h"

/******************************************************************************
 * Configuration
 ******************************************************************************/

#define CYCLE_NS	500000	/* 500 us = 2 kHz */
#define WINDOW_SIZE	20000
#define MASTER_IDX	0
#define SLAVE_ALIAS	0
#define SLAVE_POS	0
#define VENDOR_ID	0xFFFFFFFF
#define PRODUCT_CODE	0xFFFFFFFF

/******************************************************************************
 * PDO layout (matches userspace rkros_ethercat_perf)
 ******************************************************************************/

static ec_pdo_entry_info_t channel1[] = {
	{0x3101, 1, 32}, /* dummy_channel1: 32-bit output */
};

static ec_pdo_info_t pdos[] = {
	{0x1A00, 1, channel1},
};

static ec_sync_info_t syncs[] = {
	{1, EC_DIR_OUTPUT, 1, pdos, EC_WD_DISABLE},
	{0xff},
};

/******************************************************************************
 * Master state
 ******************************************************************************/
static ec_master_t *master;
static ec_domain_t *domain;
static ec_slave_config_t *sc;
static uint8_t *domain_data;
static unsigned int pd_offset;

static struct hrtimer cycle_timer;
static struct task_struct *cycle_task;
static wait_queue_head_t cycle_wq;
static atomic_t cycle_pending = ATOMIC_INIT(0);
static uint32_t toggle_val;
static int timer_cpu = -1;

module_param(timer_cpu, int, 0644);
MODULE_PARM_DESC(timer_cpu,
	"Target CPU for hrtimer callback (-1: default migration-enabled)");

/******************************************************************************
 * Jitter statistics
 ******************************************************************************/

static struct {
	u64 min_jitter;
	u64 max_jitter;
	u64 g_max_jitter;
	u64 total_jitter;
	u32 count;
	u32 last_cpu;
	u64 cpu_hits[NR_CPUS];
} stats;

/******************************************************************************
 * debugfs
 ******************************************************************************/

static struct dentry *debugfs_dir;

static void reset_stats(void)
{
	stats.min_jitter = U64_MAX;
	stats.max_jitter = 0;
	stats.g_max_jitter = 0;
	stats.total_jitter = 0;
	stats.count = 0;
	stats.last_cpu = U32_MAX;
	memset(stats.cpu_hits, 0, sizeof(stats.cpu_hits));
}

static void update_stats(u64 jitter_ns)
{
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
		pr_info("ecat_tx_driver: cycle jitter (ns)"
			" min=%llu max=%llu avg=%llu gmax=%llu\n",
			stats.min_jitter, stats.max_jitter,
			avg, stats.g_max_jitter);
		stats.min_jitter = U64_MAX;
		stats.max_jitter = 0;
		stats.total_jitter = 0;
		stats.count = 0;
	}
}

static int stats_show(struct seq_file *m, void *v)
{
	int i;
	u64 avg = stats.count ? stats.total_jitter / stats.count : 0;
	seq_printf(m, "period_ns: %u\n", CYCLE_NS);
	seq_printf(m, "window_size: %u\n", WINDOW_SIZE);
	seq_printf(m, "timer_cpu_param: %d\n", timer_cpu);
	seq_printf(m, "last_cpu: %u\n",
		   stats.last_cpu == U32_MAX ? 0U : stats.last_cpu);
	seq_printf(m, "count: %u\n", stats.count);
	seq_printf(m, "min_jitter: %llu ns\n",
		   stats.min_jitter == U64_MAX ? 0ULL : stats.min_jitter);
	seq_printf(m, "max_jitter: %llu ns\n", stats.max_jitter);
	seq_printf(m, "avg_jitter: %llu ns\n", avg);
	seq_printf(m, "g_max_jitter: %llu ns\n", stats.g_max_jitter);
	for (i = 0; i < NR_CPUS; i++) {
		if (stats.cpu_hits[i])
			seq_printf(m, "cpu%u_hits: %llu\n", i, stats.cpu_hits[i]);
	}
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
 * hrtimer callback -- one EtherCAT cycle
 ******************************************************************************/

static ktime_t prev_fire_time; /* init to 0 = uninitialized */

static int cycle_thread_fn(void *data)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(cycle_wq,
			atomic_read(&cycle_pending) || kthread_should_stop());
		if (kthread_should_stop())
			break;

		atomic_set(&cycle_pending, 0);

		ecrt_master_receive(master);
		ecrt_domain_process(domain);

		toggle_val = !toggle_val;
		EC_WRITE_U32(domain_data + pd_offset, toggle_val);

		ecrt_domain_queue(domain);
		ecrt_master_send(master);
	}

	return 0;
}

static enum hrtimer_restart cycle_cb(struct hrtimer *timer)
{
	ktime_t now;
	s64 interval_ns;
	u64 jitter_ns;
	u32 cpu;

	cpu = raw_smp_processor_id();
	stats.last_cpu = cpu;
	if (cpu < NR_CPUS)
		stats.cpu_hits[cpu]++;

	now = ktime_get();

	/* measure actual interval between consecutive firings */
	if (ktime_to_ns(prev_fire_time)) {
		interval_ns = ktime_to_ns(ktime_sub(now, prev_fire_time));
		jitter_ns = (interval_ns > CYCLE_NS)
			? (u64)(interval_ns - CYCLE_NS)
			: (u64)(CYCLE_NS - interval_ns);
		update_stats(jitter_ns);
	}
	prev_fire_time = now;

	atomic_set(&cycle_pending, 1);
	wake_up_interruptible(&cycle_wq);

	hrtimer_forward_now(timer, ns_to_ktime(CYCLE_NS));
	return HRTIMER_RESTART;
}

/******************************************************************************
 * Module init / exit
 ******************************************************************************/

static void start_timer_on_target_cpu(void *unused)
{
	hrtimer_start(&cycle_timer, ns_to_ktime(CYCLE_NS),
		      HRTIMER_MODE_REL_PINNED);
}

static int __init ecat_tx_driver_init(void)
{
	int ret;

	master = ecrt_request_master(MASTER_IDX);
	if (!master) {
		pr_err("ecat_tx_driver: request master %u failed\n",
		       MASTER_IDX);
		return -EBUSY;
	}

	domain = ecrt_master_create_domain(master);
	if (!domain) {
		pr_err("ecat_tx_driver: create domain failed\n");
		ret = -ENOMEM;
		goto err_release;
	}

	sc = ecrt_master_slave_config(master, SLAVE_ALIAS, SLAVE_POS,
				      VENDOR_ID, PRODUCT_CODE);
	if (!sc) {
		pr_err("ecat_tx_driver: slave config failed\n");
		ret = -ENODEV;
		goto err_release;
	}

	ret = ecrt_slave_config_pdos(sc, EC_END, syncs);
	if (ret < 0) {
		pr_err("ecat_tx_driver: PDO config failed (%d)\n", ret);
		goto err_release;
	}

	ret = ecrt_slave_config_reg_pdo_entry(sc, 0x3101, 1, domain, NULL);
	if (ret < 0) {
		pr_err("ecat_tx_driver: PDO entry reg failed (%d)\n", ret);
		goto err_release;
	}
	pd_offset = (unsigned int)ret;

	ret = ecrt_master_activate(master);
	if (ret < 0) {
		pr_err("ecat_tx_driver: activate failed (%d)\n", ret);
		goto err_release;
	}

	domain_data = ecrt_domain_data(domain);
	if (!domain_data) {
		pr_err("ecat_tx_driver: domain data failed\n");
		ret = -ENOMEM;
		goto err_deactivate;
	}

	reset_stats();
	toggle_val = 0;
	init_waitqueue_head(&cycle_wq);
	atomic_set(&cycle_pending, 0);

	if (timer_cpu < 0 || !cpu_online(timer_cpu)) {
		pr_err("ecat_tx_driver: invalid/offline timer_cpu=%d\n",
		       timer_cpu);
		ret = -EINVAL;
		goto err_deactivate;
	}

	/* debugfs */
	debugfs_dir = debugfs_create_dir("ecat_tx_driver", NULL);
	if (IS_ERR(debugfs_dir)) {
		ret = PTR_ERR(debugfs_dir);
		pr_err("ecat_tx_driver: debugfs dir failed (%d)\n", ret);
		goto err_deactivate;
	}
	debugfs_create_file("stats", 0444, debugfs_dir, NULL, &stats_fops);
	debugfs_create_file("reset", 0200, debugfs_dir, NULL, &reset_fops);

	cycle_task = kthread_create(cycle_thread_fn, NULL, "ecat_tx_driver_cycle");
	if (IS_ERR(cycle_task)) {
		ret = PTR_ERR(cycle_task);
		cycle_task = NULL;
		pr_err("ecat_tx_driver: create cycle thread failed (%d)\n", ret);
		goto err_debugfs;
	}

	kthread_bind(cycle_task, timer_cpu);
	sched_set_fifo(cycle_task);
	wake_up_process(cycle_task);

	/* start cycle timer */
	hrtimer_init(&cycle_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL_PINNED);
	cycle_timer.function = cycle_cb;
	ret = smp_call_function_single(timer_cpu, start_timer_on_target_cpu,
				       NULL, 1);
	if (ret) {
		pr_err("ecat_tx_driver: start timer on cpu%d failed (%d)\n",
		       timer_cpu, ret);
		goto err_stop_thread;
	}

	pr_info("ecat_tx_driver: started, period=%uns, pd_offset=%u, timer_cpu=%d\n",
		CYCLE_NS, pd_offset, timer_cpu);
	return 0;

err_stop_thread:
	if (cycle_task) {
		kthread_stop(cycle_task);
		cycle_task = NULL;
	}
err_debugfs:
	debugfs_remove_recursive(debugfs_dir);
err_deactivate:
	ecrt_master_deactivate(master);
err_release:
	ecrt_release_master(master);
	return ret;
}

static void __exit ecat_tx_driver_exit(void)
{
	hrtimer_cancel(&cycle_timer);
	if (cycle_task) {
		kthread_stop(cycle_task);
		cycle_task = NULL;
	}

	debugfs_remove_recursive(debugfs_dir);

	ecrt_master_deactivate(master);
	ecrt_release_master(master);

	pr_info("ecat_tx_driver: unloaded, g_max_jitter=%llu ns\n",
		stats.g_max_jitter);
}

module_init(ecat_tx_driver_init);
module_exit(ecat_tx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rockchip");
MODULE_DESCRIPTION("EtherCAT Master Kernel Performance Test");
MODULE_VERSION("1.0");
