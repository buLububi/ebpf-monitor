#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_DIR="$BASE_DIR/runtime/cpu_mem"
DEPLOY_DIR="$BASE_DIR/deploy"

for f in "$STATE_DIR/monitor.pid" "$STATE_DIR/exporter.pid"; do
  if [[ -f "$f" ]]; then
    PID=$(cat "$f" || true)
    if [[ -n "${PID:-}" ]] && ps -p "$PID" >/dev/null 2>&1; then
      kill "$PID" || true
    fi
    rm -f "$f"
  fi
done

cd "$DEPLOY_DIR"
if docker compose version >/dev/null 2>&1; then
  docker compose down || true
else
  docker-compose down || true
fi

echo "[OK] module01_cpu_mem stopped"
