/*
 * File: spinextend.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a spin lock extending its timeslice
 *      (https://lore.kernel.org/lkml/20231025054219.1acaa3dd@gandalf.local.home/)
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Victor Laforet
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

#ifndef _SPINEXTEND_H_
#define _SPINEXTEND_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include "atomic_ops.h"
#include "utils.h"

#define UNLOCKED 0
#define LOCKED 1

typedef volatile uint32_t spinextend_index_t;
typedef uint8_t spinextend_lock_data_t;

typedef struct spinextend_lock_t
{
  union
  {
    spinextend_lock_data_t lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} spinextend_lock_t;
#define SPINEXTEND_INITIALIZER \
  {                            \
    {                          \
      NULL                     \
    }                          \
  }

struct extend_map
{
  unsigned long flags;
};
#define EXTEND_SCHED_FS "/sys/kernel/extend_sched"

/*
 *  Declarations
 */
int spinextend_init(spinextend_lock_t *the_lock);
void spinextend_destroy(spinextend_lock_t *the_lock);
void spinextend_lock(spinextend_lock_t *the_lock);
int spinextend_trylock(spinextend_lock_t *the_lock);
void spinextend_unlock(spinextend_lock_t *the_lock);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T spinextend_lock_t
#define LOCKIF_INIT spinextend_init
#define LOCKIF_DESTROY spinextend_destroy
#define LOCKIF_LOCK spinextend_lock
#define LOCKIF_TRYLOCK spinextend_trylock
#define LOCKIF_UNLOCK spinextend_unlock
#define LOCKIF_INITIALIZER SPINEXTEND_INITIALIZER

#endif
