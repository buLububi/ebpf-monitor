#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "target.h"
#include "../common/io_multi_common.h"
#include "io_multi.skel.h"

static volatile sig_atomic_t exiting = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    exiting = 1;
}

static int read_io_stat(const char *cg_path,
                        unsigned long long *rbytes,
                        unsigned long long *wbytes,
                        unsigned long long *rios,
                        unsigned long long *wios,
                        unsigned long long *dbytes,
                        unsigned long long *dios)
{
    char path[PATH_MAX * 2];
    FILE *f;
    char line[512];

    *rbytes = *wbytes = *rios = *wios = *dbytes = *dios = 0;

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/io.stat", cg_path);
    f = fopen(path, "re");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while ((p = strstr(p, "rbytes=")) != NULL) {
            *rbytes += strtoull(p + 7, NULL, 10);
            p += 7;
        }
        p = line;
        while ((p = strstr(p, "wbytes=")) != NULL) {
            *wbytes += strtoull(p + 7, NULL, 10);
            p += 7;
        }
        p = line;
        while ((p = strstr(p, "rios=")) != NULL) {
            *rios += strtoull(p + 5, NULL, 10);
            p += 5;
        }
        p = line;
        while ((p = strstr(p, "wios=")) != NULL) {
            *wios += strtoull(p + 5, NULL, 10);
            p += 5;
        }
        p = line;
        while ((p = strstr(p, "dbytes=")) != NULL) {
            *dbytes += strtoull(p + 7, NULL, 10);
            p += 7;
        }
        p = line;
        while ((p = strstr(p, "dios=")) != NULL) {
            *dios += strtoull(p + 5, NULL, 10);
            p += 5;
        }
    }

    fclose(f);
    return 0;
}

static void print_attach_points(void)
{
    printf("===== IO ATTACH POINTS =====\n");
    printf("1. tracepoint/syscalls/sys_enter_read\n");
    printf("2. tracepoint/syscalls/sys_exit_read\n");
    printf("3. tracepoint/syscalls/sys_enter_write\n");
    printf("4. tracepoint/syscalls/sys_exit_write\n");
    printf("============================\n");
    printf("container filter          : cgroup id\n");
}

int main(int argc, char **argv)
{
    struct target_info ti;
    struct io_multi_bpf *skel = NULL;
    struct io_multi_counts counts;
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

    skel = io_multi_bpf__open();
    if (!skel) {
        fprintf(stderr, "io_multi_bpf__open failed\n");
        return 1;
    }

    skel->rodata->target_cgroup_id = ti.cgroup_id;

    if (io_multi_bpf__load(skel)) {
        fprintf(stderr, "io_multi_bpf__load failed\n");
        io_multi_bpf__destroy(skel);
        return 1;
    }

    if (io_multi_bpf__attach(skel)) {
        fprintf(stderr, "io_multi_bpf__attach failed\n");
        io_multi_bpf__destroy(skel);
        return 1;
    }

    map_fd = bpf_map__fd(skel->maps.counts_map);

    while (!exiting) {
        unsigned long long rbytes = 0, wbytes = 0, rios = 0, wios = 0, dbytes = 0, dios = 0;
        double avg_us = 0.0, max_us = 0.0, last_us = 0.0;

        if (read_io_stat(ti.cgroup_path, &rbytes, &wbytes, &rios, &wios, &dbytes, &dios) != 0) {
            fprintf(stderr, "read io.stat failed\n");
            break;
        }

        memset(&counts, 0, sizeof(counts));
        if (bpf_map_lookup_elem(map_fd, &key, &counts) != 0) {
            fprintf(stderr, "bpf_map_lookup_elem failed\n");
            break;
        }

        if (counts.io_latency_cnt > 0)
            avg_us = (double)counts.io_latency_sum_ns / (double)counts.io_latency_cnt / 1000.0;
        max_us = (double)counts.io_latency_max_ns / 1000.0;
        last_us = (double)counts.io_latency_last_ns / 1000.0;

        printf("\n===== IO SNAPSHOT =====\n");
        printf("container                : %s\n", container_name);
        printf("io.stat.rbytes           : %llu\n", rbytes);
        printf("io.stat.wbytes           : %llu\n", wbytes);
        printf("io.stat.rios             : %llu\n", rios);
        printf("io.stat.wios             : %llu\n", wios);
        printf("io.stat.dbytes           : %llu\n", dbytes);
        printf("io.stat.dios             : %llu\n", dios);
        printf("----- SYSCALL IO COUNTS -----\n");
        printf("read_calls               : %llu\n", counts.read_calls);
        printf("write_calls              : %llu\n", counts.write_calls);
        printf("read_bytes               : %llu\n", counts.read_bytes);
        printf("write_bytes              : %llu\n", counts.write_bytes);
        printf("io_errors                : %llu\n", counts.io_errors);
        printf("io_latency_avg_us        : %.3f\n", avg_us);
        printf("io_latency_max_us        : %.3f\n", max_us);
        printf("io_latency_last_us       : %.3f\n", last_us);
        printf("=======================\n");
        fflush(stdout);

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

                fprintf(sf, "io_stat_rbytes=%llu\n", rbytes);
                fprintf(sf, "io_stat_wbytes=%llu\n", wbytes);
                fprintf(sf, "io_stat_rios=%llu\n", rios);
                fprintf(sf, "io_stat_wios=%llu\n", wios);
                fprintf(sf, "io_stat_dbytes=%llu\n", dbytes);
                fprintf(sf, "io_stat_dios=%llu\n", dios);

                fprintf(sf, "read_calls=%llu\n", counts.read_calls);
                fprintf(sf, "write_calls=%llu\n", counts.write_calls);
                fprintf(sf, "read_bytes=%llu\n", counts.read_bytes);
                fprintf(sf, "write_bytes=%llu\n", counts.write_bytes);
                fprintf(sf, "io_errors=%llu\n", counts.io_errors);

                fprintf(sf, "io_latency_avg_us=%.3f\n", avg_us);
                fprintf(sf, "io_latency_max_us=%.3f\n", max_us);
                fprintf(sf, "io_latency_last_us=%.3f\n", last_us);

                fclose(sf);
                rename(tmpfile, state_file);
            }
        }

        {
            struct io_multi_counts new_counts;
            memcpy(&new_counts, &counts, sizeof(new_counts));
            new_counts.io_latency_sum_ns = 0;
            new_counts.io_latency_cnt = 0;
            new_counts.io_latency_max_ns = 0;
            new_counts.io_latency_last_ns = 0;

            if (bpf_map_update_elem(map_fd, &key, &new_counts, BPF_ANY) != 0) {
                perror("bpf_map_update_elem reset io window");
            }
        }

        sleep(1);
    }

    io_multi_bpf__destroy(skel);
    return 0;
}
