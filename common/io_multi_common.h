#ifndef __IO_MULTI_COMMON_H
#define __IO_MULTI_COMMON_H

struct io_multi_counts {
    __u64 read_calls;
    __u64 write_calls;

    __u64 read_bytes;
    __u64 write_bytes;

    __u64 io_errors;

    __u64 io_latency_sum_ns;
    __u64 io_latency_cnt;
    __u64 io_latency_max_ns;
    __u64 io_latency_last_ns;
};

struct io_pending_op {
    __u64 start_ns;
    __u32 op;
};

enum io_op_type {
    IO_OP_READ = 1,
    IO_OP_WRITE = 2,
};

#endif
