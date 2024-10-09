/*
 * File: hybridv2.c
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

#include "hybridv2.h"

#ifdef BPF
#include <bpf/libbpf.h>
#include "hybridv2.skel.h"
#endif

_Atomic(int) thread_count = 1;
_Atomic(int) lock_count = 0;
hybrid_qnode_ptr qnode_allocation_array;

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
hybrid_lock_info_t *lock_info;
#define get_preempted_count(the_lock) the_lock->preempted_count
#else
preempted_count_t *preempted_count;
#define get_preempted_count(the_lock) preempted_count
#endif

__thread int thread_id = -1;

#ifdef BPF
struct bpf_map *nodes_map;
#endif

static inline hybrid_qnode_ptr get_me()
{
    if (UNLIKELY(thread_id < 0))
    {
        thread_id = atomic_fetch_add(&thread_count, 1);
        CHECK_NUMBER_THREADS_FATAL(thread_id);

#ifdef BPF
        // Register thread in BPF map
        __u32 tid = gettid();
        int err = bpf_map__update_elem(nodes_map, &tid, sizeof(tid), &thread_id, sizeof(thread_id), BPF_ANY);
        if (err)
            fprintf(stderr, "Failed to register thread with BPF: %d\n", err);
#endif

        hybrid_qnode_ptr qnode = &qnode_allocation_array[thread_id];

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
        qnode->locking_id = -1;
#else
        qnode->is_locking = 0;
#endif

        qnode->is_running = 1;

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

void hybridv2_lock(hybridv2_lock_t *the_lock)
{
    struct timespec futex_timeout;
    futex_timeout.tv_sec = 0;
    futex_timeout.tv_nsec = 1000000;

    hybrid_qnode_ptr qnode = get_me();
#ifdef BPF
    __asm__ volatile("bhl_lock:" ::: "memory");

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
    qnode->locking_id = the_lock->id;
#else
    qnode->is_locking = 1;
#endif

    MEM_BARRIER;
#endif

    if (!the_lock->lock_value && __sync_lock_test_and_set(&the_lock->lock_value, 1) == 0)
    {
#ifdef TRACING
        if (the_lock->tracing_fn)
            the_lock->tracing_fn(getticks(), TRACING_EVENT_ACQUIRED_STOLEN, NULL, the_lock->tracing_fn_data);
#endif

        return;
    }

    // LOCK MCS
    uint8_t enqueued = 0;
    if (!*get_preempted_count(the_lock))
    {
        enqueued = 1;
        DASSERT(the_lock->queue != qnode);

        qnode->next = NULL;
        qnode->waiting = 1; // word on which to spin

        // Register rax stores the current qnode and will contain the previous value of
        // the queue after the xchgq operation.
        register hybrid_qnode_ptr pred __asm__("rcx") = qnode;

        /*
         *  Exchange pred (rcx) and queue head.
         *  Store the pointer to the queue head in a register.
         *  Uses the value in rax (pred) as an exchange parameter.
         */
        __asm__ volatile("xchgq %0, (%1)" : "+r"(pred) : "r"(&the_lock->queue) : "memory");

#ifdef BPF
        __asm__ volatile("bhl_lock_check_rcx_null:" ::: "memory");
#endif
        if (pred != NULL) /* lock was not free */
        {
            MEM_BARRIER;
            pred->next = qnode; // make pred point to me

            while (qnode->waiting != 0 && !*get_preempted_count(the_lock))
                PAUSE;
        }
#ifdef BPF
        __asm__ volatile("bhl_lock_end:" ::: "memory");
#endif
    }

#ifdef TRACING
    if (the_lock->tracing_fn)
        the_lock->tracing_fn(getticks(), enqueued ? TRACING_EVENT_ACQUIRED_SPIN : TRACING_EVENT_ACQUIRED_BLOCK, NULL, the_lock->tracing_fn_data);
#endif

    while (the_lock->lock_value || __sync_lock_test_and_set(&the_lock->lock_value, 1) != 0)
    {
        if (*get_preempted_count(the_lock))
        {
            __sync_fetch_and_add(&the_lock->waiter_count, 1);
            futex_wait_timeout((void *)&the_lock->lock_value, 1, &futex_timeout);
            __sync_fetch_and_sub(&the_lock->waiter_count, 1);
        }
        else
            PAUSE;
    }

    // UNLOCK MCS
    if (enqueued)
    {
        if (!qnode->next) // I seem to have no successor
        {
            // Trying to fix global pointer
            if (__sync_val_compare_and_swap(&the_lock->queue, qnode, NULL) == qnode)
                goto clean_up_next_waiter_preempted;

            while (!qnode->next)
                PAUSE;
        }

        if (!qnode->next->is_running)
        {
            if (!the_lock->next_waiter_preempted)
                __sync_fetch_and_add(get_preempted_count(the_lock), 1);
            the_lock->next_waiter_preempted = the_lock->queue;
        }

        qnode->next->waiting = 0;
    }

clean_up_next_waiter_preempted:
    if (the_lock->next_waiter_preempted == qnode)
    {
        __sync_fetch_and_sub(get_preempted_count(the_lock), 1);
        the_lock->next_waiter_preempted = NULL;
    }
}

void hybridv2_unlock(hybridv2_lock_t *the_lock)
{
    COMPILER_BARRIER;
    the_lock->lock_value = 0;

    if (the_lock->waiter_count > 0)
        futex_wake((void *)&the_lock->lock_value, 1);

    MEM_BARRIER;
#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
    qnode_allocation_array[thread_id].locking_id = -1; // Assuming qnode has already been initialized.
#else
    qnode_allocation_array[thread_id].is_locking = 0; // Assuming qnode has already been initialized.
#endif
}

#ifdef BPF
static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
#if DEBUG == 1
    return vfprintf(stderr, format, args);
#else
    return 0;
#endif
}
#endif

#ifdef BPF
static void deploy_bpf_code()
{
    struct hybridv2_bpf *skel;
    int err;

    // Open BPF skeleton
    libbpf_set_print(libbpf_print_fn);
    skel = hybridv2_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        exit(EXIT_FAILURE);
    }

#ifdef HYBRID_MCS
    extern char bhl_lock;
    skel->bss->addresses.lock = &bhl_lock;

    extern char bhl_lock_check_rcx_null;
    skel->bss->addresses.lock_check_rcx_null = &bhl_lock_check_rcx_null;

    extern char bhl_lock_end;
    skel->bss->addresses.lock_end = &bhl_lock_end;
#endif

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
    lock_info = skel->bss->lock_info;
#else
    preempted_count = &skel->bss->preempted_count;
#endif

    qnode_allocation_array = skel->bss->qnodes;

    // Load BPF skeleton
    err = hybridv2_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton (%d)\n", err);
        hybridv2_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }

    // Store map
    nodes_map = skel->maps.nodes_map;

    // Attach BPF skeleton
    err = hybridv2_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton (%d)\n", err);
        hybridv2_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }
}
#endif

int hybridv2_init(hybridv2_lock_t *the_lock)
{
    the_lock->id = atomic_fetch_add(&lock_count, 1);
    if (the_lock->id >= MAX_NUMBER_LOCKS)
    {
        fprintf(stderr, "Too many locks. Increase MAX_NUMBER_LOCKS in platform_defs.h.\n");
        exit(EXIT_FAILURE);
    }

    the_lock->lock_value = 0;
    the_lock->waiter_count = 0;
    the_lock->next_waiter_preempted = 0;

    static volatile uint8_t init_lock = 0;
    if (exactly_once(&init_lock) == 0)
    {
#ifdef BPF
        deploy_bpf_code();
#else
        // Initialize things without BPF
        qnode_allocation_array = malloc(MAX_NUMBER_THREADS * sizeof(hybrid_qnode_t));

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
        lock_info = malloc(MAX_NUMBER_LOCKS * sizeof(hybrid_lock_info_t));
#else
        preempted_count = malloc(sizeof(preempted_count_t));
        preempted_count = 0;
#endif
#endif
        init_lock = 2;
    }

#ifdef HYBRIDV2_LOCAL_PREEMPTIONS
    the_lock->preempted_count = &lock_info[the_lock->id].preempted_count;
    (*the_lock->preempted_count) = 0;
#endif

#ifdef HYBRID_TICKET
    the_lock->ticket_lock.calling = 1;
    the_lock->ticket_lock.next = 0;
#elif defined(HYBRID_CLH)
    *(the_lock->queue) = (hybrid_qnode_ptr)malloc(sizeof(hybrid_qnode_t)); // CLH keeps an empty node
    (*the_lock->queue)->done = 1;
    (*the_lock->queue)->pred = NULL;
#elif defined(HYBRID_MCS)
    the_lock->queue = NULL;
#endif

    MEM_BARRIER;
    return 0;
}

void hybridv2_destroy(hybridv2_lock_t *the_lock)
{
    // Nothing to do
}

#ifdef TRACING
void set_tracing_fn(hybridv2_lock_t *the_lock, void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data), void *tracing_fn_data)
{
    the_lock->tracing_fn_data = tracing_fn_data;
    the_lock->tracing_fn = tracing_fn;
}
#endif

/*
 *  Condition Variables
 */

int hybridv2_cond_init(hybridv2_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int hybridv2_cond_wait(hybridv2_cond_t *cond, hybridv2_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    hybridv2_unlock(the_lock);

    while (target > seq)
    {
        if (*get_preempted_count(the_lock))
            futex_wait(&cond->seq, seq);
        else
            PAUSE;
        seq = cond->seq;
    }
    hybridv2_lock(the_lock);
    return 0;
}

int hybridv2_cond_timedwait(hybridv2_cond_t *cond, hybridv2_lock_t *the_lock, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int hybridv2_cond_signal(hybridv2_cond_t *cond)
{
    cond->seq++;
    futex_wake(&cond->seq, 1);
    return 0;
}

int hybridv2_cond_broadcast(hybridv2_cond_t *cond)
{
    cond->seq = cond->target;
    futex_wake(&cond->seq, INT_MAX);
    return 0;
}

int hybridv2_cond_destroy(hybridv2_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}