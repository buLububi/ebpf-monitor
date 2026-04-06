#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/cpu_mem_multi"

if [[ -d "$STATE_ROOT" ]]; then
  find "$STATE_ROOT" -name monitor.pid -type f | while read -r f; do
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

echo "[OK] multi cpu_mem stopped"
