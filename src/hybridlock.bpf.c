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
	((id = (user_qnode - qnode_allocation_starting_address)) >= 0 && id < MAX_NUMBER_THREADS && (kern_qnode = &qnode_allocation_array[id]))

#ifndef HYBRID_EPOCH
unsigned long preempted_at;
#endif

hybrid_addresses_t addresses;
volatile hybrid_qnode_t qnode_allocation_array[MAX_NUMBER_THREADS];
hybrid_qnode_ptr qnode_allocation_starting_address; // Filled by the lock init function with user-space pointer to qnode_allocation_array.

#ifdef HYBRID_TICKET
uint32_t *ticket_calling;
#elif defined(HYBRID_MCS)
#ifdef HYBRID_EPOCH
volatile hybrid_qnode_t dummy_node;
hybrid_qnode_ptr dummy_pointer; // Filled by the lock init function with user-space pointer to dummy_node.

uint64_t dummy_node_enqueued = 0;
uint64_t blocking_nodes = 0;
hybrid_qnode_ptr queue_lock;
#endif
#endif

char _license[4] SEC("license") = "GPL";

typedef hybrid_qnode_ptr map_value;

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, map_value);
	__uint(max_entries, MAX_NUMBER_THREADS);
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
	__uint(max_entries, MAX_NUMBER_THREADS);
} preempted_map SEC(".maps");
#endif

static int on_preemption(u64 time, u32 tid)
{
#ifdef HYBRID_EPOCH
	// Prevent 2nd preemption of the same thread to re-enqueue the dummy_node.
	if (__sync_val_compare_and_swap(&dummy_node_enqueued, 0, 1) != 0)
	{
		DPRINT("Dummy_node already enqueued.");
		return 1;
	}

	int id;
	hybrid_qnode_ptr *holder, curr, temp;

	holder = bpf_map_lookup_elem(&nodes_map, &tid);
	if (!holder || !QNODE_USER_TO_KERNEL(*holder, id, temp))
	{
		bpf_printk("Error: Unexpected error while retrieving node");
		__sync_lock_test_and_set(&dummy_node_enqueued, 0);
		return 1;
	}

	curr = temp->next;
	if (!curr)
	{ // Can happen if system is over-subscribed by other apps or other locks. Nothing to do.
		DPRINT("Holder has no next");
		__sync_lock_test_and_set(&dummy_node_enqueued, 0);
		return 1;
	}

	// <LOCK_DUMMY_NODE>
	dummy_node.next = NULL;
	hybrid_qnode_ptr pred = (hybrid_qnode_ptr)__sync_lock_test_and_set(&queue_lock, dummy_pointer);
	if (pred == NULL)
	{
		bpf_printk("Error: Queue is empty. Holder should at least be in the queue.");
		return 1;
	}

	dummy_node.waiting = 1;
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

	__sync_fetch_and_add(&blocking_nodes, i + 1);
	DPRINT("Walked %d nodes", i + 1);

#else
	preempted_at = time;
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
			bpf_printk("%s (%d) rescheduled after %s (%d) at %ld", next->comm, next->pid, prev->comm, prev->pid, time);
			preempted_at = 18446744073709551615UL; // ULONG_MAX
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
	uint8_t flags = qnode->flags;

	/*
	 * If prev was preempted in a critical section.
	 */
	if (flags & HYBRID_FLAGS_IN_CS)
	{
		DPRINT("Preempted in CS.");
		return on_preemption(time, key);
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
		DPRINT("Error getting stack (%ld).", user_stack_size);
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
				DPRINT("Preempted at lock_check_rax_null.");
				return on_preemption(time, key);
			}
			return 0;
		}

		if ((u64)addresses.lock_spin <= user_stack && (u64)addresses.lock_end > user_stack && qnode->waiting == 0)
		{
			DPRINT("Preempted at spin. (%lld bytes away)", user_stack - (u64)addresses.lock_spin);
			return on_preemption(time, key);
		}

		if ((u64)addresses.lock_end <= user_stack)
		{
			DPRINT("Preempted at lock_end.");
			return on_preemption(time, key);
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
				DPRINT("Preempted in check_zero_flag.");
				return on_preemption(time, key);
			}
			return 0;
		}

		if ((u64)addresses.unlock_end > user_stack)
		{
			DPRINT("Preempted in unlock.");
			return on_preemption(time, key);
		}
	}

	return 0;
}
