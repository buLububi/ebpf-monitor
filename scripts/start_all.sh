#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
DEPLOY_DIR="$BASE_DIR/deploy"

cd "$BASE_DIR"

echo "[1/5] 启动 Prometheus + Grafana + Alertmanager"
cd "$DEPLOY_DIR"
docker compose up -d

cd "$BASE_DIR"

echo "[2/5] 启动 CPU/MEM 多容器监控"
bash scripts/start_cpu_mem_multi.sh test-redis test-nginx

echo "[3/5] 启动 IO 多容器监控"
bash scripts/start_io_multi.sh

echo "[4/5] 健康检查"
echo "--- docker services ---"
docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}"

echo "--- cpu metrics ---"
curl --noproxy '*' -s http://127.0.0.1:19100/metrics | grep 'container_cpu_usage_usec' || echo "NO_CPU_METRIC"

echo "--- io metrics ---"
curl --noproxy '*' -s http://127.0.0.1:2114/metrics | grep 'container_io' || echo "NO_IO_METRIC"

echo "--- alertmanager ---"
curl --noproxy '*' -s http://127.0.0.1:9093/-/ready || echo "ALERTMANAGER_NOT_READY"

echo "[5/5] 启动完成"
echo "Grafana      : http://127.0.0.1:3000"
echo "Prometheus   : http://127.0.0.1:9090"
echo "Alertmanager : http://127.0.0.1:9093"