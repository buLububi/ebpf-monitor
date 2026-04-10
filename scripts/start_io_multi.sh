#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <container1> [container2] ..."
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[STEP] build io module"
bash scripts/build_io_multi.sh

mkdir -p runtime/io_multi
mkdir -p runtime/pids

for c in "$@"; do
  echo "[INFO] target container=$c"
  nohup bash scripts/run_io_multi_monitor.sh "$c" > "runtime/io_multi/${c}.log" 2>&1 &
  echo $! > "runtime/pids/io_${c}.pid"
  echo "[OK] started $c io monitor pid=$!"
done

echo "[OK] io_multi started"
