/*
 * File: atomicclh.h
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

#ifndef _ATOMICCLH_H_
#define _ATOMICCLH_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "atomic_ops.h"
#include "utils.h"

typedef struct atomicclh_qnode_t
{
  union
  {
    volatile uint8_t done;
#ifdef ADD_PADDING
    uint8_t padding1[CACHE_LINE_SIZE];
#endif
  };
  union
  {
    volatile struct atomicclh_qnode_t *pred;
#ifdef ADD_PADDING
    uint8_t padding2[CACHE_LINE_SIZE];
#endif
  };
} atomicclh_qnode_t;

typedef volatile atomicclh_qnode_t *atomicclh_qnode_ptr;

typedef struct atomicclh_lock_t
{
  union
  {
    atomicclh_qnode_ptr *lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} atomicclh_lock_t;

typedef struct atomicclh_local_params_t
{
  union
  {
    volatile atomicclh_qnode_t *qnode;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} atomicclh_local_params_t;

/*
 * Lock manipulation methods
 */
void atomicclh_lock(atomicclh_lock_t *the_lock, atomicclh_local_params_t *local_params);
int atomicclh_trylock(atomicclh_lock_t *the_locks, atomicclh_local_params_t *local_params);
void atomicclh_unlock(atomicclh_lock_t *the_locks, atomicclh_local_params_t *local_params);
int is_free_atomicclh(atomicclh_lock_t *the_lock);

/*
 * Methods for single lock manipulation
 */
int init_atomicclh_global(atomicclh_lock_t *the_lock);
int init_atomicclh_local(uint32_t thread_num, atomicclh_local_params_t *local_params, atomicclh_lock_t *the_lock);

#endif
