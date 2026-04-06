#!/usr/bin/env bash
set -euo pipefail

REQS="${1:-100000}"
CONC="${2:-50}"

docker exec test-redis redis-benchmark -n "$REQS" -c "$CONC"
