/*
 * File: hybridlock.c
 * Author: Victor Laforet <victor.laforet@ip-paris.fr>
 *
 * Description:
 *      BPF preemption detection
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Victor Laforet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

pid_t tgid;
uint8_t *input_pid;
int *input_spinning;

char _license[4] SEC("license") = "GPL";

SEC("tp_btf/sched_switch")
int handle_sched_switch(u64 *ctx)
{
	bool preempt = (bool)ctx[0];
	struct task_struct *prev = (struct task_struct *)ctx[1];
	struct task_struct *next = (struct task_struct *)ctx[2];

	if (!preempt || !(prev->tgid == tgid || next->tgid == tgid))
		return 0;

	uint8_t pid;
	int err;

	err = bpf_probe_read_user(&pid, sizeof(pid), (const void *)input_pid);
	if (err != 0)
	{
		bpf_printk("Error on bpf_probe_read_user(pid) -> %d.\n", err);
		return 0;
	}

	bpf_printk("p: %d %d %d\n", pid, prev->pid, next->pid);

	if (pid == 0 || prev->pid != pid)
		return 0;

	bpf_printk("Spinning = 0.\n");
	int spinning = 0;

	err = bpf_probe_write_user((const void *)input_spinning, &spinning, sizeof(int));
	if (err != 0 && err != -1) // EPERM -1: raised when another thread is writing at the same time.
		bpf_printk("Error on bpf_probe_write_user(spinning) -> %d.\n", err);

	return 0;
}
