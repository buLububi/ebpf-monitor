#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

bpftool btf dump file /sys/kernel/btf/vmlinux format c > module04_syscall/vmlinux.h

make clean
make
