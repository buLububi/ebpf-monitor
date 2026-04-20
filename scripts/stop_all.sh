#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
DEPLOY_DIR="$BASE_DIR/deploy"

cd "$BASE_DIR"

echo "[1/3] 停止 eBPF 监控进程"
bash scripts/stop_cpu_mem_multi.sh || true
bash scripts/stop_io_multi.sh || true

pkill -f cpu_mem_multi_exporter.py || true
sudo pkill -f cpu_mem_multi_monitor || true
pkill -f io_exporter.py || true
pkill -f io_multi || true

echo "[2/3] 停止容器服务"
cd "$DEPLOY_DIR"
docker compose down

echo "[3/3] 已停止"