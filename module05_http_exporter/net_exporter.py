#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

STATE_DIR = os.path.expanduser("~/experience/ebpf_container_monitor/runtime/net_multi")
LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 2115


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
    metric_defs = {
        "container_net_dev_queue_total": "Packets entering net_dev_queue attach point",
        "container_net_dev_queue_bytes_total": "Bytes entering net_dev_queue attach point",
        "container_netif_receive_skb_total": "Packets entering netif_receive_skb attach point",
        "container_netif_receive_skb_bytes_total": "Bytes entering netif_receive_skb attach point",
        "container_net_inet_sock_set_state_total": "TCP socket state transitions",
        "container_net_tcp_established_total": "TCP connections reaching ESTABLISHED",
        "container_net_tcp_close_state_total": "TCP state transitions to CLOSE",
        "container_net_tcp_v4_connect_total": "tcp_v4_connect calls",
        "container_net_tcp_close_total": "tcp_close calls",
    }

    for name, help_text in metric_defs.items():
        lines.append(f"# HELP {name} {help_text}")
        lines.append(f"# TYPE {name} counter")

    lines.append("# HELP ebpf_net_exporter_up Whether net exporter parsed state files successfully")
    lines.append("# TYPE ebpf_net_exporter_up gauge")

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
                "container_net_dev_queue_total": state.get("net_dev_queue_cnt", "0"),
                "container_net_dev_queue_bytes_total": state.get("net_dev_queue_bytes", "0"),
                "container_netif_receive_skb_total": state.get("netif_receive_skb_cnt", "0"),
                "container_netif_receive_skb_bytes_total": state.get("netif_receive_skb_bytes", "0"),
                "container_net_inet_sock_set_state_total": state.get("inet_sock_set_state_cnt", "0"),
                "container_net_tcp_established_total": state.get("tcp_established_cnt", "0"),
                "container_net_tcp_close_state_total": state.get("tcp_close_state_cnt", "0"),
                "container_net_tcp_v4_connect_total": state.get("tcp_v4_connect_cnt", "0"),
                "container_net_tcp_close_total": state.get("tcp_close_cnt", "0"),
            }

            for metric_name, value in mapping.items():
                lines.append(metric_line(metric_name, labels, value))

    lines.append(f"ebpf_net_exporter_up {1 if any_file else 0}")
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
    print(f"[OK] net exporter listening on http://{LISTEN_HOST}:{LISTEN_PORT}/metrics", flush=True)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
