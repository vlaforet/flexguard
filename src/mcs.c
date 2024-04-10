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

int mcs_trylock(mcs_lock *L, mcs_qnode_ptr I)
{
    I->next = NULL;
    if (CAS_PTR(L, NULL, I) == NULL)
        return 0;
    return 1;
}

void mcs_acquire(mcs_lock *L, mcs_qnode_ptr I)
{
    I->next = NULL;
    mcs_qnode_ptr pred = (mcs_qnode *)SWAP_PTR((volatile void *)L, (void *)I);

    if (pred == NULL) /* lock was free */
        return;
    I->waiting = 1; // word on which to spin
    MEM_BARRIER;
    pred->next = I; // make pred point to me

    while (I->waiting != 0)
    {
        PAUSE;
    }
}

void mcs_release(mcs_lock *L, mcs_qnode_ptr I)
{
    mcs_qnode_ptr succ;
    if (!(succ = I->next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (CAS_PTR(L, I, NULL) == I)
            return;
        do
        {
            succ = I->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
}

int is_free_mcs(mcs_lock *L)
{
    if ((*L) == NULL)
        return 1;
    return 0;
}

int init_mcs_global(mcs_global_params *the_lock)
{
    the_lock->the_lock = (mcs_lock *)malloc(sizeof(mcs_lock));
    *(the_lock->the_lock) = 0;
    MEM_BARRIER;
    return 0;
}

int init_mcs_local(mcs_qnode **the_qnode)
{
    (*the_qnode) = (mcs_qnode *)malloc(sizeof(mcs_qnode));

    MEM_BARRIER;
    return 0;
}

void end_mcs_local(mcs_qnode *the_qnodes)
{
    free(the_qnodes);
}

void end_mcs_global(mcs_global_params the_locks)
{
    free(the_locks.the_lock);
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

int mcs_condvar_wait(mcs_condvar_t *cond, mcs_qnode_ptr I, mcs_lock *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    mcs_release(the_lock, I);

    while (target > seq)
    {
        PAUSE;
        seq = cond->seq;
    }
    mcs_acquire(the_lock, I);
    return 0;
}

int mcs_condvar_timedwait(mcs_condvar_t *cond, mcs_qnode_ptr I, mcs_lock *the_lock, const struct timespec *ts)
{
    perror("Timedwait not supported yet.");
    return 1;
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
