#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <container1> [container2] [container3] ..."
  exit 1
fi

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/multi_cpu_mem"
DEPLOY_DIR="$BASE_DIR/deploy"
EXPORTER_PID_FILE="$STATE_ROOT/exporter.pid"

mkdir -p "$STATE_ROOT"

echo "[STEP] build multi cpu_mem module"
cd "$BASE_DIR"
make clean
./scripts/build_cpu_mem.sh

echo "[STEP] stop old processes"
"$BASE_DIR/scripts/stop_multi_cpu_mem_module.sh" || true

echo "[STEP] start per-container monitors"
"$BASE_DIR/scripts/run_multi_cpu_mem_monitor.sh" "$@"

echo "[STEP] start multi exporter"
nohup python3 "$BASE_DIR/module05_http_exporter/multi_cpu_mem_exporter.py" > "$STATE_ROOT/exporter.out" 2>&1 < /dev/null &
echo $! > "$EXPORTER_PID_FILE"

echo "[STEP] start Prometheus + Grafana"
cd "$DEPLOY_DIR"
docker compose up -d

echo
echo "[OK] multi cpu_mem module started"
echo "[INFO] containers          : $*"
echo "[INFO] exporter out        : $STATE_ROOT/exporter.out"
echo "[INFO] metrics             : http://127.0.0.1:19100/metrics"
echo "[INFO] Prometheus          : http://127.0.0.1:9090"
echo "[INFO] Grafana             : http://127.0.0.1:3000"
