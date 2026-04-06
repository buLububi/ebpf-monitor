#!/usr/bin/env bash
set -euo pipefail

CONTAINER="${1:?usage: $0 <container>}"
docker update --cpus=0 "$CONTAINER"
echo "[OK] recovered $CONTAINER cpu limit"
