#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -d runtime/pids ]; then
  for f in runtime/pids/io_*.pid; do
    [ -e "$f" ] || continue
    pid="$(cat "$f")"
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" || true
    fi
    rm -f "$f"
  done
fi

pkill -f io_multi_monitor 2>/dev/null || true

echo "[OK] io_multi stopped"
