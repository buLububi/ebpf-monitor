#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <container_name_or_id>"
  exit 1
fi

CONTAINER="$1"
BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/cpu_mem_multi"
STATE_DIR="$STATE_ROOT/$CONTAINER"
mkdir -p "$STATE_DIR"

PID=$(docker inspect -f '{{.State.Pid}}' "$CONTAINER")
MONITOR="$BASE_DIR/module01_cpu_mem/cpu_mem_multi_monitor"
STATE_FILE="$STATE_DIR/state.env"

if [[ -z "$PID" || "$PID" == "0" ]]; then
  echo "[ERR] invalid container pid"
  exit 1
fi

echo "[INFO] target container=$CONTAINER pid=$PID"

nohup "$MONITOR" \
  --pid "$PID" \
  --container "$CONTAINER" \
  --state-file "$STATE_FILE" \
  > "$STATE_DIR/console.log" 2>&1 < /dev/null &

echo $! > "$STATE_DIR/monitor.pid"
echo "[OK] started $CONTAINER monitor pid=$(cat "$STATE_DIR/monitor.pid")"
