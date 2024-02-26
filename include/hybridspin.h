/*
 * File: hybridspin.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a compare-and-swap/futex hybrid lock
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

#ifndef _HYBRIDSPIN_H_
#define _HYBRIDSPIN_H_

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
#include <sys/resource.h>
#include <linux/futex.h>

#include <fcntl.h>
#include <pthread.h>
#include "atomic_ops.h"
#include "utils.h"

#ifdef BPF
#include <bpf/libbpf.h>
#endif

#ifndef hybridspin_PTHREAD_MUTEX
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

typedef uint32_t hybridspin_lock_type_t;

typedef struct hybridspin_data_t
{
  hybridspin_lock_type_t lock;
#ifdef hybridspin_PTHREAD_MUTEX
  pthread_mutex_t mutex_lock;
#else
  futex_lock_t futex_lock;
#endif
  int spinning;
} hybridspin_data_t;

typedef struct hybridspin_lock_t
{
  union
  {
    hybridspin_data_t data;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#else
    uint8_t padding;
#endif
  };
} hybridspin_lock_t;

/*
 *  Lock manipulation methods
 */
void hybridspin_lock(hybridspin_lock_t *the_lock);
int hybridspin_trylock(hybridspin_lock_t *the_locks);
void hybridspin_unlock(hybridspin_lock_t *the_locks);
int is_free_hybridspin(hybridspin_lock_t *the_lock);
void set_blocking(hybridspin_lock_t *the_lock, int blocking);

/*
 *  Methods for single lock manipulation
 */
int init_hybridspin_global(hybridspin_lock_t *the_lock);
void end_hybridspin_local();
void end_hybridspin_global();

#endif
