#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p runtime/pids runtime/io_multi

nohup python3 module05_http_exporter/io_exporter.py > runtime/io_multi/exporter.log 2>&1 &
echo $! > runtime/pids/io_exporter.pid

echo "[OK] io exporter started pid=$!"
echo "[INFO] metrics: http://127.0.0.1:2114/metrics"
