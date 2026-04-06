#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <container1> [container2] ..."
  exit 1
fi

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/cpu_mem_multi"
DEPLOY_DIR="$BASE_DIR/deploy"
EXPORTER_PID_FILE="$STATE_ROOT/exporter.pid"

mkdir -p "$STATE_ROOT"

# 为了避免 19100 端口冲突，先停旧版运行态；旧文件不会被删
if [[ -x "$BASE_DIR/scripts/stop_cpu_mem_module.sh" ]]; then
  "$BASE_DIR/scripts/stop_cpu_mem_module.sh" >/dev/null 2>&1 || true
fi

# 停旧 multi
"$BASE_DIR/scripts/stop_cpu_mem_multi.sh" >/dev/null 2>&1 || true

echo "[STEP] build multi module"
"$BASE_DIR/scripts/build_cpu_mem_multi.sh"

echo "[STEP] start per-container monitors"
for CONTAINER in "$@"; do
  docker inspect "$CONTAINER" >/dev/null 2>&1 || {
    echo "[ERR] container not found: $CONTAINER"
    exit 1
  }
  "$BASE_DIR/scripts/run_cpu_mem_multi_monitor.sh" "$CONTAINER"
done

echo "[STEP] start multi exporter"
nohup python3 "$BASE_DIR/module05_http_exporter/cpu_mem_multi_exporter.py" \
  > "$STATE_ROOT/exporter.out" 2>&1 < /dev/null &
echo $! > "$EXPORTER_PID_FILE"

echo "[STEP] start Prometheus + Grafana"
cd "$DEPLOY_DIR"
docker compose up -d

echo
echo "[OK] multi cpu_mem started"
echo "[INFO] containers : $*"
echo "[INFO] metrics    : http://127.0.0.1:19100/metrics"
echo "[INFO] prometheus : http://127.0.0.1:9090"
echo "[INFO] grafana    : http://127.0.0.1:3000"
