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
#include "platform_defs.h"
#include "hybridlock_bpf.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

lock_state_t lock_state = LOCK_STABLE(LOCK_TYPE_MCS);
mcs_qnode_t *mcs_lock;

char _license[4] SEC("license") = "GPL";

typedef mcs_qnode_t *map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
} nodes_map SEC(".maps");

/*
 * Store information about an intermediate process
 *
 * We want to ignore some preemptions (by kworkers or interrupts for example).
 * However some preemptions go through intermediate processes (ex: migration)
 * We then store the fact that a thread has been preempted by an intermediate process
 * And handle the switching (or ignoring) in the next preemption of that core.
 *
 * key: PID of the intermediate process
 * value: PID of the hybridlock process
 */
struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, int);
	__type(value, u32);
	__uint(max_entries, 80);
} intermediate_map SEC(".maps");

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u32 tid = prev->pid;
	int cpu = prev->wake_cpu;

	u32 *p = bpf_map_lookup_elem(&intermediate_map, &cpu);
	if (p != NULL)
		tid = *p;
	else
	{
		if (!preempt)
			return 0;

		// Lookup thread qnode
		mcs_qnode_t **qnode = bpf_map_lookup_elem(&nodes_map, &tid);
		if (qnode == NULL)
			return 0;

		if (!(BPF_PROBE_READ_USER(*qnode, waiting) == 0 && (mcs_lock == *qnode || (BPF_PROBE_READ_USER(*qnode, next) != NULL && BPF_PROBE_READ_USER(*qnode, next, waiting) == 1))))
			return 0;
	}

#ifdef DEBUG
	bpf_printk("[tid: %d] %s (%d) -> %s (%d)", tid, prev->comm, prev->pid, next->comm, next->pid);
#endif

	if (__builtin_memcmp(next->comm, "migration", 9) == 0)
	{
		bpf_map_delete_elem(&intermediate_map, &cpu);
		return 0;
	}

	if (next->flags & 0x00200000) // PF_KTHREAD
	{
		bpf_printk("Intermediate process - storing that");
		bpf_map_update_elem(&intermediate_map, &cpu, &tid, BPF_ANY);
		return 0;
	}

	bpf_map_delete_elem(&intermediate_map, &cpu);
	if (tid == next->pid)
		return 0;

	lock_state_t curr = LOCK_CURR_TYPE(lock_state);
	bpf_printk("Switching to Futex");

	// Changing lock type to futex
	__sync_val_compare_and_swap(&lock_state, LOCK_STABLE(curr), LOCK_TRANSITION(curr, LOCK_TYPE_FUTEX));

	return 0;
}
