#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -f runtime/pids/io_exporter.pid ]; then
  pid="$(cat runtime/pids/io_exporter.pid)"
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" || true
  fi
  rm -f runtime/pids/io_exporter.pid
fi

pkill -f io_exporter.py 2>/dev/null || true
sudo -v
sudo -n pkill -f io_exporter.py 2>/dev/null || true

echo "[OK] io exporter stopped"
