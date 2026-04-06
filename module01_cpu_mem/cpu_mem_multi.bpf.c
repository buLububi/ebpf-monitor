// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "cpu_mem_multi_common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/*
 * 每个 monitor 实例只绑定一个目标 pid namespace
 */
const volatile __u64 target_pidns_dev = 0;
const volatile __u64 target_pidns_ino = 0;

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct cpu_mem_multi_counts);
} counts_map SEC(".maps");

/*
 * 记录 wakeup 时间，用于计算调度延迟
 * key: host pid
 * value: wakeup timestamp(ns)
 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 32768);
    __type(key, __u32);
    __type(value, __u64);
} wakeup_ts_map SEC(".maps");

/*
 * 依赖 linux/bpf.h 里的 bpf_pidns_info
 */
static __always_inline int pass_pidns_filter(void)
{
    struct bpf_pidns_info nsinfo = {};
    long ret;

    if (target_pidns_ino == 0 || target_pidns_dev == 0)
        return 1;

    ret = bpf_get_ns_current_pid_tgid(target_pidns_dev,
                                      target_pidns_ino,
                                      &nsinfo,
                                      sizeof(nsinfo));
    return ret == 0;
}

static __always_inline struct cpu_mem_multi_counts *get_counts(void)
{
    __u32 key = 0;
    return bpf_map_lookup_elem(&counts_map, &key);
}

/* sched_wakeup tracepoint */
struct sched_wakeup_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;

    char  comm[16];
    __s32 pid;
    __s32 prio;
    __s32 target_cpu;
};

/* sched_switch tracepoint */
struct sched_switch_args {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;

    char  prev_comm[16];
    __s32 prev_pid;
    __s32 prev_prio;
    long  prev_state;
    char  next_comm[16];
    __s32 next_pid;
    __s32 next_prio;
};

SEC("tracepoint/sched/sched_switch")
int on_sched_switch(struct sched_switch_args *ctx)
{
    struct cpu_mem_multi_counts *c;
    __u32 next_pid;
    __u64 *tsp, now, delta;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->sched_switch_cnt++;

    next_pid = (__u32)ctx->next_pid;
    tsp = bpf_map_lookup_elem(&wakeup_ts_map, &next_pid);
    if (tsp) {
        now = bpf_ktime_get_ns();
        if (now > *tsp) {
            delta = now - *tsp;
            c->sched_latency_sum_ns += delta;
            c->sched_latency_cnt += 1;
            c->sched_latency_last_ns = delta;
            if (delta > c->sched_latency_max_ns)
                c->sched_latency_max_ns = delta;
        }
        bpf_map_delete_elem(&wakeup_ts_map, &next_pid);
    }

    return 0;
}

SEC("tracepoint/sched/sched_wakeup")
int on_sched_wakeup(struct sched_wakeup_args *ctx)
{
    struct cpu_mem_multi_counts *c;
    __u64 now;
    __u32 pid;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->sched_wakeup_cnt++;

    now = bpf_ktime_get_ns();
    pid = (__u32)ctx->pid;
    bpf_map_update_elem(&wakeup_ts_map, &pid, &now, BPF_ANY);

    return 0;
}

SEC("tracepoint/sched/sched_wakeup_new")
int on_sched_wakeup_new(struct sched_wakeup_args *ctx)
{
    struct cpu_mem_multi_counts *c;
    __u64 now;
    __u32 pid;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->sched_wakeup_new_cnt++;

    now = bpf_ktime_get_ns();
    pid = (__u32)ctx->pid;
    bpf_map_update_elem(&wakeup_ts_map, &pid, &now, BPF_ANY);

    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int on_sched_process_exit(void *ctx)
{
    struct cpu_mem_multi_counts *c;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->sched_process_exit_cnt++;
    return 0;
}

SEC("tracepoint/kmem/mm_page_alloc")
int on_mm_page_alloc(void *ctx)
{
    struct cpu_mem_multi_counts *c;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->mm_page_alloc_cnt++;
    return 0;
}

SEC("tracepoint/kmem/mm_page_free")
int on_mm_page_free(void *ctx)
{
    struct cpu_mem_multi_counts *c;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->mm_page_free_cnt++;
    return 0;
}
