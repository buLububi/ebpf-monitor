#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <container1> [container2] ..."
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[STEP] build net module"
bash scripts/build_net_multi.sh

mkdir -p runtime/net_multi
mkdir -p runtime/pids

for c in "$@"; do
  echo "[INFO] target container=$c"
  nohup bash scripts/run_net_multi_monitor.sh "$c" > "runtime/net_multi/${c}.log" 2>&1 &
  echo $! > "runtime/pids/net_${c}.pid"
  echo "[OK] started $c net monitor pid=$!"
done

echo "[OK] net_multi started"
