#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/multi_cpu_mem"
DEPLOY_DIR="$BASE_DIR/deploy"

if [[ -d "$STATE_ROOT" ]]; then
  find "$STATE_ROOT" -name monitor.pid | while read -r f; do
    PID=$(cat "$f" 2>/dev/null || true)
    if [[ -n "${PID:-}" ]] && ps -p "$PID" >/dev/null 2>&1; then
      kill "$PID" || true
    fi
    rm -f "$f"
  done
fi

if [[ -f "$STATE_ROOT/exporter.pid" ]]; then
  PID=$(cat "$STATE_ROOT/exporter.pid" 2>/dev/null || true)
  if [[ -n "${PID:-}" ]] && ps -p "$PID" >/dev/null 2>&1; then
    kill "$PID" || true
  fi
  rm -f "$STATE_ROOT/exporter.pid"
fi

cd "$DEPLOY_DIR"
docker compose down || true

echo "[OK] multi cpu_mem module stopped"
