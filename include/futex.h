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
#else
    uint8_t padding;
#endif
  };
} futex_lock_t;

/*
 * Lock manipulation methods
 */
void futex_lock(futex_lock_t *the_lock);
int futex_trylock(futex_lock_t *the_locks);
void futex_unlock(futex_lock_t *the_locks);
int is_free_futex(futex_lock_t *the_lock);

/*
 * Some methods for easy lock array manipluation
 */
futex_lock_t *init_futex_array_global(uint32_t num_locks);
void init_futex_array_local(uint32_t thread_num);
void end_futex_array_global();

/*
 * Methods for single lock manipulation
 */
int init_futex_global(futex_lock_t *the_lock);
int init_futex_local(uint32_t thread_nums);

#endif
