/*
 * File: hybridlock.bpf.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a hybrid lock with MCS, CLH or Ticket spin locks.
 * 			BPF preemptions detection
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

lock_state_t lock_state;
uint64_t preempted_at;

#ifdef HYBRID_TICKET
uint32_t *ticket_calling;
#endif

char _license[4] SEC("license") = "GPL";

typedef hybrid_qnode_t *map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
} nodes_map SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u32);
} preempted_map SEC(".maps");

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u32 tid;
	u64 time = bpf_ktime_get_ns();

#ifdef HYBRID_TICKET
	uint32_t **elem;

	uint32_t calling;
	long err;
	if ((err = bpf_core_read_user(&calling, sizeof(calling), ticket_calling)) != 0)
	{
		bpf_printk("Failed to read ticket calling (%ld).", err);
		return 0;
	}

#else
	clh_qnode_t **elem;
#endif

	if (LOCK_CURR_TYPE(lock_state) == LOCK_TYPE_FUTEX)
		return 0;

	// Handling prev
	tid = prev->pid;
	elem = bpf_map_lookup_elem(&nodes_map, &tid);

#ifdef HYBRID_TICKET
	uint32_t ticket;
	if (elem != NULL)
		if (bpf_probe_read_user(&ticket, sizeof(ticket), *elem) != 0)
		{
			bpf_printk("Failed to read thread ticket.");
			return 0;
		}

	if (elem != NULL && ticket == calling)
#else
	if (elem != NULL && BPF_PROBE_READ_USER(*elem, done) == 0 && BPF_PROBE_READ_USER(*elem, pred, done) == 1)
#endif
	{
#if DEBUG == 1
		bpf_printk("%s (%d) preempted by %s (%d) at %ld", prev->comm, prev->pid, next->comm, next->pid, time);
#endif
		preempted_at = time;
		bpf_map_update_elem(&preempted_map, &tid, &tid, BPF_MAP_CREATE);
	}

	// Handling next
	tid = next->pid;
	elem = bpf_map_lookup_elem(&nodes_map, &tid);

#ifdef HYBRID_TICKET
	if (elem != NULL)
		if (bpf_probe_read_user(&ticket, sizeof(ticket), *elem) != 0)
		{
			bpf_printk("Failed to read thread ticket.");
			return 0;
		}

	if (elem != NULL && ticket == calling)
#else
	if (elem != NULL && bpf_map_delete_elem(&preempted_map, &tid) == 0)
#endif
	{
#if DEBUG == 1
		bpf_printk("%s (%d) rescheduled after %s (%d) at %ld", next->comm, next->pid, prev->comm, prev->pid, time);
#endif
		preempted_at = 116886717438980000; // INT64_MAX
	}

	return 0;
}
