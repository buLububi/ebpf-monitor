#ifndef __NET_MULTI_COMMON_H
#define __NET_MULTI_COMMON_H

struct net_multi_counts {
    __u64 net_dev_queue_cnt;
    __u64 net_dev_queue_bytes;

    __u64 netif_receive_skb_cnt;
    __u64 netif_receive_skb_bytes;

    __u64 inet_sock_set_state_cnt;
    __u64 tcp_established_cnt;
    __u64 tcp_close_state_cnt;

    __u64 tcp_v4_connect_cnt;
    __u64 tcp_close_cnt;
};

#endif
