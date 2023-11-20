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
#endif

static unsigned long get_nsecs()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

static void futex_wait(void *addr, int val)
{
    syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0); /* Wait if *addr == val. */
}

static long futex_wake(void *addr, int nb_threads)
{
    return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, nb_threads, NULL, NULL, 0);
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
        printf("Transition types cannot be free.\n");
        exit(1);
    }
    return 1; // Free
}

static inline int trylock_type(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
#ifdef HYBRID_TICKET
        return 1; // Fail, does not support trylock for the moment.
#elif defined(HYBRID_CLH)
        return 1; // Fail, does not support trylock for the moment.
#elif defined(HYBRID_MCS)
        return 1; // Fail, does not support trylock for the moment.
#endif
        break;

    case LOCK_TYPE_FUTEX:
        if (__sync_val_compare_and_swap(&the_lock->futex_lock, 0, 1) != 0)
            return 1;
        break;
    default:
        printf("Transition types cannot be locked.\n");
        exit(1);
    }
    return 0; // Success
}

static inline int lock_type(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
#ifdef HYBRID_TICKET
        local_params->qnode->ticket = __sync_add_and_fetch(&the_lock->ticket_lock.next, 1);

        uint32_t curr, distance;
        while (1)
        {
            curr = the_lock->ticket_lock.calling;
            if (curr == local_params->qnode->ticket)
                break; // Success

            distance = curr > local_params->qnode->ticket ? curr - local_params->qnode->ticket : local_params->qnode->ticket - curr;
            if (distance <= 1)
                PAUSE;
            else
                nop_rep(distance * 512);
        }

#elif defined(HYBRID_CLH)
        DASSERT(local_params->qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);

        local_params->qnode->done = 0;
        local_params->qnode->pred = (hybrid_qnode_t *)SWAP_PTR(the_lock->queue_lock, (void *)local_params->qnode);

        while (local_params->qnode->pred->done == 0 && LOCK_CURR_TYPE(*the_lock->lock_state) == lock_type)
        {
            PAUSE;
            if (*the_lock->preempted_at + 500000 < get_nsecs())
            {
                *the_lock->preempted_at = INT64_MAX;
                __sync_val_compare_and_swap(the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_SPIN), LOCK_TRANSITION(LOCK_TYPE_SPIN, LOCK_TYPE_FUTEX));
            }
        }

        // Cannot abort properly. This only targets cases where every waiter aborts.
        if (local_params->qnode->pred->done == 0)
        {
            volatile hybrid_qnode_t *pred = local_params->qnode->pred;
            local_params->qnode->done = 1;
            local_params->qnode = pred;
            return 0; // Aborted
        }

#elif defined(HYBRID_MCS)
        DASSERT(local_params->qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
        DASSERT(*the_lock->queue_lock != local_params->qnode);

        local_params->qnode->next = NULL;
        hybrid_qnode_ptr pred = (hybrid_qnode_t *)atomic_exchange(the_lock->queue_lock, local_params->qnode);
        if (pred != NULL) /* lock was not free */
        {
            local_params->qnode->waiting = 1; // word on which to spin
            MEM_BARRIER;
            pred->next = local_params->qnode; // make pred point to me

            while (local_params->qnode->waiting != 0 && LOCK_CURR_TYPE(*the_lock->lock_state) == lock_type)
                PAUSE;

            if (local_params->qnode->waiting != 0 && __sync_val_compare_and_swap(&local_params->qnode->waiting, 1, 0) == 1)
                return 0; // Aborted
        }
#endif

        local_params->qnode->in_cs = 1;
        return 1; // Success

    case LOCK_TYPE_FUTEX:;
        int state;

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
        printf("Transition types cannot be locked.\n");
        exit(1);
    }
    return 0;
}

static inline void unlock_type(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_SPIN:
        local_params->qnode->in_cs = 0;

#ifdef HYBRID_TICKET
        the_lock->ticket_lock.calling++;
#elif defined(HYBRID_CLH)
        DASSERT(local_params->qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
        DASSERT(local_params->qnode->pred != NULL);

        volatile hybrid_qnode_t *pred = local_params->qnode->pred;
        local_params->qnode->done = 1;
        local_params->qnode = pred;
#elif defined(HYBRID_MCS)
        DASSERT(local_params->qnode != NULL);
        DASSERT(the_lock->queue_lock != NULL);
        hybrid_qnode_ptr curr = local_params->qnode, succ;
        do
        {
            succ = curr->next;
            if (!succ) /* I seem to have no succ. */
            {
                /* try to fix global pointer */
                if (CAS_PTR(the_lock->queue_lock, curr, NULL) == curr)
                    break;
                do
                {
                    succ = curr->next;
                    PAUSE;
                } while (!succ); // wait for successor
            }

            curr = succ;
        } while (__sync_val_compare_and_swap(&succ->waiting, 1, 0) != 1); // Spin over aborted nodes
#endif
        break;

    case LOCK_TYPE_FUTEX:
        static int debouncer = 0;
        long ret = 0;
        if (__sync_fetch_and_sub(&the_lock->futex_lock, 1) != 1)
        {
            the_lock->futex_lock = 0;
            ret = futex_wake((void *)&the_lock->futex_lock, 1);
        }

        if (debouncer++ >= 40 && ret == 0)
        {
            debouncer = 0;
            __sync_val_compare_and_swap(the_lock->lock_state, LOCK_STABLE(LOCK_TYPE_FUTEX), LOCK_TRANSITION(LOCK_TYPE_FUTEX, LOCK_TYPE_SPIN));
        }
        break;
    default:
        printf("Transition types cannot be unlocked.\n");
        exit(1);
    }
}

int hybridlock_trylock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params)
{
    // will see
    return 1;
}

void hybridlock_lock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params)
{
    DASSERT(the_lock->lock_state != NULL);
    lock_state_t state;
    do
    {
        state = *the_lock->lock_state;

        if (!lock_type(the_lock, local_params, LOCK_CURR_TYPE(state)))
            continue;

        if ((*the_lock->lock_state) == state)
        {
            if (LOCK_CURR_TYPE(state) != LOCK_LAST_TYPE(state))
            { // Wait for the previous holder to exit its critical section
                while (!isfree_type(the_lock, LOCK_LAST_TYPE(state)))
                    PAUSE;

                DPRINT("[%d] Switched lock to %d\n", gettid(), LOCK_CURR_TYPE(state));

                (*the_lock->lock_state) = LOCK_STABLE(LOCK_CURR_TYPE(state));
            }

            break;
        }

        unlock_type(the_lock, local_params, LOCK_CURR_TYPE(state));
    } while (1);

    // printf("func: %lld, label: %lld\n", (void *)hybridlock_lock, &&lock_held);
lock_held:
}

void hybridlock_unlock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params)
{
    DASSERT(the_lock->lock_state != NULL);
    local_params->qnode->in_cs = 0;
    unlock_type(the_lock, local_params, LOCK_LAST_TYPE(*the_lock->lock_state));
}

int is_free_hybridlock(hybridlock_lock_t *the_lock)
{
    // will see
    return 0;
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

static int get_max_pid()
{
    int max_pid;
    FILE *f;

    f = fopen("/proc/sys/kernel/pid_max", "r");
    if (!f)
        return -1;
    if (fscanf(f, "%d\n", &max_pid) != 1)
        max_pid = -1;
    fclose(f);
    return max_pid;
}
#endif

int init_hybridlock_global(hybridlock_lock_t *the_lock)
{
    the_lock->futex_lock = 0;

#ifdef BPF // { BPF
    struct hybridlock_bpf *skel;
    int err;

    // Open BPF skeleton
    libbpf_set_print(libbpf_print_fn);
    skel = hybridlock_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    // Set max size of map to max pid
    int max_pid = get_max_pid();
    if (max_pid < 0)
    {
        fprintf(stderr, "Failed to get max_pid\n");
        return 1;
    }
    bpf_map__set_max_entries(skel->maps.nodes_map, max_pid);

    // Set pointer to lock state
    the_lock->lock_state = &skel->bss->lock_state;
    the_lock->preempted_at = &skel->bss->preempted_at;

#ifdef HYBRID_TICKET
    skel->bss->ticket_calling = &the_lock->ticket_lock.next;
#endif

    // Load BPF skeleton
    err = hybridlock_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }

    // Store map
    the_lock->nodes_map = skel->maps.nodes_map;

    // Attach BPF skeleton
    err = hybridlock_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }
#else  // BPF } { NOBPF
    the_lock->lock_state = malloc(sizeof(lock_state_t));
    the_lock->preempted_at = malloc(sizeof(uint64_t));
#endif // NOBPF }

    (*the_lock->lock_state) = LOCK_STABLE(LOCK_TYPE_SPIN);
    (*the_lock->preempted_at) = INT64_MAX;

#if defined(HYBRID_CLH) || defined(HYBRID_MCS)
    the_lock->queue_lock = (hybrid_qnode_ptr *)malloc(sizeof(hybrid_qnode_ptr));
#endif

#ifdef HYBRID_TICKET
    the_lock->ticket_lock.calling = 1;
    the_lock->ticket_lock.next = 0;
#elif defined(HYBRID_CLH)
    *(the_lock->queue_lock) = (hybrid_qnode_t *)malloc(sizeof(hybrid_qnode_t)); // CLH keeps an empty node
    (*the_lock->queue_lock)->done = 1;
    (*the_lock->queue_lock)->pred = NULL;
#elif defined(HYBRID_MCS)
    *(the_lock->queue_lock) = NULL;
#endif

    MEM_BARRIER;
    return 0;
}

int init_hybridlock_local(uint32_t thread_num, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock)
{
    set_cpu(thread_num);

    local_params->qnode = (hybrid_qnode_t *)malloc(sizeof(hybrid_qnode_t));
    local_params->qnode->in_cs = 0;

#ifdef HYBRID_TICKET
    local_params->qnode->ticket = 0;
#elif defined(HYBRID_CLH)
    local_params->qnode->done = 1;
    local_params->qnode->pred = NULL;
#elif defined(HYBRID_MCS)
    local_params->qnode->waiting = 0;
    local_params->qnode->next = NULL;
#endif

#ifdef BPF
    // Register thread in BPF map
    __u32 tid = gettid();
    int err = bpf_map__update_elem(the_lock->nodes_map, &tid, sizeof(tid), &local_params->qnode, sizeof(hybrid_qnode_t *), BPF_ANY);
    if (err)
    {
        fprintf(stderr, "Failed to register thread with BPF: %d\n", err);
        return 1;
    }
#endif

    MEM_BARRIER;
    return 0;
}

/*
 *  Condition Variables
 */

int hybridlock_condvar_init(hybridlock_condvar_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int hybridlock_condvar_wait(hybridlock_condvar_t *cond, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    hybridlock_unlock(the_lock, local_params);

    while (target > seq)
    {
        if (LOCK_CURR_TYPE(*the_lock->lock_state) == LOCK_TYPE_FUTEX)
            futex_wait(&cond->seq, seq);
        else
            PAUSE;
        seq = cond->seq;
    }
    hybridlock_lock(the_lock, local_params);
    return 0;
}

int hybridlock_condvar_timedwait(hybridlock_condvar_t *cond, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock, const struct timespec *ts)
{
    perror("Timedwait not supported yet.");
    return 1;
}

int hybridlock_condvar_signal(hybridlock_condvar_t *cond)
{
    cond->seq++;
    futex_wake(&cond->seq, 1);
    return 0;
}

int hybridlock_condvar_broadcast(hybridlock_condvar_t *cond)
{
    cond->seq = cond->target;
    futex_wake(&cond->seq, INT_MAX);
    return 0;
}

int hybridlock_condvar_destroy(hybridlock_condvar_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}