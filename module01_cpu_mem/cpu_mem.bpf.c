// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "../common/cpu_mem_common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

const volatile __u64 target_pidns_dev = 0;
const volatile __u64 target_pidns_ino = 0;

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct cpu_mem_counts);
} counts_map SEC(".maps");

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

static __always_inline struct cpu_mem_counts *get_counts(void)
{
    __u32 key = 0;
    return bpf_map_lookup_elem(&counts_map, &key);
}

static __always_inline void update_wait_stats(struct cpu_mem_counts *c, __u64 delay_ns)
{
    if (!c)
        return;

    c->sched_wait_cnt++;
    c->sched_wait_total_ns += delay_ns;
    if (delay_ns > c->sched_wait_max_ns)
        c->sched_wait_max_ns = delay_ns;
}

SEC("tracepoint/sched/sched_switch")
int on_sched_switch(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->sched_switch_cnt++;
    return 0;
}

SEC("tracepoint/sched/sched_wakeup")
int on_sched_wakeup(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->sched_wakeup_cnt++;
    return 0;
}

SEC("tracepoint/sched/sched_wakeup_new")
int on_sched_wakeup_new(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->sched_wakeup_new_cnt++;
    return 0;
}

SEC("tracepoint/sched/sched_process_exit")
int on_sched_process_exit(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->sched_process_exit_cnt++;
    return 0;
}

SEC("tracepoint/kmem/mm_page_alloc")
int on_mm_page_alloc(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->mm_page_alloc_cnt++;
    return 0;
}

SEC("tracepoint/kmem/mm_page_free")
int on_mm_page_free(void *ctx)
{
    struct cpu_mem_counts *c;
    if (!pass_pidns_filter())
        return 0;
    c = get_counts();
    if (c)
        c->mm_page_free_cnt++;
    return 0;
}

/*
 * 调度等待延迟（run queue wait time）
 * delay 单位是 ns
 */
struct trace_event_raw_sched_stat_wait {
    __u16 common_type;
    __u8  common_flags;
    __u8  common_preempt_count;
    __s32 common_pid;
    char  comm[16];
    __u32 pid;
    __u64 delay;
};

SEC("tracepoint/sched/sched_stat_wait")
int on_sched_stat_wait(struct trace_event_raw_sched_stat_wait *ctx)
{
    struct cpu_mem_counts *c;

    if (!pass_pidns_filter())
        return 0;

    c = get_counts();
    if (c)
        update_wait_stats(c, ctx->delay);

    return 0;
}
