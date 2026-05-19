#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p runtime/pids runtime/net_multi

nohup python3 module05_http_exporter/net_exporter.py > runtime/net_multi/exporter.log 2>&1 &
echo $! > runtime/pids/net_exporter.pid

echo "[OK] net exporter started pid=$!"
echo "[INFO] metrics: http://127.0.0.1:2115/metrics"
