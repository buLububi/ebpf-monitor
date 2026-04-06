#!/usr/bin/env python3
import os
import time
import subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer

HOST = "0.0.0.0"
PORT = 19100

BASE_DIR = os.path.expanduser("~/experience/ebpf_container_monitor")
STATE_ROOT = os.path.join(BASE_DIR, "runtime", "cpu_mem_multi")

ATTACH_POINTS = [
    "tracepoint/sched/sched_switch",
    "tracepoint/sched/sched_wakeup",
    "tracepoint/sched/sched_wakeup_new",
    "tracepoint/sched/sched_process_exit",
    "tracepoint/kmem/mm_page_alloc",
    "tracepoint/kmem/mm_page_free",
]

ROLE_CACHE = {}
ROLE_CACHE_TTL = 30

def escape_label_value(v: str) -> str:
    return str(v).replace("\\", "\\\\").replace('"', '\\"')

def read_state(path):
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

def as_int(d, k, default=0):
    try:
        return int(float(d.get(k, default)))
    except:
        return default

def as_float(d, k, default=0.0):
    try:
        return float(d.get(k, default))
    except:
        return default

def docker_inspect_field(container, fmt):
    try:
        out = subprocess.check_output(
            ["docker", "inspect", "--format", fmt, container],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=3
        ).strip()
        return out
    except Exception:
        return ""

def infer_role(container):
    now = time.time()
    cached = ROLE_CACHE.get(container)
    if cached and now - cached["ts"] < ROLE_CACHE_TTL:
        return cached["role"]

    image = docker_inspect_field(container, "{{.Config.Image}}").lower()
    name = container.lower()
    text = f"{image} {name}"

    web_keywords = [
        "nginx", "apache", "httpd", "openresty", "caddy",
        "traefik", "envoy", "haproxy", "tomcat"
    ]
    cache_db_keywords = [
        "redis", "mysql", "mariadb", "postgres", "mongodb",
        "mongo", "memcached", "elasticsearch"
    ]
    queue_keywords = [
        "rabbitmq", "kafka", "nats"
    ]
    batch_keywords = [
        "worker", "job", "cron", "spark", "flink"
    ]

    role = "generic"
    if any(k in text for k in web_keywords):
        role = "web"
    elif any(k in text for k in cache_db_keywords):
        role = "cache_db"
    elif any(k in text for k in queue_keywords):
        role = "queue"
    elif any(k in text for k in batch_keywords):
        role = "batch"

    ROLE_CACHE[container] = {"role": role, "ts": now}
    return role

def collect_metrics():
    lines = []

    lines.append("# HELP ebpf_module_attach_points_total Number of attach points in cpu_mem multi module")
    lines.append("# TYPE ebpf_module_attach_points_total gauge")
    lines.append(f'ebpf_module_attach_points_total{{module="cpu_mem_multi"}} {len(ATTACH_POINTS)}')

    for ap in ATTACH_POINTS:
        lines.append("# HELP ebpf_attach_point_info Attach point info")
        lines.append("# TYPE ebpf_attach_point_info gauge")
        lines.append(f'ebpf_attach_point_info{{module="cpu_mem_multi",attach_point="{escape_label_value(ap)}"}} 1')

    if not os.path.isdir(STATE_ROOT):
        return "\n".join(lines) + "\n"

    for container in sorted(os.listdir(STATE_ROOT)):
        state_file = os.path.join(STATE_ROOT, container, "state.env")
        d = read_state(state_file)
        if not d:
            continue

        role = infer_role(container)

        labels = (
            f'container="{escape_label_value(container)}",'
            f'container_role="{escape_label_value(role)}",'
            f'host_pid="{escape_label_value(d.get("host_pid","0"))}",'
            f'cgroup_path="{escape_label_value(d.get("cgroup_path",""))}",'
            f'mnt_ns="{escape_label_value(d.get("mnt_ns","0"))}",'
            f'pid_ns="{escape_label_value(d.get("pid_ns","0"))}",'
            f'net_ns="{escape_label_value(d.get("net_ns","0"))}"'
        )

        lines.append(f'container_cpu_usage_usec{{{labels}}} {as_int(d, "cpu_usage_usec")}')
        lines.append(f'container_cpu_user_usec{{{labels}}} {as_int(d, "cpu_user_usec")}')
        lines.append(f'container_cpu_system_usec{{{labels}}} {as_int(d, "cpu_system_usec")}')
        lines.append(f'container_cpu_nr_periods{{{labels}}} {as_int(d, "cpu_nr_periods")}')
        lines.append(f'container_cpu_nr_throttled{{{labels}}} {as_int(d, "cpu_nr_throttled")}')
        lines.append(f'container_cpu_throttled_usec{{{labels}}} {as_int(d, "cpu_throttled_usec")}')

        lines.append(f'container_memory_current_bytes{{{labels}}} {as_int(d, "memory_current_bytes")}')
        lines.append(f'container_memory_swap_current_bytes{{{labels}}} {as_int(d, "memory_swap_current_bytes")}')
        lines.append(f'container_memory_max_bytes{{{labels}}} {as_int(d, "memory_max_bytes", -1)}')

        lines.append(f'container_sched_switch_total{{{labels}}} {as_int(d, "sched_switch_total")}')
        lines.append(f'container_sched_wakeup_total{{{labels}}} {as_int(d, "sched_wakeup_total")}')
        lines.append(f'container_sched_wakeup_new_total{{{labels}}} {as_int(d, "sched_wakeup_new_total")}')
        lines.append(f'container_sched_process_exit_total{{{labels}}} {as_int(d, "sched_process_exit_total")}')
        lines.append(f'container_mm_page_alloc_total{{{labels}}} {as_int(d, "mm_page_alloc_total")}')
        lines.append(f'container_mm_page_free_total{{{labels}}} {as_int(d, "mm_page_free_total")}')

        lines.append(f'container_sched_latency_avg_us{{{labels}}} {as_float(d, "sched_latency_avg_us")}')
        lines.append(f'container_sched_latency_max_us{{{labels}}} {as_float(d, "sched_latency_max_us")}')
        lines.append(f'container_sched_latency_last_us{{{labels}}} {as_float(d, "sched_latency_last_us")}')

    return "\n".join(lines) + "\n"

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/metrics":
            body = collect_metrics().encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
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
    server = HTTPServer((HOST, PORT), Handler)
    print(f"[OK] auto-role multi exporter listening on http://{HOST}:{PORT}/metrics")
    server.serve_forever()
