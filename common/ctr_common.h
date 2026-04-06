#ifndef __CTR_COMMON_H
#define __CTR_COMMON_H

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

#ifndef __s32
typedef int __s32;
#endif

#ifndef __s64
typedef long long __s64;
#endif

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

struct syscall_event {
    __u64 ts_ns;
    __u64 cgroup_id;
    __u64 mnt_ns;
    __u64 pid_ns;
    __u32 tgid;
    __u32 pid;
    __s64 ret;
    __u32 id;
    __u64 dur_ns;
    char comm[TASK_COMM_LEN];
};

#endif
