#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

STATE_DIR = "/home/ryx/experience/ebpf_container_monitor/runtime/io_multi"
LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 2114

def parse_state_file(path: str):
    data = {}
    try:
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                data[k.strip()] = v.strip()
    except FileNotFoundError:
        return None
    return data

def role_of_container(name: str) -> str:
    if "nginx" in name or "web" in name:
        return "web"
    if "redis" in name or "cache" in name or "db" in name:
        return "cache_db"
    return "generic"

def escape_label_value(v: str) -> str:
    return v.replace("\\", "\\\\").replace('"', '\\"')

def metric_line(name: str, labels: dict, value):
    label_text = ",".join(f'{k}="{escape_label_value(str(v))}"' for k, v in labels.items())
    return f"{name}{{{label_text}}} {value}"

def build_metrics_text():
    lines = []
    lines.append("# HELP container_io_stat_rbytes Container cgroup io.stat read bytes")
    lines.append("# TYPE container_io_stat_rbytes gauge")
    lines.append("# HELP container_io_stat_wbytes Container cgroup io.stat write bytes")
    lines.append("# TYPE container_io_stat_wbytes gauge")
    lines.append("# HELP container_io_stat_rios Container cgroup io.stat read ios")
    lines.append("# TYPE container_io_stat_rios gauge")
    lines.append("# HELP container_io_stat_wios Container cgroup io.stat write ios")
    lines.append("# TYPE container_io_stat_wios gauge")
    lines.append("# HELP container_io_stat_dbytes Container cgroup io.stat discard bytes")
    lines.append("# TYPE container_io_stat_dbytes gauge")
    lines.append("# HELP container_io_stat_dios Container cgroup io.stat discard ios")
    lines.append("# TYPE container_io_stat_dios gauge")

    lines.append("# HELP container_io_read_calls Container syscall read calls in current window")
    lines.append("# TYPE container_io_read_calls gauge")
    lines.append("# HELP container_io_write_calls Container syscall write calls in current window")
    lines.append("# TYPE container_io_write_calls gauge")
    lines.append("# HELP container_io_read_bytes Container syscall read bytes in current window")
    lines.append("# TYPE container_io_read_bytes gauge")
    lines.append("# HELP container_io_write_bytes Container syscall write bytes in current window")
    lines.append("# TYPE container_io_write_bytes gauge")
    lines.append("# HELP container_io_errors Container syscall io errors in current window")
    lines.append("# TYPE container_io_errors gauge")

    lines.append("# HELP container_io_latency_avg_us Container io average latency in current window")
    lines.append("# TYPE container_io_latency_avg_us gauge")
    lines.append("# HELP container_io_latency_max_us Container io max latency in current window")
    lines.append("# TYPE container_io_latency_max_us gauge")
    lines.append("# HELP container_io_latency_last_us Container io last latency in current window")
    lines.append("# TYPE container_io_latency_last_us gauge")

    lines.append("# HELP ebpf_io_exporter_up Whether io exporter parsed state files successfully")
    lines.append("# TYPE ebpf_io_exporter_up gauge")

    any_file = False

    if os.path.isdir(STATE_DIR):
        for fn in sorted(os.listdir(STATE_DIR)):
            if not fn.endswith(".state"):
                continue
            path = os.path.join(STATE_DIR, fn)
            state = parse_state_file(path)
            if not state:
                continue

            any_file = True
            container = state.get("container", fn[:-6])
            labels = {
                "container": container,
                "container_role": role_of_container(container),
                "host_pid": state.get("host_pid", ""),
                "cgroup_path": state.get("cgroup_path", ""),
                "mnt_ns": state.get("mnt_ns", ""),
                "pid_ns": state.get("pid_ns", ""),
                "net_ns": state.get("net_ns", ""),
            }

            mapping = {
                "container_io_stat_rbytes": state.get("io_stat_rbytes", "0"),
                "container_io_stat_wbytes": state.get("io_stat_wbytes", "0"),
                "container_io_stat_rios": state.get("io_stat_rios", "0"),
                "container_io_stat_wios": state.get("io_stat_wios", "0"),
                "container_io_stat_dbytes": state.get("io_stat_dbytes", "0"),
                "container_io_stat_dios": state.get("io_stat_dios", "0"),

                "container_io_read_calls": state.get("read_calls", "0"),
                "container_io_write_calls": state.get("write_calls", "0"),
                "container_io_read_bytes": state.get("read_bytes", "0"),
                "container_io_write_bytes": state.get("write_bytes", "0"),
                "container_io_errors": state.get("io_errors", "0"),

                "container_io_latency_avg_us": state.get("io_latency_avg_us", "0"),
                "container_io_latency_max_us": state.get("io_latency_max_us", "0"),
                "container_io_latency_last_us": state.get("io_latency_last_us", "0"),
            }

            for metric_name, value in mapping.items():
                lines.append(metric_line(metric_name, labels, value))

    lines.append(f"ebpf_io_exporter_up {1 if any_file else 0}")
    return "\n".join(lines) + "\n"

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/metrics":
            body = build_metrics_text().encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed.path == "/" or parsed.path == "/healthz":
            body = b"ok\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_response(404)
        self.end_headers()

    def log_message(self, fmt, *args):
        return

def main():
    httpd = HTTPServer((LISTEN_HOST, LISTEN_PORT), Handler)
    print(f"[OK] io exporter listening on http://{LISTEN_HOST}:{LISTEN_PORT}/metrics", flush=True)
    httpd.serve_forever()

if __name__ == "__main__":
    main()
