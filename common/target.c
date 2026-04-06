#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "target.h"

static int read_cgroup_v2_path_from_proc(pid_t pid, char *out, size_t outsz)
{
    char p[64];
    snprintf(p, sizeof(p), "/proc/%d/cgroup", pid);

    FILE *f = fopen(p, "re");
    if (!f) {
        perror("fopen /proc/<pid>/cgroup");
        return -1;
    }

    char *line = NULL;
    size_t n = 0;
    int ok = -1;

    while (getline(&line, &n, f) != -1) {
        if (!strncmp(line, "0::", 3)) {
            char *path = line + 3;
            size_t len = strcspn(path, "\n");
            if (len >= outsz)
                len = outsz - 1;
            memcpy(out, path, len);
            out[len] = '\0';
            ok = 0;
            break;
        }
    }

    free(line);
    fclose(f);
    return ok;
}

static int get_cgroup_id_from_pid(pid_t pid, unsigned long long *out_id, char *cg_path, size_t cg_path_sz)
{
    char rel[PATH_MAX];
    if (read_cgroup_v2_path_from_proc(pid, rel, sizeof(rel)) < 0)
        return -1;

    snprintf(cg_path, cg_path_sz, "%s", rel);

    char abspath[PATH_MAX * 2];
    snprintf(abspath, sizeof(abspath), "/sys/fs/cgroup%s", rel);

    struct stat st;
    if (stat(abspath, &st) != 0) {
        perror("stat cgroup path");
        return -1;
    }

    *out_id = (unsigned long long)st.st_ino;
    return 0;
}

static int get_ns_stat_from_pid(pid_t pid, const char *ns_name,
                                unsigned long long *out_ino,
                                unsigned long long *out_dev)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof(path), "/proc/%d/ns/%s", pid, ns_name);
    if (stat(path, &st) != 0) {
        perror("stat namespace link");
        return -1;
    }

    if (out_ino)
        *out_ino = (unsigned long long)st.st_ino;
    if (out_dev)
        *out_dev = (unsigned long long)st.st_dev;

    return 0;
}

int load_target_info(pid_t pid, struct target_info *ti)
{
    memset(ti, 0, sizeof(*ti));
    ti->pid = pid;

    if (get_cgroup_id_from_pid(pid, &ti->cgroup_id, ti->cgroup_path, sizeof(ti->cgroup_path)) != 0)
        return -1;
    if (get_ns_stat_from_pid(pid, "pid", &ti->pid_ns_id, &ti->pid_ns_dev) != 0)
        return -1;
    if (get_ns_stat_from_pid(pid, "mnt", &ti->mnt_ns_id, NULL) != 0)
        return -1;
    if (get_ns_stat_from_pid(pid, "net", &ti->net_ns_id, NULL) != 0)
        return -1;

    return 0;
}

void print_target_info(const struct target_info *ti)
{
    printf("========== TARGET ==========\n");
    printf("PID            : %d\n", ti->pid);
    printf("cgroup path    : %s\n", ti->cgroup_path);
    printf("cgroup id      : %llu\n", ti->cgroup_id);
    printf("pid ns id      : %llu\n", ti->pid_ns_id);
    printf("pid ns dev     : %llu\n", ti->pid_ns_dev);
    printf("mnt ns id      : %llu\n", ti->mnt_ns_id);
    printf("net ns id      : %llu\n", ti->net_ns_id);
    printf("============================\n");
}
