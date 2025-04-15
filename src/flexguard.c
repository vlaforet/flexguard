/*
 * File: flexguard.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a hybrid lock with MCS, CLH or Ticket spin locks.
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

#include "flexguard.h"

#ifdef BPF
#include <bpf/libbpf.h>
#include "flexguard.skel.h"
#endif

_Atomic(int) thread_count = 1;
_Atomic(int) lock_count = 0;
flexguard_qnode_ptr qnode_allocation_array;

preempted_count_t *preempted_count;

#ifndef BLOCKING_CONDITION
#define BLOCKING_CONDITION(the_lock) *preempted_count
#endif

__thread int thread_id = -1;

#ifdef BPF
struct bpf_map *nodes_map;
#endif

static inline flexguard_qnode_ptr get_me()
{
    if (UNLIKELY(thread_id < 0))
    {
        thread_id = atomic_fetch_add(&thread_count, 1);
        CHECK_NUMBER_THREADS_FATAL(thread_id);

        flexguard_qnode_ptr qnode = &qnode_allocation_array[thread_id];

#ifdef BPF
        // Register thread in BPF map
        __u32 tid = gettid();
        int err = bpf_map__update_elem(nodes_map, &tid, sizeof(tid), &thread_id, sizeof(thread_id), BPF_ANY);
        if (err)
            fprintf(stderr, "Failed to register thread with BPF: %d\n", err);

        qnode->is_locking = 0;
#endif

#ifdef HYBRID_TICKET
        qnode->ticket = 0;
#elif defined(HYBRID_CLH)
        qnode->done = 1;
        qnode->pred = NULL;
#elif defined(HYBRID_MCS)
        qnode->waiting = 0;
        qnode->next = NULL;
#endif
        MEM_BARRIER;
    }

    return &qnode_allocation_array[thread_id];
}

static inline void mcs_unlock(flexguard_lock_t *the_lock, flexguard_qnode_ptr qnode)
{
    if (!qnode->next) // I seem to have no successor
    {
        // Trying to fix global pointer
        if (__sync_val_compare_and_swap(&the_lock->queue, qnode, NULL) == qnode)
            return;

        while (!qnode->next)
            PAUSE;
    }

    qnode->next->waiting = 0;
}

void flexguard_lock(flexguard_lock_t *the_lock)
{
    flexguard_qnode_ptr qnode = get_me();
#ifdef BPF
    __asm__ volatile("fg_lock:" ::: "memory");

    qnode->is_locking = 1;

    MEM_BARRIER;
#endif

    if (!the_lock->lock_value)
    {
#ifdef TIMESLICE_EXTENSION
        extend();
#endif
        if (__sync_val_compare_and_swap(&the_lock->lock_value, 0, 1) == 0)
            return;
#ifdef TIMESLICE_EXTENSION
        unextend();
#endif
    }

mcs_enqueue:

    // LOCK MCS
    uint8_t enqueued = 0;
    if (!BLOCKING_CONDITION(the_lock))
    {
        enqueued = 1;
        DASSERT(the_lock->queue != qnode);

        qnode->next = NULL;
        qnode->waiting = 1; // word on which to spin

        // Register rax stores the current qnode and will contain the previous value of
        // the queue after the xchgq operation.
        register flexguard_qnode_ptr pred __asm__("rcx") = qnode;

        /*
         *  Exchange pred (rcx) and queue head.
         *  Store the pointer to the queue head in a register.
         *  Uses the value in rax (pred) as an exchange parameter.
         */
        __asm__ volatile("xchgq %0, (%1)" : "+r"(pred) : "r"(&the_lock->queue) : "memory");

#ifdef BPF
        __asm__ volatile("fg_lock_check_rcx_null:" ::: "memory");
#endif
        if (pred != NULL) /* lock was not free */
        {
            MEM_BARRIER;
            pred->next = qnode; // make pred point to me

            while (qnode->waiting != 0 && !BLOCKING_CONDITION(the_lock))
                PAUSE;
        }
#ifdef BPF
        __asm__ volatile("fg_lock_end:" ::: "memory");
#endif
    }

#ifdef TIMESLICE_EXTENSION
    extend();
#endif

    int state = the_lock->lock_value;
    if (state == 0)
        state = __sync_val_compare_and_swap(&the_lock->lock_value, 0, 1);
    while (state != 0)
    {
        if (BLOCKING_CONDITION(the_lock))
        {
            if (enqueued)
            {
                mcs_unlock(the_lock, qnode);
                enqueued = 0;
            }
            if (the_lock->lock_value != 2)
                state = __sync_lock_test_and_set(&the_lock->lock_value, 2);
            if (state != 0)
            {
#ifdef TIMESLICE_EXTENSION
                unextend_light();
#endif
                futex_wait((void *)&the_lock->lock_value, 2);
#ifdef TIMESLICE_EXTENSION
                extend_light();
#endif

                state = __sync_lock_test_and_set(&the_lock->lock_value, 2);
                if (state != 0)
                {
                    if (!BLOCKING_CONDITION(the_lock))
                        goto mcs_enqueue;
                }
            }
        }
        else
        {
            PAUSE;
            if (the_lock->lock_value == 0)
                state = __sync_val_compare_and_swap(&the_lock->lock_value, 0, 1);
        }
    }

    // UNLOCK MCS
    if (enqueued)
        mcs_unlock(the_lock, qnode);
}

void flexguard_unlock(flexguard_lock_t *the_lock)
{
    if (__sync_lock_test_and_set(&the_lock->lock_value, 0) != 1)
        futex_wake((void *)&the_lock->lock_value, 1);

#ifdef TIMESLICE_EXTENSION
    unextend();
#endif

#ifdef BPF
    MEM_BARRIER;
    qnode_allocation_array[thread_id].is_locking = 0; // Assuming qnode has already been initialized.
#endif
}

#ifdef BPF
static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
#ifdef DEBUG
    return vfprintf(stderr, format, args);
#else
    return 0;
#endif
}
#endif

#ifdef BPF
static void deploy_bpf_code()
{
    struct flexguard_bpf *skel;
    int err;

    // Open BPF skeleton
    libbpf_set_print(libbpf_print_fn);
    skel = flexguard_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        exit(EXIT_FAILURE);
    }

#ifdef HYBRID_MCS
    extern char fg_lock;
    skel->bss->addresses.lock = &fg_lock;

    extern char fg_lock_check_rcx_null;
    skel->bss->addresses.lock_check_rcx_null = &fg_lock_check_rcx_null;

    extern char fg_lock_end;
    skel->bss->addresses.lock_end = &fg_lock_end;
#endif

    preempted_count = &skel->bss->preempted_count;
    qnode_allocation_array = skel->bss->qnodes;

    // Load BPF skeleton
    err = flexguard_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton (%d)\n", err);
        flexguard_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }

    // Store map
    nodes_map = skel->maps.nodes_map;

    // Attach BPF skeleton
    err = flexguard_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton (%d)\n", err);
        flexguard_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }
}
#endif

int flexguard_init(flexguard_lock_t *the_lock)
{
    the_lock->lock_value = 0;

    static volatile uint8_t init_lock = 0;
    if (exactly_once(&init_lock) == 0)
    {
#ifdef BPF
        deploy_bpf_code();
#else
        // Initialize things without BPF
        qnode_allocation_array = malloc(MAX_NUMBER_THREADS * sizeof(flexguard_qnode_t));

        preempted_count = malloc(sizeof(preempted_count_t));
        *preempted_count = 0;
#endif
        init_lock = 2;
    }

#ifdef HYBRID_TICKET
    the_lock->ticket_lock.calling = 1;
    the_lock->ticket_lock.next = 0;
#elif defined(HYBRID_CLH)
    *(the_lock->queue) = (flexguard_qnode_ptr)malloc(sizeof(flexguard_qnode_t)); // CLH keeps an empty node
    (*the_lock->queue)->done = 1;
    (*the_lock->queue)->pred = NULL;
#elif defined(HYBRID_MCS)
    the_lock->queue = NULL;
#endif

    MEM_BARRIER;
    return 0;
}

void flexguard_destroy(flexguard_lock_t *the_lock)
{
    // Nothing to do
}

/*
 *  Condition Variables
 */

int flexguard_cond_init(flexguard_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int flexguard_cond_wait(flexguard_cond_t *cond, flexguard_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    flexguard_unlock(the_lock);

    while (target > seq)
    {
        if (BLOCKING_CONDITION(the_lock))
            futex_wait(&cond->seq, seq);
        else
            PAUSE;
        seq = cond->seq;
    }
    flexguard_lock(the_lock);
    return 0;
}

int flexguard_cond_timedwait(flexguard_cond_t *cond, flexguard_lock_t *the_lock, const struct timespec *ts)
{
    const struct timespec now;
    clock_gettime(CLOCK_REALTIME, (struct timespec *)&now);
    if ((now.tv_sec > ts->tv_sec) || (now.tv_sec == ts->tv_sec && now.tv_nsec >= ts->tv_nsec))
        return 1;

    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    flexguard_unlock(the_lock);

    while (target > seq)
    {
        clock_gettime(CLOCK_REALTIME, (struct timespec *)&now);
        if ((now.tv_sec > ts->tv_sec) || (now.tv_sec == ts->tv_sec && now.tv_nsec >= ts->tv_nsec))
            return 1;

        if (BLOCKING_CONDITION(the_lock))
            futex_wait_timeout_abs(&cond->seq, seq, (struct timespec *)ts);
        else
            PAUSE;
        seq = cond->seq;
    }
    flexguard_lock(the_lock);
    return 0;
}

int flexguard_cond_signal(flexguard_cond_t *cond)
{
    cond->seq++;
    futex_wake(&cond->seq, 1);
    return 0;
}

int flexguard_cond_broadcast(flexguard_cond_t *cond)
{
    cond->seq = cond->target;
    futex_wake(&cond->seq, INT_MAX);
    return 0;
}

int flexguard_cond_destroy(flexguard_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}