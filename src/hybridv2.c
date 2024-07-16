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
#include "hybridv2.skel.h"
#endif

_Atomic(int) thread_count = 1;
_Atomic(int) lock_count = 0;
hybrid_qnode_ptr qnode_allocation_array;

hybrid_lock_info_t *lock_info;
__thread int thread_id = -1;

#ifdef BPF
struct bpf_map *nodes_map;
#endif

static hybrid_qnode_ptr get_me()
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

        qnode->locking_id = -1;
        qnode->is_running = 1;
        qnode->is_holder_preempted = 0;
        qnode->wait_for_successor = 0;

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
    hybrid_qnode_ptr qnode = get_me();
#ifdef BPF
    asm volatile("bhl_lock:" ::: "memory");
    DASSERT(qnode->locking_id == -1);
    qnode->locking_id = the_lock->id;
    MEM_BARRIER;
#endif

    if (__sync_lock_test_and_set(&the_lock->lock_value, 1) == 0)
        return;

    // LOCK MCS
    uint8_t enqueued = 0;
    if (!*the_lock->is_blocking)
    {
        enqueued = 1;
        DASSERT(*the_lock->queue_lock != qnode);

        qnode->next = NULL;
        qnode->waiting = 1; // word on which to spin

        // Register rax stores the current qnode and will contain the previous value of
        // the queue_lock after the xchgq operation.
        register hybrid_qnode_ptr pred asm("rcx") = qnode;

        /*
         *  Exchange pred (rcx) and queue head.
         *  Store the pointer to the queue head in a register.
         *  Uses the value in rax (pred) as an exchange parameter.
         */
        asm volatile("xchgq %0, (%1)" : "+r"(pred) : "r"(the_lock->queue_lock) : "memory");

#ifdef BPF
        asm volatile("bhl_lock_check_rcx_null:" ::: "memory");
#endif
        if (pred != NULL) /* lock was not free */
        {
            MEM_BARRIER;
            pred->next = qnode; // make pred point to me

            while (qnode->waiting != 0 && !*the_lock->is_blocking)
                PAUSE;
        }
#ifdef BPF
        asm volatile("bhl_lock_end:" ::: "memory");
#endif
    }

    while (__sync_lock_test_and_set(&the_lock->lock_value, 1) != 0)
    {
        if (*the_lock->is_blocking)
        {
#ifdef BPF
            asm volatile("bhl_futex_wait:" ::: "memory");
#endif
            futex_wait((void *)&the_lock->lock_value, 1);
#ifdef BPF
            asm volatile("bhl_futex_wait_end:" ::: "memory");
#endif
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
            if (__sync_val_compare_and_swap(the_lock->queue_lock, qnode, NULL) == qnode)
                return;

            if (!qnode->next)
            {
                qnode->wait_for_successor = 1;
                return;
            }

            // TODO: if (!qnode->next->is_running) { *the_lock->is_blocking = 1; qnode->next->is_holder_preempted = 1; }
        }
        qnode->next->waiting = 0;
    }
}

void hybridv2_unlock(hybridv2_lock_t *the_lock)
{
    COMPILER_BARRIER;
    the_lock->lock_value = 0;

    hybrid_qnode_ptr qnode = &qnode_allocation_array[thread_id]; // Assuming qnode has already been initialized.
    if (qnode->wait_for_successor)
    {
        while (!qnode->next)
            PAUSE;
        qnode->next->waiting = 0;
        qnode->wait_for_successor = 0;
    }

    futex_wake((void *)&the_lock->lock_value, 1);

    MEM_BARRIER;
    qnode->locking_id = -1;
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

    extern char bhl_futex_wait;
    skel->bss->addresses.futex_wait = &bhl_futex_wait;

    extern char bhl_futex_wait_end;
    skel->bss->addresses.futex_wait_end = &bhl_futex_wait_end;

    extern char bhl_lock_end;
    skel->bss->addresses.lock_end = &bhl_lock_end;
#endif

    lock_info = skel->bss->lock_info;

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

    int fd = bpf_map__fd(skel->maps.array_map);
    qnode_allocation_array = mmap(NULL, MAX_NUMBER_THREADS * sizeof(hybrid_qnode_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (qnode_allocation_array == MAP_FAILED)
    {
        perror("qnode_allocation_array mmap");
        exit(EXIT_FAILURE);
    }

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

    static volatile uint8_t init_lock = 0;
    if (exactly_once(&init_lock) == 0)
    {
#ifdef BPF
        deploy_bpf_code();
#else
        // Initialize things without BPF
        qnode_allocation_array = malloc(MAX_NUMBER_THREADS * sizeof(hybrid_qnode_t));
        lock_info = malloc(MAX_NUMBER_LOCKS * sizeof(hybrid_lock_info_t));
#endif
        init_lock = 2;
    }

#ifdef HYBRID_MCS
    the_lock->queue_lock = &lock_info[the_lock->id].queue_lock;
#endif

    the_lock->is_blocking = &lock_info[the_lock->id].is_blocking;
    (*the_lock->is_blocking) = 0;

#ifdef HYBRID_TICKET
    the_lock->ticket_lock.calling = 1;
    the_lock->ticket_lock.next = 0;
#elif defined(HYBRID_CLH)
    *(the_lock->queue_lock) = (hybrid_qnode_ptr)malloc(sizeof(hybrid_qnode_t)); // CLH keeps an empty node
    (*the_lock->queue_lock)->done = 1;
    (*the_lock->queue_lock)->pred = NULL;
#elif defined(HYBRID_MCS)
    *(the_lock->queue_lock) = NULL;
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
        if (*the_lock->is_blocking)
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