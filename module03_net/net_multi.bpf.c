// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "../common/net_multi_common.h"

char LICENSE[] SEC("license") = "GPL";

const volatile __u64 target_cgroup_id = 0;

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct net_multi_counts);
} counts_map SEC(".maps");

struct net_dev_queue_args {
    __u16 common_type;
    __u8 common_flags;
    __u8 common_preempt_count;
    __s32 common_pid;

    void *skbaddr;
    unsigned int len;
    char name[16];
};

struct netif_receive_skb_args {
    __u16 common_type;
    __u8 common_flags;
    __u8 common_preempt_count;
    __s32 common_pid;

    void *skbaddr;
    unsigned int len;
    char name[16];
};

struct inet_sock_set_state_args {
    __u16 common_type;
    __u8 common_flags;
    __u8 common_preempt_count;
    __s32 common_pid;

    void *skaddr;
    int oldstate;
    int newstate;
    __u16 sport;
    __u16 dport;
    __u16 family;
    __u16 protocol;
};

enum {
    TCP_ESTABLISHED = 1,
    TCP_CLOSE = 7,
};

static __always_inline int match_target_cgroup(void)
{
    __u64 cg = bpf_get_current_cgroup_id();
    return cg == target_cgroup_id;
}

static __always_inline struct net_multi_counts *get_counts(void)
{
    __u32 key = 0;
    return bpf_map_lookup_elem(&counts_map, &key);
}

SEC("tracepoint/net/net_dev_queue")
int on_net_dev_queue(struct net_dev_queue_args *ctx)
{
    struct net_multi_counts *c;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->net_dev_queue_cnt++;
    c->net_dev_queue_bytes += ctx->len;
    return 0;
}

SEC("tracepoint/net/netif_receive_skb")
int on_netif_receive_skb(struct netif_receive_skb_args *ctx)
{
    struct net_multi_counts *c;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->netif_receive_skb_cnt++;
    c->netif_receive_skb_bytes += ctx->len;
    return 0;
}

SEC("tracepoint/sock/inet_sock_set_state")
int on_inet_sock_set_state(struct inet_sock_set_state_args *ctx)
{
    struct net_multi_counts *c;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->inet_sock_set_state_cnt++;
    if (ctx->newstate == TCP_ESTABLISHED)
        c->tcp_established_cnt++;
    if (ctx->newstate == TCP_CLOSE)
        c->tcp_close_state_cnt++;

    return 0;
}

SEC("kprobe/tcp_v4_connect")
int BPF_KPROBE(on_tcp_v4_connect)
{
    struct net_multi_counts *c;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->tcp_v4_connect_cnt++;
    return 0;
}

SEC("kprobe/tcp_close")
int BPF_KPROBE(on_tcp_close)
{
    struct net_multi_counts *c;

    if (!match_target_cgroup())
        return 0;

    c = get_counts();
    if (!c)
        return 0;

    c->tcp_close_cnt++;
    return 0;
}
