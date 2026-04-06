#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include "../common/ctr_common.h"
#include "../common/target.h"
#include "ctr_syscalls.skel.h"

static int handle_event(void *ctx, void *data, size_t sz)
{
    const struct syscall_event *e = data;
    printf("%-8u %-8u %-6u %-12lld %-10.3f %-16s %-12llu %-12llu %-12llu\n",
           e->tgid,
           e->pid,
           e->id,
           (long long)e->ret,
           e->dur_ns / 1000.0,
           e->comm,
           (unsigned long long)e->cgroup_id,
           (unsigned long long)e->mnt_ns,
           (unsigned long long)e->pid_ns);
    return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *fmt, va_list args)
{
    if (level == LIBBPF_DEBUG)
        return 0;
    return vfprintf(stderr, fmt, args);
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "--pid") != 0) {
        fprintf(stderr, "Usage: %s --pid <host-pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)strtol(argv[2], NULL, 10);
    struct target_info ti;

    if (load_target_info(pid, &ti) != 0) {
        fprintf(stderr, "load_target_info failed\n");
        return 1;
    }

    print_target_info(&ti);

    libbpf_set_print(libbpf_print_fn);
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    struct ctr_syscalls_bpf *skel = ctr_syscalls_bpf__open();
    if (!skel) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    skel->rodata->target_cgroup_id = ti.cgroup_id;
    skel->rodata->target_mnt_ns = ti.mnt_ns_id;

    if (ctr_syscalls_bpf__load(skel)) {
        fprintf(stderr, "load failed\n");
        ctr_syscalls_bpf__destroy(skel);
        return 1;
    }

    if (ctr_syscalls_bpf__attach(skel)) {
        fprintf(stderr, "attach failed\n");
        ctr_syscalls_bpf__destroy(skel);
        return 1;
    }

    struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "ring_buffer__new failed\n");
        ctr_syscalls_bpf__destroy(skel);
        return 1;
    }

    printf("%-8s %-8s %-6s %-12s %-10s %-16s %-12s %-12s %-12s\n",
           "TGID", "TID", "ID", "RET", "DUR(us)", "COMM",
           "CGROUP_ID", "MNT_NS", "PID_NS");

    while (1) {
        int err = ring_buffer__poll(rb, 200);
        if (err == -EINTR)
            break;
        if (err < 0) {
            fprintf(stderr, "poll failed: %d\n", err);
            break;
        }
    }

    ring_buffer__free(rb);
    ctr_syscalls_bpf__destroy(skel);
    return 0;
}
