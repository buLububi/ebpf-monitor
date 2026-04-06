#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <linux/limits.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "target.h"
#include "cpu_mem_multi_common.h"
#include "cpu_mem_multi.skel.h"

static volatile sig_atomic_t exiting = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    exiting = 1;
}

static int read_ull_from_file(const char *path, unsigned long long *v)
{
    FILE *f = fopen(path, "re");
    if (!f)
        return -1;
    if (fscanf(f, "%llu", v) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int read_cpu_stat(const char *cg_path,
                         unsigned long long *usage_usec,
                         unsigned long long *user_usec,
                         unsigned long long *system_usec,
                         unsigned long long *nr_periods,
                         unsigned long long *nr_throttled,
                         unsigned long long *throttled_usec)
{
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/cpu.stat", cg_path);

    FILE *f = fopen(path, "re");
    if (!f)
        return -1;

    char key[128];
    unsigned long long val;
    *usage_usec = *user_usec = *system_usec = 0;
    *nr_periods = *nr_throttled = *throttled_usec = 0;

    while (fscanf(f, "%127s %llu", key, &val) == 2) {
        if (strcmp(key, "usage_usec") == 0) *usage_usec = val;
        else if (strcmp(key, "user_usec") == 0) *user_usec = val;
        else if (strcmp(key, "system_usec") == 0) *system_usec = val;
        else if (strcmp(key, "nr_periods") == 0) *nr_periods = val;
        else if (strcmp(key, "nr_throttled") == 0) *nr_throttled = val;
        else if (strcmp(key, "throttled_usec") == 0) *throttled_usec = val;
    }

    fclose(f);
    return 0;
}

static void read_mem_info(const char *cg_path,
                          unsigned long long *mem_current,
                          unsigned long long *mem_swap_current,
                          long long *mem_max)
{
    char path[PATH_MAX * 2];
    char buf[128] = {0};

    *mem_current = 0;
    *mem_swap_current = 0;
    *mem_max = -1;

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.current", cg_path);
    read_ull_from_file(path, mem_current);

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.swap.current", cg_path);
    read_ull_from_file(path, mem_swap_current);

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.max", cg_path);
    FILE *f = fopen(path, "re");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            if (strncmp(buf, "max", 3) != 0)
                *mem_max = atoll(buf);
        }
        fclose(f);
    }
}

static void print_attach_points(void)
{
    printf("===== ATTACH POINTS =====\n");
    printf("1. tracepoint/sched/sched_switch\n");
    printf("2. tracepoint/sched/sched_wakeup\n");
    printf("3. tracepoint/sched/sched_wakeup_new\n");
    printf("4. tracepoint/sched/sched_process_exit\n");
    printf("5. tracepoint/kmem/mm_page_alloc\n");
    printf("6. tracepoint/kmem/mm_page_free\n");
    printf("=========================\n");
    printf("namespace filter         : pid namespace\n");
}

int main(int argc, char **argv)
{
    struct target_info ti;
    struct cpu_mem_multi_bpf *skel = NULL;
    struct cpu_mem_multi_counts counts;
    __u32 key = 0;
    int map_fd;
    const char *container_name = NULL;
    const char *state_file = NULL;
    pid_t pid = 0;

    if (argc != 7) {
        fprintf(stderr, "Usage: %s --pid <host-pid> --container <name> --state-file <path>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pid") && i + 1 < argc) {
            pid = (pid_t)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--container") && i + 1 < argc) {
            container_name = argv[++i];
        } else if (!strcmp(argv[i], "--state-file") && i + 1 < argc) {
            state_file = argv[++i];
        }
    }

    if (pid <= 0 || !container_name || !state_file) {
        fprintf(stderr, "invalid args\n");
        return 1;
    }

    if (load_target_info(pid, &ti) != 0) {
        fprintf(stderr, "load_target_info failed\n");
        return 1;
    }

    print_target_info(&ti);
    print_attach_points();

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    skel = cpu_mem_multi_bpf__open();
    if (!skel) {
        fprintf(stderr, "cpu_mem_multi_bpf__open failed\n");
        return 1;
    }

    skel->rodata->target_pidns_dev = ti.pid_ns_dev;
    skel->rodata->target_pidns_ino = ti.pid_ns_id;

    if (cpu_mem_multi_bpf__load(skel)) {
        fprintf(stderr, "cpu_mem_multi_bpf__load failed\n");
        cpu_mem_multi_bpf__destroy(skel);
        return 1;
    }

    if (cpu_mem_multi_bpf__attach(skel)) {
        fprintf(stderr, "cpu_mem_multi_bpf__attach failed\n");
        cpu_mem_multi_bpf__destroy(skel);
        return 1;
    }

    map_fd = bpf_map__fd(skel->maps.counts_map);

    while (!exiting) {
        unsigned long long usage_usec = 0, user_usec = 0, system_usec = 0;
        unsigned long long nr_periods = 0, nr_throttled = 0, throttled_usec = 0;
        unsigned long long mem_current = 0, mem_swap_current = 0;
        long long mem_max = -1;
        double avg_us = 0.0, max_us = 0.0, last_us = 0.0;

        if (read_cpu_stat(ti.cgroup_path,
                          &usage_usec, &user_usec, &system_usec,
                          &nr_periods, &nr_throttled, &throttled_usec) != 0) {
            fprintf(stderr, "read cpu.stat failed\n");
            break;
        }

        read_mem_info(ti.cgroup_path, &mem_current, &mem_swap_current, &mem_max);

        memset(&counts, 0, sizeof(counts));
        if (bpf_map_lookup_elem(map_fd, &key, &counts) != 0) {
            fprintf(stderr, "bpf_map_lookup_elem failed\n");
            break;
        }

        if (counts.sched_latency_cnt > 0)
            avg_us = (double)counts.sched_latency_sum_ns / (double)counts.sched_latency_cnt / 1000.0;
        else
            avg_us = 0.0;

        max_us = (double)counts.sched_latency_max_ns / 1000.0;
        last_us = (double)counts.sched_latency_last_ns / 1000.0;

        printf("\n===== CPU / MEM SNAPSHOT =====\n");
        printf("container                : %s\n", container_name);
        printf("cpu.usage_usec           : %llu\n", usage_usec);
        printf("cpu.user_usec            : %llu\n", user_usec);
        printf("cpu.system_usec          : %llu\n", system_usec);
        printf("cpu.nr_periods           : %llu\n", nr_periods);
        printf("cpu.nr_throttled         : %llu\n", nr_throttled);
        printf("cpu.throttled_usec       : %llu\n", throttled_usec);
        printf("memory.current           : %llu bytes\n", mem_current);
        printf("memory.swap.current      : %llu bytes\n", mem_swap_current);
        printf("memory.max               : %lld\n", mem_max);
        printf("----- ATTACH POINT COUNTS -----\n");
        printf("sched_switch             : %llu\n", counts.sched_switch_cnt);
        printf("sched_wakeup             : %llu\n", counts.sched_wakeup_cnt);
        printf("sched_wakeup_new         : %llu\n", counts.sched_wakeup_new_cnt);
        printf("sched_process_exit       : %llu\n", counts.sched_process_exit_cnt);
        printf("mm_page_alloc            : %llu\n", counts.mm_page_alloc_cnt);
        printf("mm_page_free             : %llu\n", counts.mm_page_free_cnt);
        printf("sched_latency_avg_us     : %.3f\n", avg_us);
        printf("sched_latency_max_us     : %.3f\n", max_us);
        printf("sched_latency_last_us    : %.3f\n", last_us);
        printf("===============================\n");

        {
            char tmpfile[PATH_MAX * 2];
            snprintf(tmpfile, sizeof(tmpfile), "%s.tmp", state_file);

            FILE *sf = fopen(tmpfile, "w");
            if (sf) {
                fprintf(sf, "container=%s\n", container_name);
                fprintf(sf, "host_pid=%d\n", pid);
                fprintf(sf, "cgroup_path=%s\n", ti.cgroup_path);
                fprintf(sf, "mnt_ns=%llu\n", ti.mnt_ns_id);
                fprintf(sf, "pid_ns=%llu\n", ti.pid_ns_id);
                fprintf(sf, "net_ns=%llu\n", ti.net_ns_id);

                fprintf(sf, "cpu_usage_usec=%llu\n", usage_usec);
                fprintf(sf, "cpu_user_usec=%llu\n", user_usec);
                fprintf(sf, "cpu_system_usec=%llu\n", system_usec);
                fprintf(sf, "cpu_nr_periods=%llu\n", nr_periods);
                fprintf(sf, "cpu_nr_throttled=%llu\n", nr_throttled);
                fprintf(sf, "cpu_throttled_usec=%llu\n", throttled_usec);

                fprintf(sf, "memory_current_bytes=%llu\n", mem_current);
                fprintf(sf, "memory_swap_current_bytes=%llu\n", mem_swap_current);
                fprintf(sf, "memory_max_bytes=%lld\n", mem_max);

                fprintf(sf, "sched_switch_total=%llu\n", counts.sched_switch_cnt);
                fprintf(sf, "sched_wakeup_total=%llu\n", counts.sched_wakeup_cnt);
                fprintf(sf, "sched_wakeup_new_total=%llu\n", counts.sched_wakeup_new_cnt);
                fprintf(sf, "sched_process_exit_total=%llu\n", counts.sched_process_exit_cnt);
                fprintf(sf, "mm_page_alloc_total=%llu\n", counts.mm_page_alloc_cnt);
                fprintf(sf, "mm_page_free_total=%llu\n", counts.mm_page_free_cnt);

                fprintf(sf, "sched_latency_avg_us=%.3f\n", avg_us);
                fprintf(sf, "sched_latency_max_us=%.3f\n", max_us);
                fprintf(sf, "sched_latency_last_us=%.3f\n", last_us);

                fclose(sf);
                if (rename(tmpfile, state_file) != 0) {
                    perror("rename state file");
                }
            } else {
                perror("fopen state tmpfile");
            }
        }

        /*
         * 实时窗口版核心逻辑：
         * 每轮读取并导出完当前窗口统计后，将“窗口型调度延迟字段”清零，
         * 下一轮重新统计最近 1 秒窗口的数据。
         *
         * 保留累计计数字段：
         *   sched_switch_cnt / sched_wakeup_cnt / sched_wakeup_new_cnt / ...
         *
         * 清零窗口字段：
         *   sched_latency_sum_ns
         *   sched_latency_cnt
         *   sched_latency_max_ns
         *   sched_latency_last_ns
         */
        {
            struct cpu_mem_multi_counts new_counts;

            memcpy(&new_counts, &counts, sizeof(new_counts));
            new_counts.sched_latency_sum_ns = 0;
            new_counts.sched_latency_cnt = 0;
            new_counts.sched_latency_max_ns = 0;
            new_counts.sched_latency_last_ns = 0;

            if (bpf_map_update_elem(map_fd, &key, &new_counts, BPF_ANY) != 0) {
                perror("bpf_map_update_elem reset latency window");
            }
        }

        sleep(1);
    }

    cpu_mem_multi_bpf__destroy(skel);
    return 0;
}
