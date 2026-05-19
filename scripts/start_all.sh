#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
DEPLOY_DIR="$BASE_DIR/deploy"
TARGET_CONTAINERS=(test-redis test-nginx)

ensure_demo_container() {
  local name="$1"
  local image="$2"
  local ports=("${@:3}")

  if docker inspect "$name" >/dev/null 2>&1; then
    local running
    running="$(docker inspect -f '{{.State.Running}}' "$name")"
    if [[ "$running" != "true" ]]; then
      echo "[INFO] 启动已存在的目标容器: $name"
      docker start "$name" >/dev/null
    fi
  else
    echo "[INFO] 创建目标容器: $name"
    docker run -d --name "$name" "${ports[@]}" "$image" >/dev/null
  fi

  local pid
  pid="$(docker inspect -f '{{.State.Pid}}' "$name")"
  if [[ -z "$pid" || "$pid" == "0" ]]; then
    echo "[ERR] 目标容器未运行或 PID 无效: $name"
    exit 1
  fi

  echo "[OK] target $name pid=$pid"
}

cd "$BASE_DIR"

echo "[1/5] 启动 Prometheus + Grafana + Alertmanager"
cd "$DEPLOY_DIR"
docker compose up -d

cd "$BASE_DIR"

echo "[1.5/5] 准备被监控目标容器"
ensure_demo_container test-redis redis:latest -p 6379:6379
ensure_demo_container test-nginx nginx:latest -p 8080:80

echo "[2/5] 启动 CPU/MEM 多容器监控"
bash scripts/start_cpu_mem_multi.sh "${TARGET_CONTAINERS[@]}"

echo "[3/5] 启动 IO 多容器监控"
bash scripts/start_io_multi.sh "${TARGET_CONTAINERS[@]}"
bash scripts/start_io_exporter.sh

echo "[3.5/5] 启动 NET 多容器监控"
bash scripts/start_net_multi.sh "${TARGET_CONTAINERS[@]}"
bash scripts/start_net_exporter.sh

echo "[4/5] 健康检查"
echo "--- docker services ---"
docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}"

echo "--- cpu metrics ---"
curl --noproxy '*' -s http://127.0.0.1:19100/metrics | grep 'container_cpu_usage_usec' || echo "NO_CPU_METRIC"

echo "--- io metrics ---"
curl --noproxy '*' -s http://127.0.0.1:2114/metrics | grep 'container_io' || echo "NO_IO_METRIC"

echo "--- net metrics ---"
curl --noproxy '*' -s http://127.0.0.1:2115/metrics | grep 'container_net' || echo "NO_NET_METRIC"

echo "--- alertmanager ---"
curl --noproxy '*' -s http://127.0.0.1:9093/-/ready || echo "ALERTMANAGER_NOT_READY"

echo "[5/5] 启动完成"
echo "Grafana      : http://127.0.0.1:3000"
echo "Prometheus   : http://127.0.0.1:9090"
echo "Alertmanager : http://127.0.0.1:9093"
