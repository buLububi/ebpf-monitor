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

echo "[STEP] check sudo permission for io eBPF monitor"
sudo -v

mkdir -p runtime/io_multi
mkdir -p runtime/pids
rm -f runtime/io_multi/*.state

for c in "$@"; do
  echo "[INFO] target container=$c"
  PID="$(docker inspect -f '{{.State.Pid}}' "$c")"
  if [ -z "$PID" ] || [ "$PID" = "0" ]; then
    echo "[ERR] invalid container pid: $c"
    exit 1
  fi
  STATE_FILE="$ROOT_DIR/runtime/io_multi/${c}.state"
  nohup sudo -n "$ROOT_DIR/module02_io/io_multi_monitor" \
    --pid "$PID" \
    --container "$c" \
    --state-file "$STATE_FILE" \
    > "runtime/io_multi/${c}.log" 2>&1 < /dev/null &
  echo $! > "runtime/pids/io_${c}.pid"
  echo "[OK] started $c io monitor pid=$! target_pid=$PID"
done

echo "[OK] io_multi started"
