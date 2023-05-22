/*
 * File: hybridlock.bpf.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a MCS/futex hybrid lock - BPF preemptions detection
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Victor Laforet
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
#include "hybridlock_bpf.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

int *input_spinning;

char _license[4] SEC("license") = "GPL";

typedef mcs_qnode *map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
} nodes_map SEC(".maps");

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	int err, spinning;

	if (!preempt)
		return 0;

	// Lookup thread qnode
	u32 k = prev->pid;
	mcs_qnode **qnode = bpf_map_lookup_elem(&nodes_map, &k);
	if (qnode == NULL)
		return 0;

	// Ignore if thread not lock holder
	if (!BPF_PROBE_READ_USER(*qnode, locking) || BPF_PROBE_READ_USER(*qnode, waiting))
		return 0;

	bpf_printk("Spinning = 0");

	// Changing lock state to blocking
	spinning = 0;
	err = bpf_probe_write_user((void *)input_spinning, &spinning, sizeof(spinning));
	if (err && err != -1) // EPERM -1: raised when another thread is writing at the same time.
		bpf_printk("Error on bpf_probe_write_user(spinning) -> %d", err);

	return 0;
}
