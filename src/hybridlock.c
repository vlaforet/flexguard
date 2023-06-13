/*
 * File: hybridlock.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a MCS/futex hybrid lock
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

static void futex_wait(void *addr, int val)
{
    syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0); /* Wait if *addr == val. */
}

static void futex_wake(void *addr, int nb_threads)
{
    syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, nb_threads, NULL, NULL, 0);
}

static inline int isfree_type(hybridlock_lock_t *the_lock, lock_type_t lock_type)
{
    switch (lock_type)
    {
    case LOCK_TYPE_MCS:
        if ((*the_lock->mcs_lock) != NULL)
            return 0; // Not free
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
    case LOCK_TYPE_MCS:
        local_params->qnode->next = NULL;
        if (CAS_PTR(the_lock->mcs_lock, NULL, local_params->qnode) != NULL)
            return 1;
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
    case LOCK_TYPE_MCS:
        local_params->qnode->next = NULL;
        mcs_qnode_ptr pred = (mcs_qnode_t *)SWAP_PTR((volatile void *)the_lock->mcs_lock, (void *)local_params->qnode);
        if (pred == NULL) /* lock was free */
            return 1;     // Success

        local_params->qnode->waiting = 1; // word on which to spin
        MEM_BARRIER;
        pred->next = local_params->qnode; // make pred point to me

        while (local_params->qnode->waiting != 0 && LOCK_CURR_TYPE(*the_lock->lock_state) == lock_type)
            PAUSE;

        if (local_params->qnode->waiting != 0 && __sync_val_compare_and_swap(&local_params->qnode->waiting, 1, 0) == 1)
            return 0; // Failed to acquire

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
    case LOCK_TYPE_MCS:;
        mcs_qnode_ptr curr = local_params->qnode, succ;
        do
        {
            succ = curr->next;
            if (!succ) /* I seem to have no succ. */
            {
                /* try to fix global pointer */
                if (CAS_PTR(the_lock->mcs_lock, curr, NULL) == curr)
                    break;
                do
                {
                    succ = curr->next;
                    PAUSE;
                } while (!succ); // wait for successor
            }

            curr = succ;
        } while (__sync_val_compare_and_swap(&succ->waiting, 1, 0) != 1); // Spin over aborted nodes

        break;
    case LOCK_TYPE_FUTEX:
        if (__sync_fetch_and_sub(&the_lock->futex_lock, 1) != 1)
        {
            the_lock->futex_lock = 0;
            futex_wake((void *)&the_lock->futex_lock, 1);
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
}

void hybridlock_unlock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params)
{
    unlock_type(the_lock, local_params, LOCK_LAST_TYPE(*the_lock->lock_state));
}

int is_free_hybridlock(hybridlock_lock_t *the_lock)
{
    // will see
    return 0;
}

/*
   Some methods for easy lock array manipulation
   */

hybridlock_lock_t *init_hybridlock_array_global(uint32_t size)
{
    hybridlock_lock_t *the_locks = (hybridlock_lock_t *)malloc(size * sizeof(hybridlock_lock_t));
    for (uint32_t i = 0; i < size; i++)
    {
        the_locks[i].mcs_lock = (mcs_lock_t *)malloc(sizeof(mcs_lock_t));
        *(the_locks[i].mcs_lock) = 0;
    }

    MEM_BARRIER;
    return the_locks;
}

hybridlock_local_params_t *init_hybridlock_array_local(uint32_t thread_num, uint32_t size)
{
    set_cpu(thread_num);

    hybridlock_local_params_t *local_params = (hybridlock_local_params_t *)malloc(size * sizeof(mcs_qnode_t *));
    MEM_BARRIER;
    return local_params;
}

void end_hybridlock_array_local(hybridlock_local_params_t *local_params)
{
    free(local_params);
}

void end_hybridlock_array_global(hybridlock_lock_t *the_locks, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        free(the_locks[i].mcs_lock);
    free(the_locks);
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

#ifdef BPF
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
    the_lock->mcs_lock = &skel->bss->mcs_lock;

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
#else
    the_lock->mcs_lock = (mcs_lock_t *)malloc(sizeof(mcs_lock_t));
    *(the_lock->mcs_lock) = NULL;

    the_lock->lock_state = malloc(sizeof(lock_state_t));
    (*the_lock->lock_state) = LOCK_STABLE(LOCK_TYPE_MCS);
#endif

    MEM_BARRIER;
    return 0;
}

int init_hybridlock_local(uint32_t thread_num, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock)
{
    set_cpu(thread_num);

    local_params->qnode = (mcs_qnode_t *)malloc(sizeof(mcs_qnode_t));
    local_params->qnode->waiting = 0;

#ifdef BPF
    // Register thread in BPF map
    __u32 tid = gettid();
    int err = bpf_map__update_elem(the_lock->nodes_map, &tid, sizeof(tid), &local_params->qnode, sizeof(mcs_qnode_t *), BPF_ANY);
    if (err)
    {
        fprintf(stderr, "Failed to register thread with BPF: %d\n", err);
        return 1;
    }
#endif

    MEM_BARRIER;
    return 0;
}

void end_hybridlock_local()
{
    // function not needed
}

void end_hybridlock_global()
{
    // function not needed
}
