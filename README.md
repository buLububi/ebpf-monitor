# eBPF Container Monitor 🚀

基于 eBPF 的容器性能监控系统，支持：

* CPU / 调度 / 内存
* IO（读写 / 延迟）
* Prometheus 指标采集
* Grafana 可视化
* Alertmanager 邮件告警
* 容器分类监控（web / cache_db 等）

---

# 📦 系统架构

```
eBPF monitor (宿主机)
        ↓
Exporter (19100 / 2114)
        ↓
Prometheus (抓取)
        ↓
Grafana (展示)
        ↓
Alertmanager (告警 → 邮件)
```

---

# 📁 项目目录

```
~/experience/ebpf_container_monitor
├── module01_cpu_mem
├── module02_io
├── module05_http_exporter
├── deploy
│   ├── prometheus.yml
│   ├── alert_rules.yml
│   └── grafana/
├── runtime
└── scripts
```

---

# 🚀 手动启动系统（推荐方式）

⚠️ 注意：

* **不要混用 docker compose**
* **不要删除已有容器（否则 dashboard 会丢）**
* 全部在这个目录执行：

```
/home/ryx/experience/ebpf_container_monitor
```

---

## 一键启动（推荐直接用）

```bash
cd /home/ryx/experience/ebpf_container_monitor

echo "========== [0] 环境检查 =========="
docker ps --format "table {{.Names}}\t{{.Status}}"
echo

echo "========== [1] 启动 CPU/MEM =========="
bash scripts/start_cpu_mem_multi.sh test-redis test-nginx
echo

echo "========== [2] 启动 IO =========="
sudo bash scripts/start_io_multi.sh test-redis test-nginx
nohup python3 module05_http_exporter/io_exporter.py > runtime/io_multi/exporter.out 2>&1 &
echo

echo "========== [3] 启动 Alertmanager =========="
docker start ebpf_alertmanager 2>/dev/null || true
echo

echo "========== [4] 启动 Prometheus =========="
docker start ebpf_prometheus 2>/dev/null || true
echo

echo "========== [5] 启动 Grafana =========="
docker start ebpf_grafana 2>/dev/null || true
echo

echo "========== [6] 验证 CPU =========="
curl --noproxy '*' -s http://127.0.0.1:19100/metrics | grep container_cpu_usage_usec || echo NO_CPU_METRIC
echo

echo "========== [7] 验证 IO =========="
curl --noproxy '*' -s http://127.0.0.1:2114/metrics | grep container_io | head -n 20 || echo NO_IO_METRIC
echo

echo "========== [8] 验证 Alertmanager =========="
curl --noproxy '*' -s http://127.0.0.1:9093/-/ready || echo ALERTMANAGER_NOT_READY
echo

echo "========== [9] 查看进程 =========="
ps -ef | grep -E 'cpu_mem_multi|io_multi|exporter' | grep -v grep
echo

echo "========== [10] 查看容器 =========="
docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}"
echo

echo "========== [11] 访问地址 =========="
echo "Grafana      : http://127.0.0.1:3000"
echo "Prometheus   : http://127.0.0.1:9090"
echo "Alertmanager : http://127.0.0.1:9093"
```

---

# 🧪 验证系统

## 1. CPU 指标

Prometheus 查询：

```
container_cpu_usage_usec
```

---

## 2. IO 指标

```
container_io_stat_rbytes
container_io_latency_avg_us
```

---

## 3. Grafana

打开：

```
http://127.0.0.1:3000
```

---

## 4. 压测验证

### CPU 压测

```bash
docker exec test-nginx sh -c '(while :; do :; done) & (while :; do :; done) & sleep 60'
```

---

### IO 压测

```bash
docker exec test-nginx sh -c 'dd if=/dev/zero of=/tmp/test bs=1M count=200'
```

---

# 📧 告警验证

触发 CPU 或 IO 高负载后：

* Prometheus 触发规则
* Alertmanager 转发
* 邮件发送

---

# 🛑 停止系统

```bash
cd /home/ryx/experience/ebpf_container_monitor

pkill -f cpu_mem_multi_exporter.py || true
pkill -f io_exporter.py || true
sudo pkill -f cpu_mem_multi_monitor || true
sudo pkill -f io_multi_monitor || true

docker stop ebpf_grafana 2>/dev/null || true
docker stop ebpf_prometheus 2>/dev/null || true
docker stop ebpf_alertmanager 2>/dev/null || true
```

---

# ⚠️ 常见问题

## Grafana 无数据

* 检查 datasource → 是否为：

```
http://host.docker.internal:9090
```

---

## IO 无数据

* 检查：

```
curl http://127.0.0.1:2114/metrics
```

---

## CPU 无数据

* 检查：

```
curl http://127.0.0.1:19100/metrics
```

---

# 🎯 项目特点

* 多容器维度监控
* namespace 过滤
* eBPF attach points 设计
* 指标可扩展
* 可直接对接生产监控体系

---

# 📌 作者说明

当前版本为手动启动稳定版：

* 不依赖 docker compose
* 不破坏已有数据
* 适合演示 / 项目展示 

---
