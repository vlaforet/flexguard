/*
 * File: mcsextend.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of an mcs lock extending its timeslice
 *      (https://lore.kernel.org/lkml/20231025054219.1acaa3dd@gandalf.local.home/)
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Victor Laforet
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

#include "mcsextend.h"

static volatile mcsextend_qnode *get_me(mcsextend_lock_t *the_lock)
{
    static __thread int thread_id = -1;
    static volatile int thread_count = 0;

    if (UNLIKELY(thread_id < 0))
    {
        thread_id = __sync_fetch_and_add(&thread_count, 1);
        CHECK_NUMBER_THREADS_FATAL(thread_id);
    }

    return &the_lock->qnodes[thread_id];
}

int mcsextend_trylock(mcsextend_lock_t *the_lock)
{
    volatile mcsextend_qnode *local = get_me(the_lock);

    local->next = NULL;

    extend();
    if (__sync_val_compare_and_swap(&the_lock->tail, NULL, local) == NULL)
        return 0; // Success

    unextend();   // Unextend on failure to acquire
    return EBUSY; // Locked
}

void mcsextend_lock(mcsextend_lock_t *the_lock)
{
    volatile mcsextend_qnode *local = get_me(the_lock);

    local->next = NULL;

    extend();
    volatile mcsextend_qnode *pred = (mcsextend_qnode *)SWAP_PTR((volatile void *)&the_lock->tail, (void *)local);

    if (pred == NULL) /* lock was free */
        return;

    local->waiting = 1; // word on which to spin
    MEM_BARRIER;
    pred->next = local; // make pred point to me

    while (local->waiting != 0)
    {
        PAUSE;
    }
}

void mcsextend_unlock(mcsextend_lock_t *the_lock)
{
    volatile mcsextend_qnode *local = get_me(the_lock);

    volatile mcsextend_qnode *succ;
    if (!(succ = local->next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (__sync_val_compare_and_swap(&the_lock->tail, local, NULL) == local)
            return;
        do
        {
            succ = local->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
    unextend();
}

int mcsextend_init(mcsextend_lock_t *the_lock)
{
    the_lock->tail = 0;
    the_lock->qnodes = (mcsextend_qnode *)calloc(MAX_NUMBER_THREADS, sizeof(mcsextend_qnode));
    MEM_BARRIER;
    return 0;
}

void mcsextend_destroy(mcsextend_lock_t *the_lock)
{
    free((void *)the_lock->qnodes);
}

/*
 *  Condition Variables
 */

int mcsextend_cond_init(mcsextend_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int mcsextend_cond_wait(mcsextend_cond_t *cond, mcsextend_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    mcsextend_unlock(the_lock);

    while (target > seq)
    {
        PAUSE;
        seq = cond->seq;
    }
    mcsextend_lock(the_lock);
    return 0;
}

int mcsextend_cond_timedwait(mcsextend_cond_t *cond, mcsextend_lock_t *the_lock, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int mcsextend_cond_signal(mcsextend_cond_t *cond)
{
    cond->seq++;
    return 0;
}

int mcsextend_cond_broadcast(mcsextend_cond_t *cond)
{
    cond->seq = cond->target;
    return 0;
}

int mcsextend_cond_destroy(mcsextend_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}
