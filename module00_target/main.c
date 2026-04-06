#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/target.h"

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
    return 0;
}
