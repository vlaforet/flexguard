/*
 * File: hybridlock.c
 * Author: Tudor David <tudor.david@epfl.ch>mus
 *         Victor Laforet <victor.laforet@ip-paris.fr>
 *
 * Description:
 *      Simple test-and-set spinlock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David
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

#include <bpf/libbpf.h>
#include <sys/resource.h>
#include "hybridlock.skel.h"

#ifndef HYBRIDLOCK_PTHREAD_MUTEX
static long futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

static void futex_wait(void *addr, int val)
{
    futex(addr, FUTEX_WAIT, val, NULL, NULL, 0); /* Wait if *addr == val. */
}

static void futex_wake(void *addr, int nb_threads)
{
    futex(addr, FUTEX_WAKE, nb_threads, NULL, NULL, 0);
}

static void futex_lock(futex_lock_t *lock)
{
    int state;

    if ((state = __sync_val_compare_and_swap(&lock->state, 0, 1)) != 0)
    {
        if (state != 2)
            state = __sync_lock_test_and_set(&lock->state, 2);
        while (state != 0)
        {
            futex_wait((void *)&lock->state, 2);
            state = __sync_lock_test_and_set(&lock->state, 2);
        }
    }
}

static void futex_unlock(futex_lock_t *lock)
{
    if (__sync_fetch_and_sub(&lock->state, 1) != 1)
    {
        lock->state = 0;
        futex_wake((void *)&lock->state, 1);
    }
}
#endif

int hybridlock_trylock(hybridlock_lock_t *the_lock, hybridlock_local_params *my_qnode)
{
    my_qnode->next = NULL;
#ifndef __tile__
    if (CAS_PTR(the_lock->data.mcs_lock, NULL, my_qnode) != NULL)
        return 1;
#else
    MEM_BARRIER;
    if (CAS_PTR(the_lock->data.mcs_lock, NULL, my_qnode) != NULL)
        return 1;
#endif

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_lock(&the_lock->data.mutex_lock);
#else
    futex_lock(&the_lock->data.futex_lock);
#endif

    return 0;
}

void hybridlock_lock(hybridlock_lock_t *the_lock, hybridlock_local_params *my_qnode)
{
    my_qnode->next = NULL;
    my_qnode->locking = 1;
#ifndef __tile__
    mcs_qnode_ptr pred = (mcs_qnode *)SWAP_PTR((volatile void *)the_lock->data.mcs_lock, (void *)my_qnode);
#else
    MEM_BARRIER;
    mcs_qnode_ptr pred = (mcs_qnode *)SWAP_PTR(the_lock->data.mcs_lock, my_qnode);
#endif
    if (pred != NULL) /* lock was not free */
    {
        my_qnode->waiting = 1; // word on which to spin
        MEM_BARRIER;
        pred->next = my_qnode; // make pred point to me

#if defined(OPTERON_OPTIMIZE)
        PREFETCHW(my_qnode);
#endif /* OPTERON_OPTIMIZE */
        while (my_qnode->waiting != 0 && the_lock->data.spinning)
        {
            PAUSE;
#if defined(OPTERON_OPTIMIZE)
            pause_rep(23);
            PREFETCHW(my_qnode);
#endif /* OPTERON_OPTIMIZE */
        }
    }

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_lock(&the_lock->data.mutex_lock);
#else
    futex_lock(&the_lock->data.futex_lock);
#endif
}

void hybridlock_unlock(hybridlock_lock_t *the_lock, hybridlock_local_params *my_qnode)
{
#ifdef __tile__
    MEM_BARRIER;
#endif

    mcs_qnode_ptr succ;
#if defined(OPTERON_OPTIMIZE)
    PREFETCHW(my_qnode);
#endif                            /* OPTERON_OPTIMIZE */
    if (!(succ = my_qnode->next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (CAS_PTR(the_lock->data.mcs_lock, my_qnode, NULL) == my_qnode)
        {
            my_qnode->locking = 0;

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
            pthread_mutex_unlock(&the_lock->data.mutex_lock);
#else
            futex_unlock(&the_lock->data.futex_lock);
#endif
            return;
        }
        do
        {
            succ = my_qnode->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
    my_qnode->locking = 0;

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_unlock(&the_lock->data.mutex_lock);
#else
    futex_unlock(&the_lock->data.futex_lock);
#endif
}

int is_free_hybridlock(hybridlock_lock_t *the_lock)
{
    if ((*the_lock->data.mcs_lock) == NULL)
        return 1;
    return 0;
}

void set_blocking(hybridlock_lock_t *the_lock, int blocking)
{
    if (blocking)
    {
        the_lock->data.spinning = 0;
        DPRINT("Hybrid Lock: Blocking\n");
    }
    else
    {
        the_lock->data.spinning = 1;
        DPRINT("Hybrid Lock: Spinning\n");
    }
}

/*
   Some methods for easy lock array manipulation
   */

hybridlock_lock_t *init_hybridlock_array_global(uint32_t size)
{
    hybridlock_lock_t *the_locks = (hybridlock_lock_t *)malloc(size * sizeof(hybridlock_lock_t));
    for (uint32_t i = 0; i < size; i++)
    {
        the_locks[i].data.mcs_lock = (mcs_lock *)malloc(sizeof(mcs_lock));
        *(the_locks[i].data.mcs_lock) = 0;

        the_locks[i].data.spinning = 1;
#ifdef HYBRIDLOCK_PTHREAD_MUTEX
        pthread_mutex_init(&the_locks[i].data.mutex_lock, NULL);
#endif
    }

    MEM_BARRIER;
    return the_locks;
}

hybridlock_local_params *init_hybridlock_array_local(uint32_t thread_num, uint32_t size)
{
    set_cpu(thread_num);

    hybridlock_local_params *local_params = (hybridlock_local_params *)malloc(size * sizeof(hybridlock_local_params));
    MEM_BARRIER;
    return local_params;
}

void end_hybridlock_array_local(hybridlock_local_params *local_params)
{
    free(local_params);
}

void end_hybridlock_array_global(hybridlock_lock_t *the_locks, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        free(the_locks[i].data.mcs_lock);
    free(the_locks);
}

int init_hybridlock_global(hybridlock_lock_t *the_lock)
{
    the_lock->data.mcs_lock = (mcs_lock *)malloc(sizeof(mcs_lock));
    *(the_lock->data.mcs_lock) = 0;

    the_lock->data.spinning = 1;

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_init(&the_lock->data.mutex_lock, NULL);
#endif

    MEM_BARRIER;
    return 0;
}

int init_hybridlock_local(uint32_t thread_num, hybridlock_local_params *my_qnode)
{
    set_cpu(thread_num);
    my_qnode->locking = 0;
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
