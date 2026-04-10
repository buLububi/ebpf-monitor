#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../common/io_multi_common.h"

char LICENSE[] SEC("license") = "GPL";

const volatile __u64 target_cgroup_id = 0;

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct io_multi_counts);
} counts_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);
    __type(value, struct io_pending_op);
} pending_map SEC(".maps");

struct sys_enter_args {
    unsigned long long unused;
    long id;
    unsigned long args[6];
};

struct sys_exit_args {
    unsigned long long unused;
    long id;
    long ret;
};

static __always_inline int match_target_cgroup(void)
{
    __u64 cg = bpf_get_current_cgroup_id();
    return cg == target_cgroup_id;
}

static __always_inline struct io_multi_counts *get_counts(void)
{
    __u32 key = 0;
    return bpf_map_lookup_elem(&counts_map, &key);
}

static __always_inline __u64 current_pid_tgid(void)
{
    return bpf_get_current_pid_tgid();
}

SEC("tracepoint/syscalls/sys_enter_read")
int on_sys_enter_read(struct sys_enter_args *ctx)
{
    struct io_pending_op op = {};
    __u64 pid_tgid;
    long fd;

    if (!match_target_cgroup())
        return 0;

    fd = (long)ctx->args[0];
    if (fd < 0)
        return 0;

    pid_tgid = current_pid_tgid();
    op.start_ns = bpf_ktime_get_ns();
    op.op = IO_OP_READ;
    bpf_map_update_elem(&pending_map, &pid_tgid, &op, BPF_ANY);
    return 0;
}

SEC("tracepoint/syscalls/sys_exit_read")
int on_sys_exit_read(struct sys_exit_args *ctx)
{
    struct io_multi_counts *c;
    struct io_pending_op *op;
    __u64 pid_tgid;
    __u64 delta;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    pid_tgid = current_pid_tgid();
    op = bpf_map_lookup_elem(&pending_map, &pid_tgid);
    if (!op || op->op != IO_OP_READ)
        return 0;

    delta = bpf_ktime_get_ns() - op->start_ns;

    c->read_calls++;
    if (ctx->ret > 0)
        c->read_bytes += (__u64)ctx->ret;
    else if (ctx->ret < 0)
        c->io_errors++;

    c->io_latency_sum_ns += delta;
    c->io_latency_cnt += 1;
    c->io_latency_last_ns = delta;
    if (delta > c->io_latency_max_ns)
        c->io_latency_max_ns = delta;

    bpf_map_delete_elem(&pending_map, &pid_tgid);
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_write")
int on_sys_enter_write(struct sys_enter_args *ctx)
{
    struct io_pending_op op = {};
    __u64 pid_tgid;
    long fd;

    if (!match_target_cgroup())
        return 0;

    fd = (long)ctx->args[0];
    if (fd < 0)
        return 0;

    pid_tgid = current_pid_tgid();
    op.start_ns = bpf_ktime_get_ns();
    op.op = IO_OP_WRITE;
    bpf_map_update_elem(&pending_map, &pid_tgid, &op, BPF_ANY);
    return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int on_sys_exit_write(struct sys_exit_args *ctx)
{
    struct io_multi_counts *c;
    struct io_pending_op *op;
    __u64 pid_tgid;
    __u64 delta;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    pid_tgid = current_pid_tgid();
    op = bpf_map_lookup_elem(&pending_map, &pid_tgid);
    if (!op || op->op != IO_OP_WRITE)
        return 0;

    delta = bpf_ktime_get_ns() - op->start_ns;

    c->write_calls++;
    if (ctx->ret > 0)
        c->write_bytes += (__u64)ctx->ret;
    else if (ctx->ret < 0)
        c->io_errors++;

    c->io_latency_sum_ns += delta;
    c->io_latency_cnt += 1;
    c->io_latency_last_ns = delta;
    if (delta > c->io_latency_max_ns)
        c->io_latency_max_ns = delta;

    bpf_map_delete_elem(&pending_map, &pid_tgid);
    return 0;
}
