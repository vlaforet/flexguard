/*
 * File: flexguard.bpf.c
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
#include "flexguard_bpf.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <asm/processor-flags.h>
#include "bpf_fixes.bpf.h"

#ifdef DEBUG
#define DPRINT(args...) bpf_printk(args);
#else
#define DPRINT(...)
#endif

#define MAX_STACK_TRACE_DEPTH 8

hybrid_addresses_t addresses;
flexguard_qnode_t qnodes[MAX_NUMBER_THREADS];

num_preempted_cs_t num_preempted_cs = 0;

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
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, u32);
	__type(value, u32);
	__uint(max_entries, MAX_NUMBER_THREADS);
} is_preempted_map SEC(".maps");

/*
 * Will return 1 if the thread is detected as a critical thread.
 * A critical thread holds the MCS or TAS lock.
 */
static int is_critical_thread(struct task_struct *task, flexguard_qnode_ptr qnode)
{
	u64 user_stack[MAX_STACK_TRACE_DEPTH];
	long user_stack_size = bpf_get_task_stack(task, user_stack, MAX_STACK_TRACE_DEPTH * sizeof(u64), BPF_F_USER_STACK);
	if (user_stack_size <= 0)
	{
		bpf_printk("Error getting stack (%ld).", user_stack_size);
		return 0;
	}

	if ((u64)addresses.lock <= user_stack[0] && user_stack[0] < (u64)addresses.fastpath)
	{
		return 0; // Not critical before fastpath.
	}

	if ((u64)addresses.fastpath_out <= user_stack[0] && user_stack[0] < (u64)addresses.lock_check_rcx_null)
	{
		return 0; // Not critical before enqueue.
	}

	if ((u64)addresses.lock_check_rcx_null <= user_stack[0] && user_stack[0] < (u64)addresses.phase2)
	{
		struct pt_regs *regs = (struct pt_regs *)bpf_task_pt_regs(task);

		/*
		 * Critical preemption in waiting phase1 iff:
		 * - rcx is NULL (lock was free)
		 * - or if the thread is not waiting anymore (waiting == 0).
		 */
		return (void *)regs->cx == NULL || qnode->waiting == 0;
	}

	if ((u64)addresses.fastpath <= user_stack[0] && user_stack[0] < (u64)addresses.fastpath_out)
	{
		struct pt_regs *regs = (struct pt_regs *)bpf_task_pt_regs(task);

		/*
		 * Critical preemption in Fastpath iff:
		 * - CAS succeeded (rax != 0).
		 */
		return ((int)regs->ax) != 0;
	}

	return 1;
}

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_switch_btf, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	u32 key;
	flexguard_qnode_ptr qnode;
	int *thread_id;

	/*
	 * Clear preempted status of next thread.
	 * Optimization: skip if next is a kernel thread.
	 */
	if (!(next->flags & 0x00200000)) // PF_KTHREAD
	{
		key = next->pid;
		if (bpf_map_delete_elem(&is_preempted_map, &key) == 0)
			__sync_fetch_and_add(&num_preempted_cs, -1);
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
	if (!thread_id || *thread_id < 0 || *thread_id >= MAX_NUMBER_THREADS || !(qnode = &qnodes[*thread_id]))
		return 0;

	if (get_task_state(prev) & ((((TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE | TASK_STOPPED | TASK_TRACED | EXIT_DEAD | EXIT_ZOMBIE | TASK_PARKED) + 1) << 1) - 1))
		return 0;

	if (qnode->cs_counter && is_critical_thread(prev, qnode))
	{
		DPRINT("Detected preemption: %s (%d) -> %s (%d)", prev->comm, prev->pid, next->comm, next->pid);
		bpf_map_update_elem(&is_preempted_map, &key, &key, BPF_NOEXIST);
		__sync_fetch_and_add(&num_preempted_cs, 1);
	}

	return 0;
}
