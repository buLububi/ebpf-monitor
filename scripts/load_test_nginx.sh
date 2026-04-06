#!/usr/bin/env bash
set -euo pipefail

URL="${1:-http://127.0.0.1:8080/}"
REQS="${2:-50000}"
CONC="${3:-200}"

echo "[INFO] 开始持续压测 nginx..."
echo "[INFO] URL=$URL 并发=$CONC"

while true; do
  ab -n "$REQS" -c "$CONC" "$URL" >/dev/null 2>&1
  sleep 1
done
