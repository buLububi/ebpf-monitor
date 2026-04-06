#ifndef __CPU_MEM_COMMON_H
#define __CPU_MEM_COMMON_H

#ifndef __u8
typedef unsigned char __u8;
#endif

#ifndef __u16
typedef unsigned short __u16;
#endif

#ifndef __u32
typedef unsigned int __u32;
#endif

#ifndef __u64
typedef unsigned long long __u64;
#endif

struct cpu_mem_counts {
    __u64 sched_switch_cnt;
    __u64 sched_wakeup_cnt;
    __u64 sched_wakeup_new_cnt;
    __u64 sched_process_exit_cnt;
    __u64 mm_page_alloc_cnt;
    __u64 mm_page_free_cnt;

    __u64 sched_wait_cnt;
    __u64 sched_wait_total_ns;
    __u64 sched_wait_max_ns;
};

#endif
