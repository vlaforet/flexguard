#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "sched_switch.h"

int *input_pid;
int *input_spinning;

char _license[4] SEC("license") = "GPL";

SEC("tp_btf/sched_switch")
int handle_sched_switch(u64 *ctx)
{
	bool preempt = (bool)ctx[0];
	struct task_struct *prev = (struct task_struct *)ctx[1];

	if (!preempt || !prev->pid)
		return 0;

	int pid;
	int err;

	err = bpf_probe_read_user(&pid, sizeof(int), (const void *)input_pid);
	if (err == -14) // EFAULT -14: raised when the variable is not in use thus no thread is waiting.
		return 0;
	if (err != 0)
	{
		bpf_printk("Error on bpf_probe_read_user(pid) -> %d.\n", err);
		return 0;
	}

	if (pid == 0 || prev->pid != pid)
		return 0;

	bpf_printk("Spinning = 0.\n");
	int spinning = 0;

	err = bpf_probe_write_user((const void *)input_spinning, &spinning, sizeof(int));
	if (err != 0 && err != -1) // EPERM -1: raised when another thread is writing at the same time.
		bpf_printk("Error on bpf_probe_write_user(spinning) -> %d.\n", err);

	return 0;
}
