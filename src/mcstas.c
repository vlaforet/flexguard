/*
 * File: mcstas.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of an MCS/TAS lock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Victor Laforet
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

#include "mcstas.h"
#include "extend.h"

static __thread volatile mcstas_qnode local = {.next = NULL, .waiting = 0};

int mcstas_trylock(mcstas_lock_t *the_lock)
{
    return atomic_flag_test_and_set(&(the_lock->lock));
}

void mcstas_lock(mcstas_lock_t *the_lock)
{
#ifdef TIMESLICE_EXTENSION
    if (the_lock->lock == 0)
    {
        extend();
#endif
        if (!atomic_flag_test_and_set(&(the_lock->lock)))
            return;
#ifdef TIMESLICE_EXTENSION
        unextend();
    }
#endif

    local.next = NULL;

    volatile mcstas_qnode *pred = atomic_exchange(&the_lock->tail, &local);
    if (pred != NULL)
    {
        local.waiting = 1; // word on which to spin
        MEM_BARRIER;
        pred->next = &local; // make pred point to me

        while (local.waiting != 0)
            PAUSE;
    }

#ifdef TIMESLICE_EXTENSION
    extend();
#endif

    volatile uint8_t *l = &(the_lock->lock);
    while (atomic_flag_test_and_set(l))
        PAUSE;

    // Unlock MCS
    volatile mcstas_qnode *succ;
    if (!(succ = local.next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (__sync_val_compare_and_swap(&the_lock->tail, &local, NULL) == &local)
            return;
        do
        {
            succ = local.next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
}

void mcstas_unlock(mcstas_lock_t *the_lock)
{
    COMPILER_BARRIER();
    the_lock->lock = 0;

#ifdef TIMESLICE_EXTENSION
    unextend();
#endif
}

int mcstas_init(mcstas_lock_t *the_lock)
{
    the_lock->lock = 0;
    the_lock->tail = NULL;
    MEM_BARRIER;
    return 0;
}

void mcstas_destroy(mcstas_lock_t *the_lock)
{
    // Nothing to do
}

/*
 *  Condition Variables
 */

int mcstas_cond_init(mcstas_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int mcstas_cond_wait(mcstas_cond_t *cond, mcstas_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    mcstas_unlock(the_lock);

    while (target > seq)
    {
        PAUSE;
        seq = cond->seq;
    }
    mcstas_lock(the_lock);
    return 0;
}

int mcstas_cond_timedwait(mcstas_cond_t *cond, mcstas_lock_t *the_lock, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int mcstas_cond_signal(mcstas_cond_t *cond)
{
    cond->seq++;
    return 0;
}

int mcstas_cond_broadcast(mcstas_cond_t *cond)
{
    cond->seq = cond->target;
    return 0;
}

int mcstas_cond_destroy(mcstas_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}
