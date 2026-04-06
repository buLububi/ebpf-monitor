#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <container1> [container2] [container3] ..."
  exit 1
fi

BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_ROOT="$BASE_DIR/runtime/multi_cpu_mem"
MONITOR="$BASE_DIR/module01_cpu_mem/cpu_mem_monitor"

mkdir -p "$STATE_ROOT"

for CONTAINER in "$@"; do
  PID=$(docker inspect -f '{{.State.Pid}}' "$CONTAINER")
  if [[ -z "$PID" || "$PID" == "0" ]]; then
    echo "[ERR] invalid pid for container=$CONTAINER"
    continue
  fi

  DIR="$STATE_ROOT/$CONTAINER"
  mkdir -p "$DIR"

  echo "[INFO] start monitor for $CONTAINER pid=$PID"

  nohup "$MONITOR" \
    --pid "$PID" \
    --container "$CONTAINER" \
    --state-file "$DIR/state.env" \
    > "$DIR/monitor.out" 2>&1 < /dev/null &

  echo $! > "$DIR/monitor.pid"
done
