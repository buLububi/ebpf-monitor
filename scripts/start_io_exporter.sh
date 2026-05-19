#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p runtime/pids runtime/io_multi

if [ -f runtime/pids/io_exporter.pid ]; then
  old_pid="$(cat runtime/pids/io_exporter.pid)"
  if kill -0 "$old_pid" 2>/dev/null; then
    kill "$old_pid" || true
  fi
  rm -f runtime/pids/io_exporter.pid
fi
pkill -f io_exporter.py 2>/dev/null || true
sudo -v
sudo -n pkill -f io_exporter.py 2>/dev/null || true

nohup python3 module05_http_exporter/io_exporter.py > runtime/io_multi/exporter.log 2>&1 &
echo $! > runtime/pids/io_exporter.pid

echo "[OK] io exporter started pid=$!"
echo "[INFO] metrics: http://127.0.0.1:2114/metrics"
