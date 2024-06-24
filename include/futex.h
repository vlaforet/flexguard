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
#define FUTEX_INITIALIZER \
  {                       \
    .data = 0             \
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
} futex_cond_t;
#define FUTEX_COND_INITIALIZER \
  {                            \
    .seq = 0, .target = 0      \
  }

/*
 * Declarations
 */
int futex_init(futex_lock_t *the_lock);
void futex_destroy(futex_lock_t *the_lock);
void futex_lock(futex_lock_t *the_lock);
int futex_trylock(futex_lock_t *the_lock);
void futex_unlock(futex_lock_t *the_lock);

int futex_cond_init(futex_cond_t *cond);
int futex_cond_wait(futex_cond_t *cond, futex_lock_t *the_lock);
int futex_cond_timedwait(futex_cond_t *cond, futex_lock_t *the_lock, const struct timespec *ts);
int futex_cond_signal(futex_cond_t *cond);
int futex_cond_broadcast(futex_cond_t *cond);
int futex_cond_destroy(futex_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T futex_lock_t
#define LOCKIF_INIT futex_init
#define LOCKIF_DESTROY futex_destroy
#define LOCKIF_LOCK futex_lock
#define LOCKIF_TRYLOCK futex_trylock
#define LOCKIF_UNLOCK futex_unlock
#define LOCKIF_INITIALIZER FUTEX_INITIALIZER

#define LOCKIF_COND_T futex_cond_t
#define LOCKIF_COND_INIT futex_cond_init
#define LOCKIF_COND_DESTROY futex_cond_destroy
#define LOCKIF_COND_WAIT futex_cond_wait
#define LOCKIF_COND_TIMEDWAIT futex_cond_timedwait
#define LOCKIF_COND_SIGNAL futex_cond_signal
#define LOCKIF_COND_BROADCAST futex_cond_broadcast
#define LOCKIF_COND_INITIALIZER FUTEX_COND_INITIALIZER

#endif
