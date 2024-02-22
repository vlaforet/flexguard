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

#ifdef DEBUG
#define DPRINT(args...) bpf_printk(args);
#else
#define DPRINT(...)
#endif

#define MAX_STACK_TRACE_DEPTH 8

/*
 * Translate a user-space qnode pointer to a kernel-space qnode pointer.
 * Should be used as a condition to quiet the eBPF verifier.
 * The negative condition should never happen as long as your
 * user-space pointer is valid.
 *
 * Example:
 * ```
 * int id;
 * hybrid_qnode_ptr kern;
 *
 * // Inline
 * return QNODE_USER_TO_KERNEL(user, id, kern) ? kern->waiting : 0;
 *
 * // Full condition
 * if (QNODE_USER_TO_KERN(user, id, kern)) return kern->waiting;
 *
 * // Negative
 * if (!QNODE_USER_TO_KERN(user, id, kern)) return 0; // Should never happen if "user" is valid
 * return kern->waiting;
 * ```
 */
#define QNODE_USER_TO_KERNEL(user_qnode, id, kern_qnode) \
	((id = (user_qnode - qnode_allocation_starting_address)) >= 0 && id < (MAX_NUMBER_LOCKS * MAX_NUMBER_THREADS) && (kern_qnode = &qnode_allocation_array[id]))

#ifndef HYBRID_EPOCH
unsigned long preempted_at[MAX_NUMBER_LOCKS];
#endif

hybrid_addresses_t addresses;
volatile hybrid_qnode_t qnode_allocation_array[MAX_NUMBER_LOCKS * MAX_NUMBER_THREADS];
hybrid_qnode_ptr qnode_allocation_starting_address; // Filled by the lock init function with user-space pointer to qnode_allocation_array.

#ifdef HYBRID_MCS
hybrid_qnode_ptr queue_lock[MAX_NUMBER_LOCKS];
#ifdef HYBRID_EPOCH
uint64_t dummy_node_enqueued[MAX_NUMBER_LOCKS];
uint64_t blocking_nodes[MAX_NUMBER_LOCKS];
#endif
#endif

char _license[4] SEC("license") = "GPL";

typedef hybrid_qnode_ptr map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
	__uint(max_entries, (MAX_NUMBER_LOCKS * MAX_NUMBER_THREADS));
} nodes_map SEC(".maps");

#ifndef HYBRID_EPOCH
/*
 * Store whether a thread is currently preempted.
 */
struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u32);
	__uint(max_entries, (MAX_NUMBER_LOCKS * MAX_NUMBER_THREADS));
} preempted_map SEC(".maps");
#endif

static int on_preemption(u64 time, u32 tid, hybrid_qnode_ptr holder)
{
	int lock_id = holder->lock_id;
	if (!(lock_id >= 0 && lock_id < MAX_NUMBER_LOCKS)) // Weird negative to please the verifier
		return 1;																				 // Should never happen

#ifdef HYBRID_EPOCH
	// Prevent 2nd preemption of the same thread to re-enqueue the dummy_node.
	if (__sync_val_compare_and_swap(&dummy_node_enqueued[lock_id], 0, 1) != 0)
	{
		DPRINT("Dummy_node already enqueued.");
		return 1;
	}

	int id;
	hybrid_qnode_ptr curr, temp;
	hybrid_qnode_ptr dummy_node = &qnode_allocation_array[lock_id * MAX_NUMBER_THREADS];
	hybrid_qnode_ptr dummy_pointer = qnode_allocation_starting_address + lock_id * MAX_NUMBER_THREADS;

	curr = holder->next;
	if (!curr)
	{ // Can happen if system is over-subscribed by other apps or other locks. Nothing to do.
		DPRINT("Holder has no next");
		__sync_lock_test_and_set(&dummy_node_enqueued[lock_id], 0);
		return 1;
	}

	// <LOCK_DUMMY_NODE>
	dummy_node->next = NULL;
	hybrid_qnode_ptr pred = (hybrid_qnode_ptr)__sync_lock_test_and_set(&queue_lock[lock_id], dummy_pointer);
	if (pred == NULL)
	{
		bpf_printk("Error: Queue is empty. Holder should at least be in the queue.");
		return 1;
	}

	dummy_node->waiting = 1;
	if (QNODE_USER_TO_KERNEL(pred, id, temp))
		temp->next = dummy_pointer;
	// </LOCK_DUMMY_NODE>

	int i;
	for (i = 0; i < MAX_NUMBER_THREADS; i++)
	{
		if (QNODE_USER_TO_KERNEL(curr, id, temp))
		{
			if (temp->waiting != 1)
			{
				bpf_printk("Error: Thread was not waiting");
				return 1;
			}

			temp->should_block = 1;
			curr = temp->next;
		}

		if (!curr)
		{
			bpf_printk("Error: No dummy node.");
			return 1;
		}

		if (curr == dummy_pointer)
			break;
	}

	__sync_fetch_and_add(&blocking_nodes[lock_id], i + 1);
	DPRINT("Walked %d nodes", i + 1);

#else
	preempted_at[lock_id] = time;
	long ret = bpf_map_update_elem(&preempted_map, &tid, &tid, BPF_NOEXIST);
	if (ret < 0)
		bpf_printk("Error on map update.");
#endif
	return 0;
}

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u64 time = bpf_ktime_get_ns();
	struct pt_regs *regs;
	u32 key;
	int id;
	hybrid_qnode_ptr *temp, qnode;

	/*
	 * Clear preempted status of next thread.
	 */
	key = next->pid;
	temp = bpf_map_lookup_elem(&nodes_map, &key);
	if (temp && QNODE_USER_TO_KERNEL(*temp, id, qnode))
	{
		qnode->is_running = 1;

#ifndef HYBRID_EPOCH
		if (bpf_map_delete_elem(&preempted_map, &key) == 0)
		{
			DPRINT("%s (%d) rescheduled after %s (%d) at %ld", next->comm, next->pid, prev->comm, prev->pid, time);
			id = qnode->lock_id;
			if (!(id >= 0 && id < MAX_NUMBER_LOCKS))	 // Weird negative to please the verifier
				return 1;																 // Should never happen
			preempted_at[id] = 18446744073709551615UL; // ULONG_MAX
		}
#endif
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
	temp = bpf_map_lookup_elem(&nodes_map, &key);
	if (!temp || !QNODE_USER_TO_KERNEL(*temp, id, qnode))
		return 0;

	qnode->is_running = 0;

	/*
	 * Ignore preemption if the thread was not locking.
	 */
	if (!qnode->is_locking)
		return 0;

	/*
	 * Retrieve preemption address.
	 */
	u64 user_stack[MAX_STACK_TRACE_DEPTH];
	long user_stack_size = bpf_get_task_stack(prev, user_stack, MAX_STACK_TRACE_DEPTH * sizeof(u64), BPF_F_USER_STACK);
	if (user_stack_size < 0)
	{
		DPRINT("Error getting stack (%ld).", user_stack_size);
		return 0;
	}

	int i;
	for (i = 0; i < user_stack_size / sizeof(u64); i++)
	{
		/*
		 * Ignore preemptions before enqueue.
		 */
		if ((u64)addresses.lock <= user_stack[i] && user_stack[i] < (u64)addresses.lock_spin)
		{
			DPRINT("[%d] Ignored not enqueued", i);
			return 0;
		}

		/*
		 * Ignore preemptions after lock release.
		 */
		if ((u64)addresses.unlock_end <= user_stack[i] && user_stack[i] < (u64)addresses.unlock_end_b)
		{
			DPRINT("[%d] Ignored after release", i);
			return 0;
		}

		/*
		 * No need to check registers if preemption happened deeper
		 * than the lock/unlock functions.
		 */
		if (i == 0)
			regs = (struct pt_regs *)bpf_task_pt_regs(prev);

		/*
		 * Ignore preemptions if lock was not free (prev != NULL)
		 * Only needs to be checked on first stack address.
		 */
		if (i == 0 && (u64)addresses.lock_check_rax_null <= user_stack[i] && user_stack[i] < (u64)addresses.lock_spin &&
				(void *)regs->ax != NULL)
		{
			DPRINT("[%d] Ignored pred != NULL", i);
			return 0;
		}

		/*
		 * Ignore preemptions while spinning if
		 * previous node did not release lock (waiting != 0)
		 */
		if ((u64)addresses.lock_spin <= user_stack[i] && user_stack[i] < (u64)addresses.lock_end &&
				qnode->waiting != 0)
		{
			DPRINT("[%d] Ignored while spinning", i);
			return 0;
		}

		/*
		 * Ignore preemptions in unlock atomic operations if successful.
		 * Only if preemption happened in unlock function.
		 */
		if ((u64)addresses.unlock_check_zero_flag1 == user_stack[i] || (u64)addresses.unlock_check_zero_flag2 == user_stack[i])
		{
			if (i != 0)
			{
				bpf_printk("Error: unlock atomic operation found %d deep in stack.", i);
				return 0;
			}

			if (regs->flags & X86_EFLAGS_ZF)
			{
				DPRINT("Ignored while in unlock atomic operation");
				return 0;
			}
		}
	}

	DPRINT("%s (%d) preempted to %s (%d) at %ld", prev->comm, prev->pid, next->comm, next->pid, time);
	return on_preemption(time, key, qnode);
}
