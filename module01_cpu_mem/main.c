#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include "../common/target.h"

static int read_ull_from_file(const char *path, unsigned long long *v)
{
    FILE *f = fopen(path, "re");
    if (!f) return -1;
    if (fscanf(f, "%llu", v) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int read_cpu_stat(const char *cg_path,
                         unsigned long long *usage_usec,
                         unsigned long long *user_usec,
                         unsigned long long *system_usec,
                         unsigned long long *nr_periods,
                         unsigned long long *nr_throttled,
                         unsigned long long *throttled_usec)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/cpu.stat", cg_path);

    FILE *f = fopen(path, "re");
    if (!f) return -1;

    char key[128];
    unsigned long long val;
    while (fscanf(f, "%127s %llu", key, &val) == 2) {
        if (strcmp(key, "usage_usec") == 0) *usage_usec = val;
        else if (strcmp(key, "user_usec") == 0) *user_usec = val;
        else if (strcmp(key, "system_usec") == 0) *system_usec = val;
        else if (strcmp(key, "nr_periods") == 0) *nr_periods = val;
        else if (strcmp(key, "nr_throttled") == 0) *nr_throttled = val;
        else if (strcmp(key, "throttled_usec") == 0) *throttled_usec = val;
    }

    fclose(f);
    return 0;
}

static void print_mem_info(const char *cg_path)
{
    char path[PATH_MAX];
    unsigned long long memory_current = 0;

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.current", cg_path);
    if (read_ull_from_file(path, &memory_current) == 0)
        printf("memory.current      : %llu bytes\n", memory_current);

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.max", cg_path);
    FILE *f = fopen(path, "re");
    if (f) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), f))
            printf("memory.max          : %s", buf);
        fclose(f);
    }

    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.swap.current", cg_path);
    if (read_ull_from_file(path, &memory_current) == 0)
        printf("memory.swap.current : %llu bytes\n", memory_current);
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "--pid") != 0) {
        fprintf(stderr, "Usage: %s --pid <host-pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)strtol(argv[2], NULL, 10);
    struct target_info ti;

    if (load_target_info(pid, &ti) != 0) {
        fprintf(stderr, "load_target_info failed\n");
        return 1;
    }

    print_target_info(&ti);

    while (1) {
        unsigned long long usage_usec = 0, user_usec = 0, system_usec = 0;
        unsigned long long nr_periods = 0, nr_throttled = 0, throttled_usec = 0;

        if (read_cpu_stat(ti.cgroup_path,
                          &usage_usec, &user_usec, &system_usec,
                          &nr_periods, &nr_throttled, &throttled_usec) != 0) {
            fprintf(stderr, "read cpu.stat failed\n");
            return 1;
        }

        printf("\n===== CPU / MEM SNAPSHOT =====\n");
        printf("cpu.usage_usec      : %llu\n", usage_usec);
        printf("cpu.user_usec       : %llu\n", user_usec);
        printf("cpu.system_usec     : %llu\n", system_usec);
        printf("cpu.nr_periods      : %llu\n", nr_periods);
        printf("cpu.nr_throttled    : %llu\n", nr_throttled);
        printf("cpu.throttled_usec  : %llu\n", throttled_usec);
        print_mem_info(ti.cgroup_path);
        printf("==============================\n");

        sleep(1);
    }

    return 0;
}
