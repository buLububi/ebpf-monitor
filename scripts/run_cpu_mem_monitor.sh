#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <container_name_or_id>"
  exit 1
fi

CONTAINER="$1"
BASE_DIR="$HOME/experience/ebpf_container_monitor"
STATE_DIR="$BASE_DIR/runtime/cpu_mem"
mkdir -p "$STATE_DIR"

PID=$(docker inspect -f '{{.State.Pid}}' "$CONTAINER")
MONITOR="$BASE_DIR/module01_cpu_mem/cpu_mem_monitor"

if [[ -z "$PID" || "$PID" == "0" ]]; then
  echo "[ERR] invalid container pid"
  exit 1
fi

CGROUP_PATH=$(awk -F: '/^0::/ {print $3}' /proc/$PID/cgroup)
MNT_NS=$(stat -Lc '%i' /proc/$PID/ns/mnt)
PID_NS=$(stat -Lc '%i' /proc/$PID/ns/pid)
NET_NS=$(stat -Lc '%i' /proc/$PID/ns/net)

cat > "$STATE_DIR/meta.env" <<META
container=$CONTAINER
host_pid=$PID
cgroup_path=$CGROUP_PATH
mnt_ns=$MNT_NS
pid_ns=$PID_NS
net_ns=$NET_NS
META

echo "[INFO] target container=$CONTAINER pid=$PID"

stdbuf -oL -eL "$MONITOR" --pid "$PID" > "$STATE_DIR/console.log" 2>&1 &
MON_PID=$!
echo $MON_PID > "$STATE_DIR/monitor.pid"

# Background process to periodically update counts from console.log
(
  while kill -0 "$MON_PID" 2>/dev/null; do
    awk '
    /sched_switch[[:space:]]*:/       { ss=$3 }
    /sched_wakeup[[:space:]]*:/       { sw=$3 }
    /sched_wakeup_new[[:space:]]*:/   { swn=$3 }
    /sched_process_exit[[:space:]]*:/ { spe=$3 }
    /mm_page_alloc[[:space:]]*:/      { mpa=$3 }
    /mm_page_free[[:space:]]*:/       { mpf=$3 }
    /sched_latency_avg_us[[:space:]]*:/  { sla=$3 }
    /sched_latency_max_us[[:space:]]*:/  { slm=$3 }
    /sched_latency_last_us[[:space:]]*:/ { sll=$3 }
    END {
      if (ss == "") ss=0;
      if (sw == "") sw=0;
      if (swn == "") swn=0;
      if (spe == "") spe=0;
      if (mpa == "") mpa=0;
      if (mpf == "") mpf=0;
      if (sla == "") sla=0;
      if (slm == "") slm=0;
      if (sll == "") sll=0;
      print "sched_switch=" ss;
      print "sched_wakeup=" sw;
      print "sched_wakeup_new=" swn;
      print "sched_process_exit=" spe;
      print "mm_page_alloc=" mpa;
      print "mm_page_free=" mpf;
      print "sched_latency_avg_us=" sla;
      print "sched_latency_max_us=" slm;
      print "sched_latency_last_us=" sll;
    }' "$STATE_DIR/console.log" > "$STATE_DIR/cpu_mem_counts.prom.tmp"

    mv "$STATE_DIR/cpu_mem_counts.prom.tmp" "$STATE_DIR/cpu_mem_counts.prom"
    sleep 1
  done
) &
echo $! > "$STATE_DIR/parser.pid"

wait "$MON_PID"
