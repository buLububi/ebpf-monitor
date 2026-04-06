#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <docker-container-name-or-id>"
  exit 1
fi

CONTAINER="$1"
BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_DIR="$BASE_DIR/runtime/cpu_mem"
DEPLOY_DIR="$BASE_DIR/deploy"
MONITOR_PID_FILE="$STATE_DIR/monitor.pid"
EXPORTER_PID_FILE="$STATE_DIR/exporter.pid"

mkdir -p "$STATE_DIR"

docker inspect "$CONTAINER" >/dev/null 2>&1 || {
  echo "[ERR] container not found: $CONTAINER"
  exit 1
}

echo "[STEP] build module01_cpu_mem"
"$BASE_DIR/scripts/build_cpu_mem.sh"

if [[ -f "$MONITOR_PID_FILE" ]]; then
  OLD_PID=$(cat "$MONITOR_PID_FILE" || true)
  if [[ -n "${OLD_PID:-}" ]] && ps -p "$OLD_PID" >/dev/null 2>&1; then
    kill "$OLD_PID" || true
  fi
  rm -f "$MONITOR_PID_FILE"
fi

if [[ -f "$EXPORTER_PID_FILE" ]]; then
  OLD_PID=$(cat "$EXPORTER_PID_FILE" || true)
  if [[ -n "${OLD_PID:-}" ]] && ps -p "$OLD_PID" >/dev/null 2>&1; then
    kill "$OLD_PID" || true
  fi
  rm -f "$EXPORTER_PID_FILE"
fi

echo "[STEP] start cpu_mem monitor"
nohup "$BASE_DIR/scripts/run_cpu_mem_monitor.sh" "$CONTAINER" > "$STATE_DIR/monitor.out" 2>&1 &
echo $! > "$MONITOR_PID_FILE"

sleep 2

echo "[STEP] start exporter"
nohup python3 "$BASE_DIR/module05_http_exporter/cpu_mem_exporter.py" "$CONTAINER" "$STATE_DIR" > "$STATE_DIR/exporter.out" 2>&1 &
echo $! > "$EXPORTER_PID_FILE"

echo "[STEP] start Prometheus + Grafana"
cd "$DEPLOY_DIR"
if docker compose version >/dev/null 2>&1; then
  docker compose up -d
else
  docker-compose up -d
fi

echo
echo "[OK] module01_cpu_mem started"
echo "[INFO] container            : $CONTAINER"
echo "[INFO] console log         : $STATE_DIR/console.log"
echo "[INFO] monitor out         : $STATE_DIR/monitor.out"
echo "[INFO] exporter out        : $STATE_DIR/exporter.out"
echo "[INFO] metrics             : http://127.0.0.1:19100/metrics"
echo "[INFO] Prometheus          : http://127.0.0.1:9090"
echo "[INFO] Grafana            : http://127.0.0.1:3000"
echo "[INFO] Grafana login      : admin / admin"
