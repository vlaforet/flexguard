/*
 * File: atomicclh.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of an atomic CLH lock
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

#include "atomicclh.h"

int atomicclh_trylock(atomicclh_lock_t *the_locks, atomicclh_local_params_t *local_params)
{
    return 1; // Fail
}

void atomicclh_lock(atomicclh_lock_t *the_lock, atomicclh_local_params_t *local_params)
{
    do
    {
        local_params->qnode->pred = *the_lock->lock;
        local_params->qnode->done = 0;
    } while (!__sync_bool_compare_and_swap(the_lock->lock, local_params->qnode->pred, local_params->qnode));

    while (local_params->qnode->pred->done == 0)
        PAUSE;
}

void atomicclh_unlock(atomicclh_lock_t *the_locks, atomicclh_local_params_t *local_params)
{
    volatile atomicclh_qnode_t *pred = local_params->qnode->pred;
    local_params->qnode->done = 1;
    local_params->qnode = pred;
}

int is_free_atomicclh(atomicclh_lock_t *the_lock)
{
    return 0; // Not free
}

int init_atomicclh_global(atomicclh_lock_t *the_lock)
{
    the_lock->lock = (atomicclh_qnode_ptr *)malloc(sizeof(atomicclh_qnode_ptr));
    (*the_lock->lock) = (atomicclh_qnode_t *)malloc(sizeof(atomicclh_qnode_t));
    (*the_lock->lock)->done = 1;
    (*the_lock->lock)->pred = NULL;

    MEM_BARRIER;
    return 0;
}

int init_atomicclh_local(uint32_t thread_num, atomicclh_local_params_t *local_params, atomicclh_lock_t *the_lock)
{
    set_cpu(thread_num);

    local_params->qnode = (atomicclh_qnode_t *)malloc(sizeof(atomicclh_qnode_t));
    local_params->qnode->done = 1;
    local_params->qnode->pred = NULL;

    MEM_BARRIER;
    return 0;
}
