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
#include <asm/processor-flags.h>

lock_state_t lock_state;
uint64_t preempted_at;
hybrid_addresses_t addresses;

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

/*
 * Store whether a thread is currently preempted.
 */
struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u32);
} preempted_map SEC(".maps");

void preemption(u64 time, u32 tid)
{
	preempted_at = time;
	long ret = bpf_map_update_elem(&preempted_map, &tid, &tid, BPF_NOEXIST);
	if (ret < 0)
		bpf_printk("Error on map update.");
}

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u64 time = bpf_ktime_get_ns();
	struct pt_regs *regs;
	u32 key;
	hybrid_qnode_t **qnode;

	/*
	 * Clear preempted status of next thread.
	 */
	key = next->pid;
	qnode = bpf_map_lookup_elem(&nodes_map, &key);
	if (qnode != NULL && bpf_map_delete_elem(&preempted_map, &key) == 0)
	{
		bpf_printk("%s (%d) rescheduled after %s (%d) at %ld", next->comm, next->pid, prev->comm, prev->pid, time);
		preempted_at = 116886717438980000; // INT64_MAX
	}

	/*
	 * Optimization: No map lookup if prev is a kernel thread.
	 */
	if (prev->flags & 0x00200000) // PF_KTHREAD
		return 0;

	/*
	 * Retrieve prev's qnode.
	 */
	key = prev->pid;
	qnode = bpf_map_lookup_elem(&nodes_map, &key);
	if (!qnode)
		return 0;

	uint8_t flags = BPF_PROBE_READ_USER(*qnode, flags);

	/*
	 * If prev was preempted in a critical section.
	 */
	if (flags & HYBRID_FLAGS_IN_CS)
	{
		bpf_printk("Preempted in CS.");
		preemption(time, key);
		return 0;
	}

	/*
	 * Ignore preemption if not in CS, Lock or Unlock.
	 */
	if (!(flags & (HYBRID_FLAGS_LOCK | HYBRID_FLAGS_UNLOCK)))
		return 0;

	/*
	 * Retrieve preemption address.
	 */
	u64 user_stack;
	long user_stack_size = bpf_get_task_stack(prev, &user_stack, sizeof(u64), BPF_F_USER_STACK);
	if (user_stack_size < 1)
	{
		bpf_printk("Error getting stack (%ld).", user_stack_size);
		return 0;
	}

	/*
	 * Handle preemption in lock function.
	 */
	if (flags & HYBRID_FLAGS_LOCK)
	{
		if ((u64)addresses.lock_check_rax_null <= user_stack && (u64)addresses.lock_check_rax_null_end > user_stack)
		{
			regs = (struct pt_regs *)bpf_task_pt_regs(prev);
			if ((void *)regs->ax == NULL)
			{
				bpf_printk("Preempted at lock_check_rax_null.");
				preemption(time, key);
				return 0;
			}
		}

		if ((u64)addresses.lock_spin <= user_stack && (u64)addresses.lock_end > user_stack && BPF_PROBE_READ_USER(*qnode, waiting) == 0)
		{
			bpf_printk("Preempted at spin. (%lld bytes away)", user_stack - (u64)addresses.lock_spin);
			preemption(time, key);
			return 0;
		}

		if ((u64)addresses.lock_end <= user_stack)
		{
			bpf_printk("Preempted at end.");
			preemption(time, key);
			return 0;
		}
	}

	/*
	 * Handle preemption in unlock function.
	 */
	if (flags & HYBRID_FLAGS_UNLOCK)
	{
		if ((u64)addresses.unlock_check_zero_flag1 == user_stack || (u64)addresses.unlock_check_zero_flag2 == user_stack)
		{
			regs = (struct pt_regs *)bpf_task_pt_regs(prev);
			if (!(regs->flags & X86_EFLAGS_ZF))
			{
				bpf_printk("Preempted in check_zero_flag.");
				preemption(time, key);
				return 0;
			}
		}

		if ((u64)addresses.unlock_end > user_stack)
		{
			bpf_printk("Preempted in unlock.");
			preemption(time, key);
			return 0;
		}
	}

	return 0;
}
