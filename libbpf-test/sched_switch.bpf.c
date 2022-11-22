#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "sched_switch.h"

pid_t target_pid = 0;

char _license[4] SEC("license") = "GPL";

struct
{
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

SEC("tp_btf/sched_switch")
int handle_sched_switch(u64 *ctx)
{
	bool preempt = (bool)ctx[0];
	struct task_struct *prev = (struct task_struct *)ctx[1];
	struct task_struct *next = (struct task_struct *)ctx[2];

	struct data_t data = {};
	int pid = next->pid;

	if (!preempt || !pid || target_pid != pid)
		return 0;

	data.pid = pid;
	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &data, sizeof(data));

	return 0;
}
