/*
 * File: futex.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a futex lock
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

#ifndef _FUTEX_H_
#define _FUTEX_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <numa.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include "atomic_ops.h"
#include "utils.h"

typedef volatile int futex_lock_data_t;
typedef struct futex_lock_t
{
  union
  {
    futex_lock_data_t data;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} futex_lock_t;
#define FUTEX_GLOBAL_INITIALIZER \
  {                              \
    .data = 0                    \
  }

typedef union
{
  struct
  {
    uint32_t seq;
    uint32_t target;
  };
#ifdef ADD_PADDING
  uint8_t padding[CACHE_LINE_SIZE];
#endif
} futex_condvar_t;
#define FUTEX_COND_INITIALIZER \
  {                            \
    .seq = 0, .target = 0      \
  }

/*
 * Lock manipulation methods
 */
void futex_lock(futex_lock_t *the_lock);
int futex_trylock(futex_lock_t *the_lock);
void futex_unlock(futex_lock_t *the_lock);

/*
 * Methods for single lock manipulation
 */
int init_futex_global(futex_lock_t *the_lock);
void end_futex_global(futex_lock_t *the_lock);

/*
 *  Condition Variables
 */
int futex_condvar_init(futex_condvar_t *cond);
int futex_condvar_wait(futex_condvar_t *cond, futex_lock_t *the_lock);
int futex_condvar_timedwait(futex_condvar_t *cond, futex_lock_t *the_lock, const struct timespec *ts);
int futex_condvar_signal(futex_condvar_t *cond);
int futex_condvar_broadcast(futex_condvar_t *cond);
int futex_condvar_destroy(futex_condvar_t *cond);

#define LOCAL_NEEDED 0

#define GLOBAL_DATA_T futex_lock_t
#define CONDVAR_DATA_T futex_condvar_t

#define INIT_GLOBAL_DATA init_futex_global
#define DESTROY_GLOBAL_DATA end_futex_global

#define ACQUIRE_LOCK futex_lock
#define RELEASE_LOCK futex_unlock
#define ACQUIRE_TRYLOCK futex_trylock

#define COND_INIT futex_condvar_init
#define COND_WAIT futex_condvar_wait
#define COND_TIMEDWAIT futex_condvar_timedwait
#define COND_SIGNAL futex_condvar_signal
#define COND_BROADCAST futex_condvar_broadcast
#define COND_DESTROY futex_condvar_destroy

#define LOCK_GLOBAL_INITIALIZER FUTEX_GLOBAL_INITIALIZER
#define COND_INITIALIZER FUTEX_COND_INITIALIZER

#endif
