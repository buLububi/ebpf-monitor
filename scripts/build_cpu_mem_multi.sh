#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
cd "$BASE_DIR"

clang -O2 -g -target bpf -D__TARGET_ARCH_x86 \
  -Icommon -Imodule01_cpu_mem \
  -c module01_cpu_mem/cpu_mem_multi.bpf.c \
  -o module01_cpu_mem/cpu_mem_multi.bpf.o

bpftool gen skeleton module01_cpu_mem/cpu_mem_multi.bpf.o > module01_cpu_mem/cpu_mem_multi.skel.h

gcc -O2 -g \
  module01_cpu_mem/cpu_mem_multi.c common/target.c \
  -o module01_cpu_mem/cpu_mem_multi_monitor \
  -Icommon -Imodule01_cpu_mem \
  -lbpf -lelf -lz
