#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CONTAINER="${1:?container name required}"
PID="$(docker inspect -f '{{.State.Pid}}' "$CONTAINER")"
STATE_DIR="$ROOT_DIR/runtime/io_multi"
STATE_FILE="$STATE_DIR/${CONTAINER}.state"

mkdir -p "$STATE_DIR"

exec "$ROOT_DIR/module02_io/io_multi_monitor" \
  --pid "$PID" \
  --container "$CONTAINER" \
  --state-file "$STATE_FILE"
