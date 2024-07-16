/*
 * File: hybridv2.bpf.c
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
#include "hybridv2_bpf.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <asm/processor-flags.h>

#ifdef DEBUG
#define DPRINT(args...) bpf_printk(args);
#else
#define DPRINT(...)
#endif

#define MAX_STACK_TRACE_DEPTH 8

hybrid_addresses_t addresses;

volatile hybrid_lock_info_t lock_info[MAX_NUMBER_LOCKS];

char _license[4] SEC("license") = "GPL";

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, int);
	__uint(max_entries, MAX_NUMBER_THREADS);
} nodes_map SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, hybrid_qnode_t);
	__uint(max_entries, MAX_NUMBER_THREADS);
	__uint(map_flags, BPF_F_MMAPABLE);
} array_map SEC(".maps");

static int on_preemption(hybrid_qnode_ptr holder)
{
	int lock_id = holder->locking_id;
	if (!(lock_id >= 0 && lock_id < MAX_NUMBER_LOCKS)) // Weird negative to please the verifier
		return 1;																				 // Should never happen
	volatile hybrid_lock_info_t *linfo = &lock_info[lock_id];

	__sync_fetch_and_add(&linfo->is_blocking, 1);
	holder->is_holder_preempted = 1;
	return 0;
}

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	struct pt_regs *regs;
	u32 key;
	hybrid_qnode_ptr qnode;
	int lock_id, *thread_id;

	if (next->flags & 0x00200000) // PF_KTHREAD
		return 0;

	/*
	 * Clear preempted status of next thread.
	 */
	key = next->pid;
	thread_id = bpf_map_lookup_elem(&nodes_map, &key);
	if (thread_id && (qnode = bpf_map_lookup_elem(&array_map, thread_id)))
	{
		qnode->is_running = 1;

		lock_id = qnode->locking_id;
		if (lock_id >= 0 && lock_id < MAX_NUMBER_LOCKS && qnode->is_holder_preempted)
		{
			DPRINT("%s (%d) rescheduled after %s (%d)", next->comm, next->pid, prev->comm, prev->pid);
			__sync_fetch_and_sub(&lock_info[lock_id].is_blocking, 1);
			qnode->is_holder_preempted = 0;
		}
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
	thread_id = bpf_map_lookup_elem(&nodes_map, &key);
	if (!thread_id || !(qnode = bpf_map_lookup_elem(&array_map, thread_id)))
		return 0;

	qnode->is_running = 0;
	lock_id = qnode->locking_id;

	/*
	 * Ignore preemption if the thread was not locking.
	 */
	if (lock_id < 0 || lock_id >= MAX_NUMBER_LOCKS)
		return 0;

	/*
	 * Retrieve preemption address.
	 */
	u64 user_stack[MAX_STACK_TRACE_DEPTH];
	long user_stack_size = bpf_get_task_stack(prev, user_stack, MAX_STACK_TRACE_DEPTH * sizeof(u64), BPF_F_USER_STACK);
	if (user_stack_size <= 0)
	{
		bpf_printk("Error getting stack (%ld).", user_stack_size);
		return 1;
	}

	for (int i = 0; i < user_stack_size / sizeof(u64); i++)
	{
		/*
		 * Ignore preemptions due to futex wait.
		 */
		if ((u64)addresses.futex_wait <= user_stack[i] && user_stack[i] < (u64)addresses.futex_wait_end)
			return 0;

		/*
		 * Ignore preemptions before enqueue.
		 */
		if ((u64)addresses.lock <= user_stack[i] && user_stack[i] < (u64)addresses.lock_check_rcx_null)
		{
			DPRINT("[%d] Ignored not enqueued", i);
			return 0;
		}

		if ((u64)addresses.lock_check_rcx_null <= user_stack[i] && user_stack[i] < (u64)addresses.lock_end)
		{
			/*
			 * No need to check registers if preemption happened deeper
			 * than the lock function.
			 */
			if (i == 0)
			{
				regs = (struct pt_regs *)bpf_task_pt_regs(prev);

				/*
				 * Ignore preemptions if lock was not free (prev != NULL)
				 * Only needs to be checked on first stack address.
				 */
				if ((void *)regs->cx != NULL)
				{
					DPRINT("[%d] Ignored pred != NULL", i);
					return 0;
				}
			}

			/*
			 * Ignore preemptions while spinning if
			 * previous node did not release lock (waiting != 0)
			 */
			if (qnode->waiting != 0)
			{
				DPRINT("[%d] Ignored while spinning", i);
				return 0;
			}
		}
	}

	DPRINT("%s (%d) preempted to %s (%d): %lld B away from bhl_lock", prev->comm, prev->pid, next->comm, next->pid, (long long)user_stack[0] - (long long)addresses.lock);

	__sync_fetch_and_add(&lock_info[lock_id].is_blocking, 1);
	qnode->is_holder_preempted = 1;

	return 0;
}
