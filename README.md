# eBPF Container Monitor

基于 eBPF 的容器性能监测工具，面向容器 CPU、内存、调度、IO、网络五类性能数据采集，并对接 Prometheus、Grafana 和 Alertmanager，实现指标采集、可视化展示和邮件告警。

## 系统能力

- CPU / 内存 / 调度监控：采集 CPU 使用、内存占用、调度切换、唤醒、调度延迟等指标。
- IO 监控：采集容器 cgroup IO 统计、read/write 调用、读写字节、IO 延迟等指标。
- 网络监控：挂载 `net_dev_queue`、`netif_receive_skb`、`inet_sock_set_state`、`tcp_v4_connect`、`tcp_close`，采集收发流量、收发包、TCP 建连/关闭和状态变化。
- Prometheus 指标采集：CPU/MEM 暴露 `19100`，IO 暴露 `2114`，NET 暴露 `2115`。
- Grafana 可视化：包含容器总览、容器详情、网络专项 dashboard。
- Alertmanager 告警：支持 CPU、IO、调度、网络、采集器异常告警，并发送邮件。

## 系统架构

```text
eBPF monitor
  -> runtime state files
  -> Python exporter
  -> Prometheus
  -> Grafana
  -> Alertmanager
  -> Email
```

## 目录结构

```text
.
├── common                         # 公共结构体和目标容器识别逻辑
├── module01_cpu_mem               # CPU / 内存 / 调度 eBPF 模块
├── module02_io                    # IO eBPF 模块
├── module03_net                   # 网络 eBPF 模块
├── module05_http_exporter          # Prometheus exporter
├── deploy
│   ├── prometheus.yml
│   ├── alert_rules.yml
│   ├── alertmanager.yml
│   └── grafana/
├── scripts                         # 构建、启动、停止脚本
└── runtime                         # 运行时 state / log / pid，已忽略提交
```

## 一键启动整个项目

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/start_all.sh
```

`start_all.sh` 会完成：

- 启动 Prometheus、Grafana、Alertmanager。
- 准备 `test-redis` 和 `test-nginx` 两个测试容器。
- 启动 CPU/MEM、IO、NET 多容器监控。
- 启动 IO 和 NET exporter。
- 输出健康检查和访问地址。

访问地址：

```text
Grafana      : http://127.0.0.1:3000
Prometheus   : http://127.0.0.1:9090
Alertmanager : http://127.0.0.1:9093
```

## 停止整个项目

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/stop_all.sh
```

## 单独启动 / 停止网络模块

启动：

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/start_net_multi.sh test-redis test-nginx
bash scripts/start_net_exporter.sh
```

停止：

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/stop_net_multi.sh
bash scripts/stop_net_exporter.sh
```

## 单独启动 / 停止 IO 模块

IO 模块需要 eBPF/root 权限，脚本会先执行 `sudo -v`，如果提示输入密码，输入当前用户 sudo 密码即可。

启动：

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/stop_io_multi.sh
bash scripts/stop_io_exporter.sh
bash scripts/start_io_multi.sh test-redis test-nginx
bash scripts/start_io_exporter.sh
```

停止：

```bash
cd /home/ryx/experience/ebpf_container_monitor
bash scripts/stop_io_multi.sh
bash scripts/stop_io_exporter.sh
```

如果 IO exporter 出现旧数据，清理 root 旧进程：

```bash
sudo pkill -f io_exporter.py
bash scripts/start_io_exporter.sh
```

## 指标端口和核心指标

### CPU / 内存 / 调度

Exporter：

```text
http://127.0.0.1:19100/metrics
```

核心指标：

```text
container_cpu_usage_usec
container_memory_current_bytes
container_sched_switch_total
container_sched_wakeup_total
container_sched_latency_avg_us
container_sched_latency_max_us
```

### IO

Exporter：

```text
http://127.0.0.1:2114/metrics
```

核心指标：

```text
container_io_stat_rbytes
container_io_stat_wbytes
container_io_stat_rios
container_io_stat_wios
container_io_read_calls
container_io_write_calls
container_io_read_bytes
container_io_write_bytes
container_io_latency_avg_us
container_io_latency_max_us
```

### 网络

Exporter：

```text
http://127.0.0.1:2115/metrics
```

核心指标：

```text
container_net_dev_queue_total
container_net_dev_queue_bytes_total
container_netif_receive_skb_total
container_netif_receive_skb_bytes_total
container_net_inet_sock_set_state_total
container_net_tcp_established_total
container_net_tcp_close_state_total
container_net_tcp_v4_connect_total
container_net_tcp_close_total
```

## Grafana 仪表盘

主要 dashboard：

- `容器性能总览`：观察容器数量、告警数量、健康状态、CPU/内存/IO/调度/网络总览和排行。
- `容器性能详情`：按单个容器查看 CPU、内存、IO、调度、网络趋势和告警阈值。
- `容器网络监控（eBPF）`：专项展示网络发送/接收速率、包数量、TCP 建连/关闭、五个网络挂载点计数。

Grafana 重载 dashboard：

```bash
docker restart ebpf_grafana
```

## 告警规则

告警配置文件：

```text
deploy/alert_rules.yml
```

当前业务告警：

```text
容器CPU使用率过高
容器IO平均延迟过高
容器IO最大延迟过高
容器IO写入活动过高
容器调度最大延迟过高
容器网络发送流量过高
容器网络接收流量过高
容器TCP关闭过于频繁
```

采集器异常告警：

```text
CPU内存采集器异常
IO采集器异常
网络采集器异常
```

规则策略：

- 固定触发线保证演示和验收时可解释、可触发。
- `for: 30s` 过滤短暂尖峰，避免一两秒波动直接 firing。
- 采集器异常使用 `for: 1m`，避免 exporter 短暂重启误报。

修改告警规则后重载 Prometheus：

```bash
cd /home/ryx/experience/ebpf_container_monitor/deploy
docker compose up -d --force-recreate prometheus
docker exec ebpf_prometheus promtool check rules /etc/prometheus/alert_rules.yml
```

查看告警：

```bash
docker exec ebpf_prometheus wget -qO- 'http://127.0.0.1:9090/api/v1/alerts'
```

浏览器查看：

```text
http://127.0.0.1:9090/alerts
http://127.0.0.1:9093
```

## 分类压测命令

以下命令都会自动停止，命令里通过时间控制运行约 120 秒。

### CPU / 调度压测

```bash
docker exec -d test-nginx sh -c '(while :; do :; done) & (while :; do :; done) & sleep 120'
```

主要变化：

```text
rate(container_cpu_usage_usec[30s]) 上升
container_sched_latency_max_us 可能上升
container_sched_switch_total 增加
container_sched_wakeup_total 增加
```

可能触发：

```text
容器CPU使用率过高
容器调度最大延迟过高
```

### IO 压测

如果 IO 告警没有变化，先确认 IO exporter 返回的是当前容器 PID：

```bash
docker inspect -f '{{.State.Pid}}' test-nginx
curl --noproxy '*' -s http://127.0.0.1:2114/metrics | grep 'container_io_write_calls' | grep test-nginx
```

压测：

```bash
docker exec -d test-nginx sh -c 'end=$(( $(date +%s) + 120 )); while [ $(date +%s) -lt $end ]; do dd if=/dev/zero of=/tmp/ebpf_io_test bs=4K count=20000 conv=fdatasync >/dev/null 2>&1; rm -f /tmp/ebpf_io_test; done'
```

主要变化：

```text
container_io_write_calls 增加
container_io_stat_wbytes 增加
container_io_stat_wios 增加
container_io_write_bytes 可能增加
container_io_latency_avg_us 可能上升
container_io_latency_max_us 可能上升
```

可能触发：

```text
容器IO平均延迟过高
容器IO最大延迟过高
容器IO写入活动过高
```

### 网络 / TCP 压测

```bash
docker exec -d test-nginx sh -c 'end=$(( $(date +%s) + 120 )); while [ $(date +%s) -lt $end ]; do curl -s http://127.0.0.1/ >/dev/null; done'
```

主要变化：

```text
rate(container_net_dev_queue_bytes_total[30s]) 上升
rate(container_netif_receive_skb_bytes_total[30s]) 上升
rate(container_net_tcp_close_total[30s]) 上升
container_net_tcp_established_total 增加
container_net_inet_sock_set_state_total 增加
```

可能触发：

```text
容器网络发送流量过高
容器网络接收流量过高
容器TCP关闭过于频繁
```

## 停止压测

虽然上述压测命令会自动停止，但如果需要立即停止，可以执行：

```bash
docker exec test-nginx sh -c "for p in /proc/[0-9]*; do pid=\${p##*/}; cmd=\$(tr '\0' ' ' < \$p/cmdline 2>/dev/null); case \"\$cmd\" in *'while :; do :; done'*|*'ebpf_io_test'*|*'dd if=/dev/zero'*|*'curl -s http://127.0.0.1'*) kill \$pid 2>/dev/null || true;; esac; done; rm -f /tmp/ebpf_io_test*"
```

压测停止后等待 30-60 秒，Prometheus 告警会恢复。

## 常见问题

### IO 无数据或一直是旧 PID

查看当前容器 PID：

```bash
docker inspect -f '{{.State.Pid}}' test-nginx
```

查看 IO exporter 指标：

```bash
curl --noproxy '*' -s http://127.0.0.1:2114/metrics | grep 'container_io_write_calls' | grep test-nginx
```

如果看到旧 `host_pid`，清理旧 exporter 并重启：

```bash
sudo pkill -f io_exporter.py
bash scripts/start_io_exporter.sh
```

然后重启 IO monitor：

```bash
bash scripts/stop_io_multi.sh
bash scripts/start_io_multi.sh test-redis test-nginx
```

### Prometheus 规则不是最新

`alert_rules.yml` 是单文件挂载，修改后建议强制重建 Prometheus：

```bash
cd /home/ryx/experience/ebpf_container_monitor/deploy
docker compose up -d --force-recreate prometheus
```

### Grafana 看不到新 dashboard

```bash
docker restart ebpf_grafana
```

### 网络无数据

确认网络 exporter：

```bash
curl --noproxy '*' -s http://127.0.0.1:2115/metrics | grep container_net
```

重启网络模块：

```bash
bash scripts/stop_net_multi.sh
bash scripts/stop_net_exporter.sh
bash scripts/start_net_multi.sh test-redis test-nginx
bash scripts/start_net_exporter.sh
```

## 推送说明

仓库地址：

```text
https://github.com/buLububi/ebpf-monitor.git
```
