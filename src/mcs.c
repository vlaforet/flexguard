/*
 * File: mcs.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description:
 *      MCS lock implementation
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

#include "mcs.h"

int mcs_trylock(mcs_global_params *global, mcs_local_params *local)
{
    local->next = NULL;
    if (__sync_val_compare_and_swap(&global->tail, NULL, local) == NULL)
        return 0;
    return 1;
}

void mcs_lock(mcs_global_params *global, mcs_local_params *local)
{
    local->next = NULL;
    volatile mcs_qnode *pred = (mcs_qnode *)SWAP_PTR((volatile void *)&global->tail, (void *)local);

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

void mcs_unlock(mcs_global_params *global, mcs_local_params *local)
{
    volatile mcs_qnode *succ;
    if (!(succ = local->next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (__sync_val_compare_and_swap(&global->tail, local, NULL) == local)
            return;
        do
        {
            succ = local->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
}

int init_mcs_global(mcs_global_params *global)
{
    global->tail = 0;
    MEM_BARRIER;
    return 0;
}

int init_mcs_local(mcs_global_params *global, mcs_local_params *local)
{
    local->waiting = 0;
    local->next = NULL;

    MEM_BARRIER;
    return 0;
}

void end_mcs_local(mcs_local_params *local)
{
    // Nothing to do
}

void end_mcs_global(mcs_global_params *global)
{
    // Nothing to do
}

/*
 *  Condition Variables
 */

int mcs_condvar_init(mcs_condvar_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int mcs_condvar_wait(mcs_condvar_t *cond, mcs_global_params *global, mcs_local_params *local)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    mcs_unlock(global, local);

    while (target > seq)
    {
        PAUSE;
        seq = cond->seq;
    }
    mcs_lock(global, local);
    return 0;
}

int mcs_condvar_timedwait(mcs_condvar_t *cond, mcs_global_params *global, mcs_local_params *local, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int mcs_condvar_signal(mcs_condvar_t *cond)
{
    cond->seq++;
    return 0;
}

int mcs_condvar_broadcast(mcs_condvar_t *cond)
{
    cond->seq = cond->target;
    return 0;
}

int mcs_condvar_destroy(mcs_condvar_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}
