#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CONTAINER="${1:?container name required}"
PID="$(docker inspect -f '{{.State.Pid}}' "$CONTAINER")"
STATE_DIR="$ROOT_DIR/runtime/net_multi"
STATE_FILE="$STATE_DIR/${CONTAINER}.state"

mkdir -p "$STATE_DIR"

exec sudo "$ROOT_DIR/module03_net/net_multi_monitor" \
  --pid "$PID" \
  --container "$CONTAINER" \
  --state-file "$STATE_FILE"
