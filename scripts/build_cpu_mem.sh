#!/usr/bin/env bash
set -euo pipefail

cd "$HOME/experience/ebpf_container_monitor"
bpftool btf dump file /sys/kernel/btf/vmlinux format c > module01_cpu_mem/vmlinux.h
make clean
make
