#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <docker-container-id-or-name>"
  exit 1
fi

docker inspect -f '{{.State.Pid}}' "$1"
