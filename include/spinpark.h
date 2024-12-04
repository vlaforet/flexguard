/*
 * File: spinpark.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a spin-then-park lock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Victor Laforet
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

#ifndef _SPINPARK_H_
#define _SPINPARK_H_

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

typedef volatile int spinpark_lock_data_t;
typedef struct spinpark_lock_t
{
  union
  {
    spinpark_lock_data_t data;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} spinpark_lock_t;
#define SPINPARK_INITIALIZER {0}

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
} spinpark_cond_t;
#define SPINPARK_COND_INITIALIZER \
  {                               \
    {                             \
      0, 0                        \
    }                             \
  }

/*
 * Declarations
 */
int spinpark_init(spinpark_lock_t *the_lock);
void spinpark_destroy(spinpark_lock_t *the_lock);
void spinpark_lock(spinpark_lock_t *the_lock);
int spinpark_trylock(spinpark_lock_t *the_lock);
void spinpark_unlock(spinpark_lock_t *the_lock);

int spinpark_cond_init(spinpark_cond_t *cond);
int spinpark_cond_wait(spinpark_cond_t *cond, spinpark_lock_t *the_lock);
int spinpark_cond_timedwait(spinpark_cond_t *cond, spinpark_lock_t *the_lock, const struct timespec *ts);
int spinpark_cond_signal(spinpark_cond_t *cond);
int spinpark_cond_broadcast(spinpark_cond_t *cond);
int spinpark_cond_destroy(spinpark_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T spinpark_lock_t
#define LOCKIF_INIT spinpark_init
#define LOCKIF_DESTROY spinpark_destroy
#define LOCKIF_LOCK spinpark_lock
#define LOCKIF_TRYLOCK spinpark_trylock
#define LOCKIF_UNLOCK spinpark_unlock
#define LOCKIF_INITIALIZER SPINPARK_INITIALIZER

#define LOCKIF_COND_T spinpark_cond_t
#define LOCKIF_COND_INIT spinpark_cond_init
#define LOCKIF_COND_DESTROY spinpark_cond_destroy
#define LOCKIF_COND_WAIT spinpark_cond_wait
#define LOCKIF_COND_TIMEDWAIT spinpark_cond_timedwait
#define LOCKIF_COND_SIGNAL spinpark_cond_signal
#define LOCKIF_COND_BROADCAST spinpark_cond_broadcast
#define LOCKIF_COND_INITIALIZER SPINPARK_COND_INITIALIZER

#endif
