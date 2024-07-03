/*
 * File: hybridlock.c
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

#include "hybridlock.h"

#ifdef BPF
#include "hybridlock.skel.h"

#ifndef HYBRID_EPOCH
#define NO_WAITER_DURATION_BACK_SPIN_NSECS 50000
#define CS_PREEMPTION_DURATION_TO_BLOCK_NSECS 100000
#define MINIMUM_DURATION_BETWEEN_SWITCHES_NSECS 10000000

static unsigned long get_nsecs()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}
#endif
#endif

_Atomic(int) thread_count = 1;
_Atomic(int) lock_count = 0;
hybrid_qnode_ptr qnode_allocation_array;

hybrid_lock_info_t *lock_info;
hybrid_thread_info_t *thread_info;
__thread int thread_id = -1;

#ifdef BPF
hybrid_addresses_t *addresses;
struct bpf_map *nodes_map;
#endif

#ifndef HYBRID_EPOCH
int global_blocking_id = 0;
#endif

static hybrid_qnode_ptr get_me(hybridlock_lock_t *the_lock)
{
    if (UNLIKELY(thread_id < 0))
    {
        thread_id = atomic_fetch_add(&thread_count, 1);
        CHECK_NUMBER_THREADS_FATAL(thread_id);

        // Init thread_info
        thread_info[thread_id].locking_id = -1;
        thread_info[thread_id].is_running = 0;
        thread_info[thread_id].is_holder_preempted = 0;
    }

    hybrid_qnode_ptr qnode = &qnode_allocation_array[the_lock->id + thread_id * MAX_NUMBER_LOCKS];
    if (UNLIKELY(!qnode->is_init))
    {
        qnode->lock_id = the_lock->id;
        qnode->is_init = 1;

#ifdef HYBRID_EPOCH
        qnode->should_block = 0;
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

#ifdef BPF
        // Register thread in BPF map
        __u32 tid = gettid();
        int err = bpf_map__update_elem(nodes_map, &tid, sizeof(tid), &thread_id, sizeof(thread_id), BPF_ANY);
        if (err)
            fprintf(stderr, "Failed to register thread with BPF: %d\n", err);
#endif
        MEM_BARRIER;
    }

    return qnode;
}

static inline int isfree_type(hybridlock_lock_t *the_lock, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
#ifdef HYBRID_TICKET
        if (the_lock->ticket_lock.calling - the_lock->ticket_lock.next != 1)
            return 0; // Not free
#elif defined(HYBRID_CLH)
        DASSERT(the_lock->queue_lock != NULL);
        if ((*the_lock->queue_lock)->done != 1)
            return 0; // Not free
#elif defined(HYBRID_MCS)
        DASSERT(the_lock->queue_lock != NULL);
        if ((*the_lock->queue_lock) != NULL)
            return 0; // Not free
#endif
        break;

    case LOCK_TYPE_FUTEX:
        if (the_lock->futex_lock != 0)
            return 0; // Not free
        break;
    default:
        fprintf(stderr, "Transition types cannot be free.\n");
        exit(EXIT_FAILURE);
    }
    return 1; // Free
}

__attribute__((noinline)) __attribute__((noipa)) static int lock_type(hybridlock_lock_t *the_lock, hybrid_qnode_ptr qnode, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
#ifdef BPF
        asm volatile("bhl_lock:" ::: "memory");
        thread_info[thread_id].locking_id = the_lock->id;
        MEM_BARRIER;
#endif

#ifdef HYBRID_TICKET
        qnode->ticket = __sync_add_and_fetch(&the_lock->ticket_lock.next, 1);

        uint32_t curr, distance;
        while (1)
        {
            curr = the_lock->ticket_lock.calling;
            if (curr == qnode->ticket)
                break; // Success

            distance = curr > qnode->ticket ? curr - qnode->ticket : qnode->ticket - curr;
            if (distance <= 1)
                PAUSE;
            else
                nop_rep(distance * 512);
        }

#elif defined(HYBRID_CLH)
        DASSERT(qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);

        qnode->done = 0;
        qnode->pred = (hybrid_qnode_ptr)SWAP_PTR(the_lock->queue_lock, (void *)qnode);

#if defined(BPF) && !defined(HYBRID_EPOCH)
        unsigned long now;
#endif
        while (qnode->pred->done == 0 && LOCK_CURR_TYPE(the_lock->lock_state) == lock_type)
        {
            PAUSE;

#if defined(BPF) && !defined(HYBRID_EPOCH)
            now = get_nsecs();
            if (
                now - CS_PREEMPTION_DURATION_TO_BLOCK_NSECS > *the_lock->preempted_at &&
                now - MINIMUM_DURATION_BETWEEN_SWITCHES_NSECS > (unsigned long)atomic_load(&the_lock->last_switched_at) &&
                __sync_bool_compare_and_swap(&the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_SPIN), LOCK_TRANSITION(LOCK_TYPE_SPIN, LOCK_TYPE_FUTEX)))
            {
                *the_lock->preempted_at = ULONG_MAX;
                atomic_store(&the_lock->last_switched_at, now);
            }
#endif
        }

        // Cannot abort properly. This only targets cases where every waiter aborts.
        if (LOCK_CURR_TYPE(the_lock->lock_state) != lock_type)
        {
            hybrid_qnode_ptr pred = qnode->pred;
            qnode->done = 1;
            qnode = pred;
            return 0; // Aborted
        }

#elif defined(HYBRID_MCS)
        DASSERT(qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
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
#ifdef BPF
            asm volatile("bhl_lock_spin:" ::: "memory");
#endif
            MEM_BARRIER;
            pred->next = qnode; // make pred point to me

#if defined(BPF) && !defined(HYBRID_EPOCH)
            unsigned long now;
#endif
            while (qnode->waiting != 0)
            {
                PAUSE;

#if !defined(HYBRID_EPOCH) && defined(BPF)
                if (global_blocking_id > the_lock->blocking_id && __sync_bool_compare_and_swap(&the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_SPIN), LOCK_TRANSITION(LOCK_TYPE_SPIN, LOCK_TYPE_FUTEX)))
                {
                    atomic_store(&the_lock->last_switched_at, now);
                    the_lock->blocking_id = global_blocking_id;
                }
#endif

#if defined(BPF) && !defined(HYBRID_EPOCH)
                if (*the_lock->preempted_at != ULONG_MAX)
                {
                    now = get_nsecs();
                    if (
                        now - CS_PREEMPTION_DURATION_TO_BLOCK_NSECS > *the_lock->preempted_at &&
                        now - MINIMUM_DURATION_BETWEEN_SWITCHES_NSECS > (unsigned long)atomic_load(&the_lock->last_switched_at) &&
                        __sync_bool_compare_and_swap(&the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_SPIN), LOCK_TRANSITION(LOCK_TYPE_SPIN, LOCK_TYPE_FUTEX)))
                    {
                        the_lock->blocking_id = atomic_fetch_add(&global_blocking_id, 1) + 1;
                        *the_lock->preempted_at = ULONG_MAX;
                        atomic_store(&the_lock->last_switched_at, now);
                    }
                }
#endif

                if (
#ifdef HYBRID_EPOCH
                    qnode->should_block == 1
#else
                    LOCK_CURR_TYPE(the_lock->lock_state) != lock_type
#endif
                    && __sync_bool_compare_and_swap(&qnode->waiting, 1, 2))
                {
#ifdef BPF
                    MEM_BARRIER;
                    thread_info[thread_id].locking_id = -1;
#endif
                    return 0; // Aborted
                }
            }
        }
#else
#error "Unknown Hybrid Lock Version"
#endif

#ifdef BPF
        asm volatile("bhl_lock_end:" ::: "memory");
#endif
        return 1; // Success

    case LOCK_TYPE_FUTEX:;
        int state;
#if defined(BPF) && !defined(HYBRID_EPOCH)
        atomic_store(&the_lock->last_waiter_at, get_nsecs());
#endif

        if ((state = __sync_val_compare_and_swap(&the_lock->futex_lock, 0, 1)) != 0)
        {
            if (state != 2)
                state = __sync_lock_test_and_set(&the_lock->futex_lock, 2);
            while (state != 0)
            {
                futex_wait((void *)&the_lock->futex_lock, 2);
                state = __sync_lock_test_and_set(&the_lock->futex_lock, 2);
            }
        }
        return 1;
    default:
        fprintf(stderr, "Transition types cannot be locked.\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

__attribute__((noinline)) __attribute__((noipa)) static void unlock_type(hybridlock_lock_t *the_lock, hybrid_qnode_ptr qnode, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
#ifdef HYBRID_TICKET
        the_lock->ticket_lock.calling++;
#elif defined(HYBRID_CLH)
        DASSERT(qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
        DASSERT(qnode->pred != NULL);

        hybrid_qnode_ptr pred = qnode->pred;
        qnode->done = 1;
        qnode = pred;
#elif defined(HYBRID_MCS)
        DASSERT(qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
        hybrid_qnode_ptr curr = qnode, succ;

        while (1) // Spin over aborted nodes
        {
            succ = curr->next;
            if (!succ) /* I seem to have no succ. */
            {
                /*
                 *  Try to fix global pointer (and break if CAS succeeds).
                 *  Store the pointer to the queue head and new value (NULL) in registers.
                 *  Store the compare value (curr) in the accumulator register.
                 */
                asm volatile("lock cmpxchgq %2, (%0)" : : "r"(the_lock->queue_lock), "a"(curr), "r"(NULL) : "memory");
                asm goto("bhl_unlock_check_zero_flag1: je %l[unlock_end]" : : : : unlock_end); // Do a jump to break here.

                do
                {
                    succ = curr->next;
                    PAUSE;
                } while (!succ); // wait for successor
            }

            curr = succ;

            // TODO: Do something with that information
            /*if (!succ->is_running)
                printf("Should block\n");*/

            /*
             *  Try to give the lock to the next waiter (and break if CAS succeeds).
             *  Store the pointer to the waiting flag and new value (0) in registers.
             *  Store the compare value (1) in the accumulator register.
             */
            asm volatile("lock cmpxchgb %2, (%0)" : : "r"(&succ->waiting), "a"((uint8_t)1), "r"((uint8_t)0) : "memory");
            asm goto("bhl_unlock_check_zero_flag2: je %l[unlock_end]" : : : : unlock_end); // Do a jump to break here.
        }
#endif

    unlock_end:
        asm volatile("bhl_unlock_end:" ::: "memory");
#ifdef BPF
        MEM_BARRIER;
        thread_info[thread_id].locking_id = -1;
        asm volatile("bhl_unlock_end_b:" ::: "memory");
#endif
        break;

    case LOCK_TYPE_FUTEX:
#if defined(BPF) && !defined(HYBRID_EPOCH)
        long ret = 0;
#endif
        if (__sync_fetch_and_sub(&the_lock->futex_lock, 1) != 1)
        {
            the_lock->futex_lock = 0;
#if defined(BPF) && !defined(HYBRID_EPOCH)
            ret =
#endif
                futex_wake((void *)&the_lock->futex_lock, 1);
        }

#if defined(BPF) && !defined(HYBRID_EPOCH) // If BPF is disabled, automatic switching is also disabled
        if (ret == 0)
        {
            unsigned long fa = atomic_load(&the_lock->last_waiter_at);
            unsigned long lsa = atomic_load(&the_lock->last_switched_at);
            unsigned long now = get_nsecs();

            if (now - NO_WAITER_DURATION_BACK_SPIN_NSECS > fa && now - MINIMUM_DURATION_BETWEEN_SWITCHES_NSECS > lsa)
            {
                if (__sync_bool_compare_and_swap(&the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_FUTEX), LOCK_TRANSITION(LOCK_TYPE_FUTEX, LOCK_TYPE_SPIN)))
                {
                    atomic_store(&the_lock->last_switched_at, now);
                }
            }
        }
#endif

        break;
    default:
        fprintf(stderr, "Transition types cannot be unlocked.\n");
        exit(EXIT_FAILURE);
    }
}

void hybridlock_lock(hybridlock_lock_t *the_lock)
{
    hybrid_qnode_ptr qnode = get_me(the_lock);

#ifdef HYBRID_EPOCH
    qnode->should_block = 0;
    if (!lock_type(the_lock, qnode, LOCK_TYPE_SPIN))
    {
#ifdef TRACING
        if (the_lock->tracing_fn)
            the_lock->tracing_fn(getticks(), TRACING_EVENT_SWITCH_BLOCK, NULL, the_lock->tracing_fn_data);
#endif
        lock_type(the_lock, qnode, LOCK_TYPE_FUTEX);
        qnode->should_block = 2; // 2 = Futex should be released on unlock.

        while (the_lock->dummy_qnode->waiting != 0)
            PAUSE;
    }
#else
    lock_state_t state;
    do
    {
        state = the_lock->lock_state;

        if (!lock_type(the_lock, qnode, LOCK_CURR_TYPE(state)))
            continue;

        if (the_lock->lock_state == state)
        {
            if (LOCK_CURR_TYPE(state) != LOCK_LAST_TYPE(state))
            { // Wait for the previous holder to exit its critical section
                while (!isfree_type(the_lock, LOCK_LAST_TYPE(state)))
                    PAUSE;

#ifdef TRACING
                if (the_lock->tracing_fn)
                    the_lock->tracing_fn(getticks(), LOCK_CURR_TYPE(state) == LOCK_TYPE_SPIN ? TRACING_EVENT_SWITCH_SPIN : TRACING_EVENT_SWITCH_BLOCK, NULL, the_lock->tracing_fn_data);
#endif

                DPRINT("[%d] Switched lock #%d to %d\n", gettid(), the_lock->id, LOCK_CURR_TYPE(state));

                the_lock->lock_state = LOCK_STABLE(LOCK_CURR_TYPE(state));
            }
            break;
        }

        unlock_type(the_lock, qnode, LOCK_CURR_TYPE(state));
    } while (1);
#endif
}

void hybridlock_unlock(hybridlock_lock_t *the_lock)
{
    hybrid_qnode_ptr qnode = get_me(the_lock);
#ifdef HYBRID_EPOCH
    if (qnode->should_block != 0)
    {
        if (__sync_fetch_and_sub(the_lock->blocking_nodes, 1) == 1)
        {
            unlock_type(the_lock, the_lock->dummy_qnode, LOCK_TYPE_SPIN);
            __sync_val_compare_and_swap(the_lock->dummy_node_enqueued, 1, 0);
        }

#ifdef DEBUG
        /*if (qnode->should_block == 1)
            DPRINT("Not a bug: Thread should have blocked but did not.\n");*/
#endif
    }

    unlock_type(the_lock, qnode, qnode->should_block == 2 ? LOCK_TYPE_FUTEX : LOCK_TYPE_SPIN);
#else
    unlock_type(the_lock, qnode, LOCK_LAST_TYPE(the_lock->lock_state));
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
    struct hybridlock_bpf *skel;
    int err;

    // Open BPF skeleton
    libbpf_set_print(libbpf_print_fn);
    skel = hybridlock_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        exit(EXIT_FAILURE);
    }

    addresses = &skel->bss->addresses;
#ifdef HYBRID_MCS
    extern char bhl_lock;
    extern char bhl_lock_check_rcx_null;
    extern char bhl_lock_spin;
    extern char bhl_lock_end;

    extern char bhl_unlock_check_zero_flag1;
    extern char bhl_unlock_check_zero_flag2;
    extern char bhl_unlock_end;
    extern char bhl_unlock_end_b;

    addresses->lock = &bhl_lock;
    addresses->lock_check_rcx_null = &bhl_lock_check_rcx_null;
    addresses->lock_spin = &bhl_lock_spin;
    addresses->lock_end = &bhl_lock_end;

    addresses->unlock_check_zero_flag1 = &bhl_unlock_check_zero_flag1;
    addresses->unlock_check_zero_flag2 = &bhl_unlock_check_zero_flag2;
    addresses->unlock_end = &bhl_unlock_end;
    addresses->unlock_end_b = &bhl_unlock_end_b;
#endif

    thread_info = skel->bss->thread_info;
    lock_info = skel->bss->lock_info;

    // Load BPF skeleton
    err = hybridlock_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton (%d)\n", err);
        hybridlock_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }

    // Store map
    nodes_map = skel->maps.nodes_map;

    int fd = bpf_map__fd(skel->maps.array_map);
    qnode_allocation_array = mmap(NULL, MAX_NUMBER_THREADS * sizeof(hybrid_qnode_thread), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (qnode_allocation_array == MAP_FAILED)
    {
        perror("qnode_allocation_array mmap");
        exit(EXIT_FAILURE);
    }
    skel->bss->qnode_allocation_starting_address = qnode_allocation_array;

    // Attach BPF skeleton
    err = hybridlock_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton (%d)\n", err);
        hybridlock_bpf__destroy(skel);
        exit(EXIT_FAILURE);
    }
}
#endif

int hybridlock_init(hybridlock_lock_t *the_lock)
{
    the_lock->id = atomic_fetch_add(&lock_count, 1);
    if (the_lock->id >= MAX_NUMBER_LOCKS)
    {
        fprintf(stderr, "Too many locks. Increase MAX_NUMBER_LOCKS in platform_defs.h.\n");
        exit(EXIT_FAILURE);
    }

    the_lock->blocking_id = 0;

    static volatile uint8_t init_lock = 0;
    if (exactly_once(&init_lock) == 0)
    {
#ifdef BPF
        deploy_bpf_code();
#else
        // Initialize things without BPF
        qnode_allocation_array = malloc(MAX_NUMBER_THREADS * sizeof(hybrid_qnode_thread));
        lock_info = malloc(MAX_NUMBER_LOCKS * sizeof(hybrid_lock_info_t));
        thread_info = malloc(MAX_NUMBER_THREADS * sizeof(hybrid_thread_info_t));
#endif
        init_lock = 2;
    }

#ifdef HYBRID_MCS
    the_lock->queue_lock = &lock_info[the_lock->id].queue_lock;

#ifdef HYBRID_EPOCH
#ifdef BPF
    the_lock->dummy_node_enqueued = &lock_info[the_lock->id].dummy_node_enqueued;
    the_lock->blocking_nodes = &lock_info[the_lock->id].blocking_nodes;

    the_lock->dummy_qnode = &qnode_allocation_array[the_lock->id];
    the_lock->dummy_qnode->waiting = 0;
    the_lock->dummy_qnode->next = NULL;
    the_lock->dummy_qnode->should_block = 0;
#endif
#endif
#endif

    the_lock->futex_lock = 0;

#if defined(BPF) && !defined(HYBRID_EPOCH)
    the_lock->preempted_at = &lock_info[the_lock->id].preempted_at;
    (*the_lock->preempted_at) = ULONG_MAX;
    the_lock->last_waiter_at = ULONG_MAX;
    the_lock->last_switched_at = 0;
#endif

#ifndef HYBRID_EPOCH
    the_lock->lock_state = LOCK_STABLE(LOCK_TYPE_SPIN);
#endif

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

void hybridlock_destroy(hybridlock_lock_t *the_lock)
{
    // Nothing to do
}

#ifdef TRACING
void set_tracing_fn(hybridlock_lock_t *the_lock, void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data), void *tracing_fn_data)
{
    the_lock->tracing_fn_data = tracing_fn_data;
    the_lock->tracing_fn = tracing_fn;
}
#endif

/*
 *  Condition Variables
 */

int hybridlock_cond_init(hybridlock_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int hybridlock_cond_wait(hybridlock_cond_t *cond, hybridlock_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    hybridlock_unlock(the_lock);

    while (target > seq)
    {
#ifndef HYBRID_EPOCH
        if (LOCK_CURR_TYPE(the_lock->lock_state) == LOCK_TYPE_FUTEX)
#endif
            futex_wait(&cond->seq, seq);
#ifndef HYBRID_EPOCH
        else
            PAUSE;
#endif
        seq = cond->seq;
    }
    hybridlock_lock(the_lock);
    return 0;
}

int hybridlock_cond_timedwait(hybridlock_cond_t *cond, hybridlock_lock_t *the_lock, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int hybridlock_cond_signal(hybridlock_cond_t *cond)
{
    cond->seq++;
    futex_wake(&cond->seq, 1);
    return 0;
}

int hybridlock_cond_broadcast(hybridlock_cond_t *cond)
{
    cond->seq = cond->target;
    futex_wake(&cond->seq, INT_MAX);
    return 0;
}

int hybridlock_cond_destroy(hybridlock_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}