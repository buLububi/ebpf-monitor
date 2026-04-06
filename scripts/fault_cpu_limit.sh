#!/usr/bin/env bash
set -euo pipefail

CONTAINER="${1:?usage: $0 <container> [cpus]}"
CPUS="${2:-0.5}"

docker update --cpus="$CPUS" "$CONTAINER"
echo "[OK] set $CONTAINER cpu limit to $CPUS"
