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
#include "../common/net_multi_common.h"
#include "net_multi.skel.h"

static volatile sig_atomic_t exiting = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    exiting = 1;
}

static void print_attach_points(void)
{
    printf("===== NET ATTACH POINTS =====\n");
    printf("1. tracepoint/net/net_dev_queue\n");
    printf("2. tracepoint/net/netif_receive_skb\n");
    printf("3. tracepoint/sock/inet_sock_set_state\n");
    printf("4. kprobe/tcp_v4_connect\n");
    printf("5. kprobe/tcp_close\n");
    printf("=============================\n");
    printf("container filter           : cgroup id\n");
}

int main(int argc, char **argv)
{
    struct target_info ti;
    struct net_multi_bpf *skel = NULL;
    struct net_multi_counts counts;
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

    skel = net_multi_bpf__open();
    if (!skel) {
        fprintf(stderr, "net_multi_bpf__open failed\n");
        return 1;
    }

    skel->rodata->target_cgroup_id = ti.cgroup_id;

    if (net_multi_bpf__load(skel)) {
        fprintf(stderr, "net_multi_bpf__load failed\n");
        net_multi_bpf__destroy(skel);
        return 1;
    }

    if (net_multi_bpf__attach(skel)) {
        fprintf(stderr, "net_multi_bpf__attach failed\n");
        net_multi_bpf__destroy(skel);
        return 1;
    }

    map_fd = bpf_map__fd(skel->maps.counts_map);

    while (!exiting) {
        memset(&counts, 0, sizeof(counts));
        if (bpf_map_lookup_elem(map_fd, &key, &counts) != 0) {
            fprintf(stderr, "bpf_map_lookup_elem failed\n");
            break;
        }

        printf("\n===== NET SNAPSHOT =====\n");
        printf("container                : %s\n", container_name);
        printf("net_dev_queue_cnt        : %llu\n", counts.net_dev_queue_cnt);
        printf("net_dev_queue_bytes      : %llu\n", counts.net_dev_queue_bytes);
        printf("netif_receive_skb_cnt    : %llu\n", counts.netif_receive_skb_cnt);
        printf("netif_receive_skb_bytes  : %llu\n", counts.netif_receive_skb_bytes);
        printf("inet_sock_set_state      : %llu\n", counts.inet_sock_set_state_cnt);
        printf("tcp_established          : %llu\n", counts.tcp_established_cnt);
        printf("tcp_close_state          : %llu\n", counts.tcp_close_state_cnt);
        printf("tcp_v4_connect           : %llu\n", counts.tcp_v4_connect_cnt);
        printf("tcp_close                : %llu\n", counts.tcp_close_cnt);
        printf("========================\n");
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

                fprintf(sf, "net_dev_queue_cnt=%llu\n", counts.net_dev_queue_cnt);
                fprintf(sf, "net_dev_queue_bytes=%llu\n", counts.net_dev_queue_bytes);
                fprintf(sf, "netif_receive_skb_cnt=%llu\n", counts.netif_receive_skb_cnt);
                fprintf(sf, "netif_receive_skb_bytes=%llu\n", counts.netif_receive_skb_bytes);
                fprintf(sf, "inet_sock_set_state_cnt=%llu\n", counts.inet_sock_set_state_cnt);
                fprintf(sf, "tcp_established_cnt=%llu\n", counts.tcp_established_cnt);
                fprintf(sf, "tcp_close_state_cnt=%llu\n", counts.tcp_close_state_cnt);
                fprintf(sf, "tcp_v4_connect_cnt=%llu\n", counts.tcp_v4_connect_cnt);
                fprintf(sf, "tcp_close_cnt=%llu\n", counts.tcp_close_cnt);

                fclose(sf);
                rename(tmpfile, state_file);
            }
        }

        sleep(1);
    }

    net_multi_bpf__destroy(skel);
    return 0;
}
