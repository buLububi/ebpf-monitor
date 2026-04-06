#!/usr/bin/env python3
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

HOST = "0.0.0.0"
PORT = 19100

ATTACH_POINTS = [
    "tracepoint/sched/sched_switch",
    "tracepoint/sched/sched_wakeup",
    "tracepoint/sched/sched_wakeup_new",
    "tracepoint/sched/sched_process_exit",
    "tracepoint/kmem/mm_page_alloc",
    "tracepoint/kmem/mm_page_free",
]

def read_text(path):
    with open(path, "r") as f:
        return f.read().strip()

def read_int_or_zero(path):
    try:
        s = read_text(path).strip()
        if s == "":
            return 0
        return int(s)
    except:
        return 0

def parse_kv_file(path):
    data = {}
    if not os.path.exists(path):
        return data
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            k, v = line.split("=", 1)
            data[k.strip()] = v.strip()
    return data

def read_cpu_stat(cg_path):
    path = f"/sys/fs/cgroup{cg_path}/cpu.stat"
    data = {
        "usage_usec": 0,
        "user_usec": 0,
        "system_usec": 0,
        "nr_periods": 0,
        "nr_throttled": 0,
        "throttled_usec": 0,
    }
    with open(path, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) == 2 and parts[0] in data:
                try:
                    data[parts[0]] = int(parts[1])
                except:
                    data[parts[0]] = 0
    return data

def read_mem_stat(cg_path):
    base = f"/sys/fs/cgroup{cg_path}"
    mem_current = read_int_or_zero(f"{base}/memory.current")
    mem_swap_current = read_int_or_zero(f"{base}/memory.swap.current")
    try:
        mem_max_raw = read_text(f"{base}/memory.max").strip()
        mem_max = -1 if mem_max_raw == "max" else int(mem_max_raw) if mem_max_raw else -1
    except:
        mem_max = -1
    return {
        "memory_current": mem_current,
        "memory_swap_current": mem_swap_current,
        "memory_max": mem_max,
    }

def parse_latest_counts_from_console(console_path):
    result = {
        "sched_switch": 0.0,
        "sched_wakeup": 0.0,
        "sched_wakeup_new": 0.0,
        "sched_process_exit": 0.0,
        "mm_page_alloc": 0.0,
        "mm_page_free": 0.0,
        "sched_latency_avg_us": 0.0,
        "sched_latency_max_us": 0.0,
        "sched_latency_last_us": 0.0,
    }

    if not os.path.exists(console_path):
        return result

    try:
        with open(console_path, "r") as f:
            lines = f.readlines()

        # 只看最后 200 行，够用
        lines = lines[-200:]

        for line in lines:
            s = line.strip()
            if s.startswith("sched_switch"):
                result["sched_switch"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_wakeup_new"):
                result["sched_wakeup_new"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_wakeup"):
                result["sched_wakeup"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_process_exit"):
                result["sched_process_exit"] = float(s.split(":")[-1].strip())
            elif s.startswith("mm_page_alloc"):
                result["mm_page_alloc"] = float(s.split(":")[-1].strip())
            elif s.startswith("mm_page_free"):
                result["mm_page_free"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_latency_avg_us"):
                result["sched_latency_avg_us"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_latency_max_us"):
                result["sched_latency_max_us"] = float(s.split(":")[-1].strip())
            elif s.startswith("sched_latency_last_us"):
                result["sched_latency_last_us"] = float(s.split(":")[-1].strip())
    except:
        pass

    return result

def collect_metrics(base_dir):
    lines = []

    lines.append("# HELP ebpf_module_attach_points_total Number of attach points in cpu_mem module")
    lines.append("# TYPE ebpf_module_attach_points_total gauge")
    lines.append(f'ebpf_module_attach_points_total{{module="cpu_mem"}} {len(ATTACH_POINTS)}')

    for ap in ATTACH_POINTS:
        lines.append("# HELP ebpf_attach_point_info Attach point info")
        lines.append("# TYPE ebpf_attach_point_info gauge")
        lines.append(f'ebpf_attach_point_info{{module="cpu_mem",attach_point="{ap}"}} 1')

    meta = parse_kv_file(os.path.join(base_dir, "meta.env"))
    if not meta:
        return "collector_error{msg=\"meta_env_not_found\"} 1\n"

    container = meta.get("container", "unknown")
    host_pid = meta.get("host_pid", "0")
    cg_path = meta.get("cgroup_path", "/")
    mnt_ns = meta.get("mnt_ns", "0")
    pid_ns = meta.get("pid_ns", "0")
    net_ns = meta.get("net_ns", "0")

    cpu = read_cpu_stat(cg_path)
    mem = read_mem_stat(cg_path)
    counts = parse_latest_counts_from_console(os.path.join(base_dir, "console.log"))

    labels = (
        f'container="{container}",'
        f'host_pid="{host_pid}",'
        f'cgroup_path="{cg_path}",'
        f'mnt_ns="{mnt_ns}",'
        f'pid_ns="{pid_ns}",'
        f'net_ns="{net_ns}"'
    )

    lines.append("# HELP container_cpu_usage_usec Container CPU usage in microseconds")
    lines.append("# TYPE container_cpu_usage_usec gauge")
    lines.append(f"container_cpu_usage_usec{{{labels}}} {cpu['usage_usec']}")

    lines.append("# HELP container_cpu_user_usec Container CPU user time in microseconds")
    lines.append("# TYPE container_cpu_user_usec gauge")
    lines.append(f"container_cpu_user_usec{{{labels}}} {cpu['user_usec']}")

    lines.append("# HELP container_cpu_system_usec Container CPU system time in microseconds")
    lines.append("# TYPE container_cpu_system_usec gauge")
    lines.append(f"container_cpu_system_usec{{{labels}}} {cpu['system_usec']}")

    lines.append("# HELP container_cpu_nr_periods CFS periods")
    lines.append("# TYPE container_cpu_nr_periods gauge")
    lines.append(f"container_cpu_nr_periods{{{labels}}} {cpu['nr_periods']}")

    lines.append("# HELP container_cpu_nr_throttled Throttled periods")
    lines.append("# TYPE container_cpu_nr_throttled gauge")
    lines.append(f"container_cpu_nr_throttled{{{labels}}} {cpu['nr_throttled']}")

    lines.append("# HELP container_cpu_throttled_usec Throttled time in microseconds")
    lines.append("# TYPE container_cpu_throttled_usec gauge")
    lines.append(f"container_cpu_throttled_usec{{{labels}}} {cpu['throttled_usec']}")

    lines.append("# HELP container_memory_current_bytes Container memory usage in bytes")
    lines.append("# TYPE container_memory_current_bytes gauge")
    lines.append(f"container_memory_current_bytes{{{labels}}} {mem['memory_current']}")

    lines.append("# HELP container_memory_swap_current_bytes Container swap usage in bytes")
    lines.append("# TYPE container_memory_swap_current_bytes gauge")
    lines.append(f"container_memory_swap_current_bytes{{{labels}}} {mem['memory_swap_current']}")

    lines.append("# HELP container_memory_max_bytes Container memory max bytes")
    lines.append("# TYPE container_memory_max_bytes gauge")
    lines.append(f"container_memory_max_bytes{{{labels}}} {mem['memory_max']}")

    lines.append("# HELP container_sched_switch_total sched_switch count")
    lines.append("# TYPE container_sched_switch_total gauge")
    lines.append(f"container_sched_switch_total{{{labels}}} {counts['sched_switch']}")

    lines.append("# HELP container_sched_wakeup_total sched_wakeup count")
    lines.append("# TYPE container_sched_wakeup_total gauge")
    lines.append(f"container_sched_wakeup_total{{{labels}}} {counts['sched_wakeup']}")

    lines.append("# HELP container_sched_wakeup_new_total sched_wakeup_new count")
    lines.append("# TYPE container_sched_wakeup_new_total gauge")
    lines.append(f"container_sched_wakeup_new_total{{{labels}}} {counts['sched_wakeup_new']}")

    lines.append("# HELP container_sched_process_exit_total sched_process_exit count")
    lines.append("# TYPE container_sched_process_exit_total gauge")
    lines.append(f"container_sched_process_exit_total{{{labels}}} {counts['sched_process_exit']}")

    lines.append("# HELP container_mm_page_alloc_total mm_page_alloc count")
    lines.append("# TYPE container_mm_page_alloc_total gauge")
    lines.append(f"container_mm_page_alloc_total{{{labels}}} {counts['mm_page_alloc']}")

    lines.append("# HELP container_mm_page_free_total mm_page_free count")
    lines.append("# TYPE container_mm_page_free_total gauge")
    lines.append(f"container_mm_page_free_total{{{labels}}} {counts['mm_page_free']}")

    lines.append("# HELP container_sched_latency_avg_us Average scheduling latency in microseconds")
    lines.append("# TYPE container_sched_latency_avg_us gauge")
    lines.append(f"container_sched_latency_avg_us{{{labels}}} {counts['sched_latency_avg_us']}")

    lines.append("# HELP container_sched_latency_max_us Max scheduling latency in microseconds")
    lines.append("# TYPE container_sched_latency_max_us gauge")
    lines.append(f"container_sched_latency_max_us{{{labels}}} {counts['sched_latency_max_us']}")

    lines.append("# HELP container_sched_latency_last_us Last scheduling latency in microseconds")
    lines.append("# TYPE container_sched_latency_last_us gauge")
    lines.append(f"container_sched_latency_last_us{{{labels}}} {counts['sched_latency_last_us']}")

    return "\n".join(lines) + "\n"

class Handler(BaseHTTPRequestHandler):
    state_dir = None

    def do_GET(self):
        if self.path == "/metrics":
            try:
                body = collect_metrics(self.state_dir).encode()
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except Exception as e:
                body = f'collector_error{{msg="{str(e)}"}} 1\n'.encode()
                self.send_response(500)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
        else:
            body = b"ok\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <container_name_or_id> <state_dir>")
        sys.exit(1)

    Handler.state_dir = sys.argv[2]
    server = HTTPServer((HOST, PORT), Handler)
    print(f"[OK] exporter listening on http://{HOST}:{PORT}/metrics")
    server.serve_forever()
