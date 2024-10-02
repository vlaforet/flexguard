/*
 * File: clh.h
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

#ifndef _CLH_H_
#define _CLH_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdatomic.h>
#include "atomic_ops.h"
#include "utils.h"

typedef struct clh_qnode_t
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
    volatile struct clh_qnode_t *pred;
#ifdef ADD_PADDING
    uint8_t padding2[CACHE_LINE_SIZE];
#endif
  };
} clh_qnode_t;

typedef volatile clh_qnode_t *clh_qnode_ptr;

typedef struct clh_lock_t
{
  union
  {
    clh_qnode_ptr lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
  volatile clh_qnode_ptr qnodes[MAX_NUMBER_THREADS];
} clh_lock_t;
#define CLH_INITIALIZER {NULL}

/*
 * Declarations
 */

int clh_init(clh_lock_t *the_lock);
void clh_destroy(clh_lock_t *the_lock);
void clh_lock(clh_lock_t *the_lock);
int clh_trylock(clh_lock_t *the_lock);
void clh_unlock(clh_lock_t *the_lock);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T clh_lock_t
#define LOCKIF_INIT clh_init
#define LOCKIF_DESTROY clh_destroy
#define LOCKIF_LOCK clh_lock
#define LOCKIF_TRYLOCK clh_trylock
#define LOCKIF_UNLOCK clh_unlock
#define LOCKIF_INITIALIZER CLH_INITIALIZER

#endif
