// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include "vmlinux.h"
#include <stdbool.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../common/ctr_common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

const volatile __u64 target_cgroup_id = 0;
const volatile __u64 target_mnt_ns = 0;

struct start_val {
    __u64 ts_ns;
    __u32 id;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u32);
    __type(value, struct start_val);
} starts SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} rb SEC(".maps");

static __always_inline __u64 current_mnt_ns_id(void)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task_btf();
    if (!task)
        return 0;
    return BPF_CORE_READ(task, nsproxy, mnt_ns, ns.inum);
}

static __always_inline __u64 current_pid_ns_id(void)
{
    struct task_struct *task = (struct task_struct *)bpf_get_current_task_btf();
    if (!task)
        return 0;
    return BPF_CORE_READ(task, nsproxy, pid_ns_for_children, ns.inum);
}

static __always_inline bool pass_container_filter(void)
{
    __u64 cur_cgroup = bpf_get_current_cgroup_id();
    __u64 cur_mnt_ns = current_mnt_ns_id();

    bool cgroup_ok = (target_cgroup_id == 0) || (cur_cgroup == target_cgroup_id);
    bool mnt_ok = (target_mnt_ns == 0) || (cur_mnt_ns == target_mnt_ns);

    return cgroup_ok && mnt_ok;
}

SEC("tracepoint/raw_syscalls/sys_enter")
int tp_sys_enter(struct trace_event_raw_sys_enter *ctx)
{
    if (!pass_container_filter())
        return 0;

    __u64 ts = bpf_ktime_get_ns();
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = (__u32)pid_tgid;
    __u32 tgid = pid_tgid >> 32;
    __u32 id = (__u32)ctx->id;

    struct start_val sv = {
        .ts_ns = ts,
        .id = id,
    };
    bpf_map_update_elem(&starts, &pid, &sv, BPF_ANY);

    return 0;
}

SEC("tracepoint/raw_syscalls/sys_exit")
int tp_sys_exit(struct trace_event_raw_sys_exit *ctx)
{
    if (!pass_container_filter())
        return 0;

    __u64 ts = bpf_ktime_get_ns();
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = (__u32)pid_tgid;
    __u32 tgid = pid_tgid >> 32;

    __u64 dur = 0;
    __u32 id = 0;

    struct start_val *sv = bpf_map_lookup_elem(&starts, &pid);
    if (sv) {
        id = sv->id;
        if (ts > sv->ts_ns)
            dur = ts - sv->ts_ns;
        bpf_map_delete_elem(&starts, &pid);
    } else {
        id = (__u32)ctx->id;
    }

    struct syscall_event *e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e)
        return 0;

    e->ts_ns = ts;
    e->cgroup_id = bpf_get_current_cgroup_id();
    e->mnt_ns = current_mnt_ns_id();
    e->pid_ns = current_pid_ns_id();
    e->tgid = tgid;
    e->pid = pid;
    e->ret = ctx->ret;
    e->id = id;
    e->dur_ns = dur;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    return 0;
}
