/*
 * File: clh.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a CLH lock
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

#include "clh.h"

static volatile int clh_thread_count = 0;
static __thread int clh_thread_id = -1;

#define CHECK_THREAD_ID(the_lock)                                   \
    if (UNLIKELY(clh_thread_id < 0))                                \
    {                                                               \
        clh_thread_id = __sync_fetch_and_add(&clh_thread_count, 1); \
        CHECK_NUMBER_THREADS_FATAL(clh_thread_id);                  \
    }

int clh_trylock(clh_lock_t *the_lock)
{
    return 1; // Fail
}

void clh_lock(clh_lock_t *the_lock)
{
    CHECK_THREAD_ID(the_lock);
    clh_qnode_ptr qnode = the_lock->qnodes[clh_thread_id];

    qnode->done = 0;
    qnode->pred = atomic_exchange(&the_lock->lock, qnode);

    while (qnode->pred->done == 0)
        PAUSE;
}

void clh_unlock(clh_lock_t *the_lock)
{
    CHECK_THREAD_ID(the_lock);
    clh_qnode_ptr qnode = the_lock->qnodes[clh_thread_id];

    volatile clh_qnode_t *pred = qnode->pred;
    qnode->done = 1;
    the_lock->qnodes[clh_thread_id] = pred;
}

int clh_init(clh_lock_t *the_lock)
{
    volatile clh_qnode_t *qnodes = malloc(sizeof(clh_qnode_t) * (MAX_NUMBER_THREADS + 1));
    the_lock->lock = &qnodes[MAX_NUMBER_THREADS];
    the_lock->lock->done = 1;
    the_lock->lock->pred = NULL;

    for (int i = 0; i < MAX_NUMBER_THREADS; i++)
    {
        the_lock->qnodes[i] = &qnodes[i];
        the_lock->qnodes[i]->done = 1;
        the_lock->qnodes[i]->pred = NULL;
    }

    MEM_BARRIER;
    return 0;
}

void clh_destroy(clh_lock_t *the_lock)
{
}