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

int clh_trylock(clh_lock_t *the_locks, clh_local_params_t *local_params)
{
    return 1; // Fail
}

void clh_lock(clh_lock_t *the_lock, clh_local_params_t *local_params)
{
    local_params->qnode->done = 0;
    local_params->qnode->pred = SWAP_PTR(the_lock->lock, local_params->qnode);

    while (local_params->qnode->pred->done == 0)
        PAUSE;
}

void clh_unlock(clh_lock_t *the_locks, clh_local_params_t *local_params)
{
    volatile clh_qnode_t *pred = local_params->qnode->pred;
    local_params->qnode->done = 1;
    local_params->qnode = pred;
}

int is_free_clh(clh_lock_t *the_lock)
{
    return 0; // Not free
}

int init_clh_global(clh_lock_t *the_lock)
{
    the_lock->lock = (clh_qnode_ptr *)malloc(sizeof(clh_qnode_ptr));
    (*the_lock->lock) = (clh_qnode_t *)malloc(sizeof(clh_qnode_t));
    (*the_lock->lock)->done = 1;
    (*the_lock->lock)->pred = NULL;

    MEM_BARRIER;
    return 0;
}

int init_clh_local(uint32_t thread_num, clh_local_params_t *local_params, clh_lock_t *the_lock)
{
    set_cpu(thread_num);

    local_params->qnode = (clh_qnode_t *)malloc(sizeof(clh_qnode_t));
    local_params->qnode->done = 1;
    local_params->qnode->pred = NULL;

    MEM_BARRIER;
    return 0;
}
