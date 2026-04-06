CC       ?= gcc
BPF_CLANG?= clang
BPFTOOL  ?= bpftool

all: module01_cpu_mem/cpu_mem_monitor

module01_cpu_mem/cpu_mem.bpf.o: module01_cpu_mem/cpu_mem.bpf.c common/cpu_mem_common.h
	$(BPF_CLANG) -O2 -g -target bpf -D__TARGET_ARCH_x86 -Icommon -Imodule01_cpu_mem -c $< -o $@

module01_cpu_mem/cpu_mem.skel.h: module01_cpu_mem/cpu_mem.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

module01_cpu_mem/cpu_mem_monitor: module01_cpu_mem/cpu_mem.c module01_cpu_mem/cpu_mem.skel.h common/target.c common/target.h common/cpu_mem_common.h
	$(CC) -O2 -g module01_cpu_mem/cpu_mem.c common/target.c -o $@ \
		-Icommon -Imodule01_cpu_mem -lbpf -lelf -lz

clean:
	rm -f module01_cpu_mem/*.o
	rm -f module01_cpu_mem/*.skel.h
	rm -f module01_cpu_mem/cpu_mem_monitor
