#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -I./common -c module03_net/net_multi.bpf.c -o module03_net/net_multi.bpf.o

bpftool gen skeleton module03_net/net_multi.bpf.o > module03_net/net_multi.skel.h

cc -O2 -g \
  -I./common \
  -I./module03_net \
  -I/usr/include \
  module03_net/net_multi.c common/target.c \
  -lbpf -lelf -lz \
  -o module03_net/net_multi_monitor

echo "[OK] build net_multi done"
