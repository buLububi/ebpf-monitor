#ifndef __TARGET_H
#define __TARGET_H

#include <linux/limits.h>
#include <sys/types.h>

struct target_info {
    pid_t pid;
    unsigned long long cgroup_id;
    unsigned long long pid_ns_id;
    unsigned long long pid_ns_dev;
    unsigned long long mnt_ns_id;
    unsigned long long net_ns_id;
    char cgroup_path[PATH_MAX];
};

int load_target_info(pid_t pid, struct target_info *ti);
void print_target_info(const struct target_info *ti);

#endif
