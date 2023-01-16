/*
 * File: hybridlock.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *         Victor Laforet <victor.laforet@ip-paris.fr>
 *
 * Description:
 *      Implementation of a simple test-and-set spinlock
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

#ifndef _HYBRIDLOCK_H_
#define _HYBRIDLOCK_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include <fcntl.h>
#ifndef __sparc__
#include <numa.h>
#endif
#include <pthread.h>
#include "atomic_ops.h"
#include "utils.h"

#ifndef HYBRIDLOCK_PTHREAD_MUTEX
typedef struct
{
  union
  {
    volatile int state;

#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#else
    uint8_t padding;
#endif
  };
} futex_lock_t;

#endif

typedef uint32_t hybridlock_lock_type_t;

typedef struct hybridlock_data_t
{
  hybridlock_lock_type_t lock;
#ifdef HYBRIDLOCK_PTHREAD_MUTEX
  pthread_mutex_t mutex_lock;
#else
  futex_lock_t futex_lock;
#endif
  int spinning;
} hybridlock_data_t;

typedef struct hybridlock_lock_t
{
  union
  {
    hybridlock_data_t data;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#else
    uint8_t padding;
#endif
  };
} hybridlock_lock_t;

/*
 *  Lock manipulation methods
 */

void hybridlock_lock(hybridlock_lock_t *the_lock, uint32_t *limits);

int hybridlock_trylock(hybridlock_lock_t *the_locks, uint32_t *limits);

void hybridlock_unlock(hybridlock_lock_t *the_locks);

int is_free_hybridlock(hybridlock_lock_t *the_lock);

void set_blocking(hybridlock_lock_t *the_lock, int blocking);

/*
 *  Some methods for easy lock array manipluation
 */

hybridlock_lock_t *init_hybridlock_array_global(uint32_t num_locks);

uint32_t *init_hybridlock_array_local(uint32_t thread_num, uint32_t size);

void end_hybridlock_array_local(uint32_t *limits);

void end_hybridlock_array_global(hybridlock_lock_t *the_locks);

/*
 *  Methods for single lock manipulation
 */

int init_hybridlock_global(hybridlock_lock_t *the_lock);

int init_hybridlock_local(uint32_t thread_num, uint32_t *limit);

void end_hybridlock_local();

void end_hybridlock_global();

#endif
