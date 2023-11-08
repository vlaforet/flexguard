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
uint64_t preempted_at;

char _license[4] SEC("license") = "GPL";

typedef mcs_qnode_t *map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
} nodes_map SEC(".maps");

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u32 tid;
	mcs_qnode_t **qnode;
	u64 time = bpf_ktime_get_ns();

	if (LOCK_CURR_TYPE(lock_state) == LOCK_TYPE_FUTEX)
		return 0;

	// Handling prev
	tid = prev->pid;
	qnode = bpf_map_lookup_elem(&nodes_map, &tid);
	if (qnode != NULL && BPF_PROBE_READ_USER(*qnode, waiting) == 0 && (mcs_lock == *qnode || (BPF_PROBE_READ_USER(*qnode, next) != NULL && BPF_PROBE_READ_USER(*qnode, next, waiting) == 1)))
	{
#if DEBUG == 1
		bpf_printk("%s (%d) preempted by %s (%d) at %ld: %d", prev->comm, prev->pid, next->comm, next->pid, time, BPF_PROBE_READ_USER(*qnode, has_lock));
#endif
		preempted_at = time;
	}

	// Handling next
	tid = next->pid;
	qnode = bpf_map_lookup_elem(&nodes_map, &tid);
	if (qnode != NULL && BPF_PROBE_READ_USER(*qnode, waiting) == 0 && (mcs_lock == *qnode || (BPF_PROBE_READ_USER(*qnode, next) != NULL && BPF_PROBE_READ_USER(*qnode, next, waiting) == 1)))
	{
#if DEBUG == 1
		bpf_printk("%s (%d) rescheduled after %s (%d) at %ld", next->comm, next->pid, prev->comm, prev->pid, time);
#endif
		preempted_at = 18446744073709551615UL; // INT64_MAX
	}

	return 0;
}
