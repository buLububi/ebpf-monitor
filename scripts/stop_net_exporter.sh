#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -f runtime/pids/net_exporter.pid ]; then
  pid="$(cat runtime/pids/net_exporter.pid)"
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" || true
  fi
  rm -f runtime/pids/net_exporter.pid
fi

pkill -f net_exporter.py 2>/dev/null || true

echo "[OK] net exporter stopped"
