#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/timerfd.h>

#include "ecrt.h"

#define CYCLE_NS 500000ULL
#define WINDOW_SIZE 20000U

static unsigned int pd_offset;
static uint32_t toggle_val;
static volatile sig_atomic_t g_stop;

struct jitter_stats {
    uint64_t min_jitter;
    uint64_t max_jitter;
    uint64_t g_max_jitter;
    uint64_t total_jitter;
    uint32_t count;
};

struct pthread_args {
    ec_master_t *master;
    ec_domain_t *domain;
    uint8_t *domain_data;
    int timer_fd;
};

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

static uint64_t ts_to_ns(const struct timespec *ts)
{
    return (uint64_t) ts->tv_sec * 1000000000ULL + (uint64_t) ts->tv_nsec;
}

static void reset_stats(struct jitter_stats *s)
{
    s->min_jitter = UINT64_MAX;
    s->max_jitter = 0;
    s->g_max_jitter = 0;
    s->total_jitter = 0;
    s->count = 0;
}

static void update_stats(struct jitter_stats *s, uint64_t jitter_ns)
{
    if (jitter_ns < s->min_jitter)
        s->min_jitter = jitter_ns;
    if (jitter_ns > s->max_jitter)
        s->max_jitter = jitter_ns;
    if (jitter_ns > s->g_max_jitter)
        s->g_max_jitter = jitter_ns;

    s->total_jitter += jitter_ns;
    s->count++;

    if (s->count >= WINDOW_SIZE) {
        uint64_t avg = s->total_jitter / s->count;
        printf("ec_user_master: cycle jitter(ns) min=%llu max=%llu avg=%llu gmax=%llu\n",
               (unsigned long long) s->min_jitter,
               (unsigned long long) s->max_jitter,
               (unsigned long long) avg,
               (unsigned long long) s->g_max_jitter);
        fflush(stdout);
        s->min_jitter = UINT64_MAX;
        s->max_jitter = 0;
        s->total_jitter = 0;
        s->count = 0;
    }
}

static void on_signal(int sig)
{
    (void) sig;
    g_stop = 1;
}

void *cycle_thread_fn(void *data)
{
    struct pthread_args *args = (struct pthread_args *)data;
    ec_master_t *master = args->master;
    ec_domain_t *domain = args->domain;
    uint8_t *domain_data = args->domain_data;
    struct jitter_stats stats;
    struct timespec now;
    uint64_t prev_fire_ns = 0;

    reset_stats(&stats);

    while (!g_stop) {
        uint64_t expirations;
        ssize_t n;

        n = read(args->timer_fd, &expirations, sizeof(expirations));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("timerfd read failed");
            break;
        }
        if ((size_t)n != sizeof(expirations)) {
            fprintf(stderr, "timerfd read returned %zd bytes\n", n);
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
            uint64_t now_ns = ts_to_ns(&now);
            if (prev_fire_ns != 0) {
                uint64_t interval_ns = now_ns - prev_fire_ns;
                uint64_t jitter_ns = (interval_ns > CYCLE_NS)
                    ? (interval_ns - CYCLE_NS)
                    : (CYCLE_NS - interval_ns);
                update_stats(&stats, jitter_ns);
            }
            prev_fire_ns = now_ns;
        }

        if (expirations > 1) {
            printf("ec_user_master: timer overrun, expirations=%llu\n",
                   (unsigned long long) expirations);
        }

        ecrt_master_receive(master);
        ecrt_domain_process(domain);

        toggle_val = !toggle_val;
        EC_WRITE_U32(domain_data + pd_offset, toggle_val);

        ecrt_domain_queue(domain);
        ecrt_master_send(master);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    ec_master_t *master;
    ec_domain_t *domain;
    ec_slave_config_t *sc;
    uint8_t *domain_data;
    int ret;
    int timer_fd = -1;
    int cycle_cpu = 7;
    struct itimerspec its;
    pthread_t cycle_thread;
    struct pthread_args args;

    if (argc >= 2)
        cycle_cpu = atoi(argv[1]);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    master = ecrt_request_master(0);
    if (!master) {
        fprintf(stderr, "Failed to request master\n");
        return EXIT_FAILURE;
    }

    domain = ecrt_master_create_domain(master);
    if (!domain) {
        fprintf(stderr, "Failed to create domain\n");
        goto release_master;
    }

    sc = ecrt_master_slave_config(master, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    if (!sc) {
        fprintf(stderr, "Failed to create slave config\n");
        goto release_master;
    }

    ret = ecrt_slave_config_pdos(sc, EC_END, syncs);
    if (ret < 0) {
        fprintf(stderr, "Failed to configure PDOs: %d\n", ret);
        goto release_master;
    }

    ret = ecrt_slave_config_reg_pdo_entry(sc, 0x3101, 1, domain, &pd_offset);
    if (ret < 0) {
        fprintf(stderr, "Failed to register PDO entry: %d\n", ret);
        goto release_master;
    }

    ret = ecrt_master_activate(master);
    if (ret < 0) {
        fprintf(stderr, "Failed to activate master: %d\n", ret);
        goto release_master;
    }

    domain_data = ecrt_domain_data(domain);
    if (!domain_data) {
        fprintf(stderr, "Failed to get domain data\n");
        goto err_master_deactivate;
    }

    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd < 0) {
        perror("timerfd_create failed");
        goto err_master_deactivate;
    }

    memset(&its, 0, sizeof(its));
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = CYCLE_NS;
    its.it_value = its.it_interval;

    if (timerfd_settime(timer_fd, 0, &its, NULL) < 0) {
        perror("timerfd_settime failed");
        goto err_timer_close;
    }

    args = (struct pthread_args) {
        .master = master,
        .domain = domain,
        .domain_data = domain_data,
        .timer_fd = timer_fd,
    };

    ret = pthread_create(&cycle_thread, NULL, cycle_thread_fn, &args);
    if (ret != 0) {
        fprintf(stderr, "Failed to create cycle thread: %d\n", ret);
        goto err_timer_close;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cycle_cpu, &cpuset);
    ret = pthread_setaffinity_np(cycle_thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        fprintf(stderr, "pthread_setaffinity_np failed: %d\n", ret);
    } else {
        printf("Cycle thread pinned to CPU %d\n", cycle_cpu);
    }

    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = 80;
    ret = pthread_setschedparam(cycle_thread, SCHED_FIFO, &sp);
    if (ret != 0) {
        fprintf(stderr, "pthread_setschedparam(SCHED_FIFO) failed: %d\n", ret);
    } else {
        printf("Cycle thread set to SCHED_FIFO priority %d\n", sp.sched_priority);
    }

    printf("ec_user_master running at 2kHz, press Ctrl+C to stop\n");

    pthread_join(cycle_thread, NULL);
    printf("Cycle thread finished\n");

    close(timer_fd);

    ecrt_master_deactivate(master);
    ecrt_release_master(master);

    return 0;

err_timer_close:
    if (timer_fd >= 0)
        close(timer_fd);
err_master_deactivate:
    ecrt_master_deactivate(master);
release_master:
    ecrt_release_master(master);

    return EXIT_FAILURE;
}