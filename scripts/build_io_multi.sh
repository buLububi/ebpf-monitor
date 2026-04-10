#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -I./common -c module02_io/io_multi.bpf.c -o module02_io/io_multi.bpf.o

bpftool gen skeleton module02_io/io_multi.bpf.o > module02_io/io_multi.skel.h

cc -O2 -g \
  -I./common \
  -I./module02_io \
  -I/usr/include \
  module02_io/io_multi.c common/target.c \
  -lbpf -lelf -lz \
  -o module02_io/io_multi_monitor

echo "[OK] build io_multi done"
