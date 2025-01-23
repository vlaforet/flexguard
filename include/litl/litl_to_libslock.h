/*
 * File: litl_to_libslock.h
 * Authors: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Allow the use of LiTL locks in libslock
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

#ifndef __LITL_TO_LIBSLOCK__
#define __LITL_TO_LIBSLOCK__

#include <stdint.h>
#include "../utils.h"

typedef struct litl_lock_t
{
  lock_mutex_t *lock;
#ifdef ADD_PADDING
  uint8_t padding[CACHE_LINE_SIZE - 8];
#endif

#if NEED_CONTEXT == 1
  lock_context_t *contexts;
#endif

} litl_lock_t;

extern __thread unsigned int cur_thread_id;

static lock_context_t *get_me(litl_lock_t *the_lock)
{
  static unsigned int last_thread_id = 1;

  if (UNLIKELY(cur_thread_id == 0))
  {
    cur_thread_id = __sync_fetch_and_add(&last_thread_id, 1);
    CHECK_NUMBER_THREADS_FATAL(cur_thread_id);
  }

#if NEED_CONTEXT == 1
  return &the_lock->contexts[cur_thread_id];
#else
  return NULL;
#endif
}

static inline int litl_mutex_init(litl_lock_t *the_lock)
{
  the_lock->lock = lock_mutex_create(NULL);

#if NEED_CONTEXT == 1
  the_lock->contexts = (lock_context_t *)calloc(MAX_NUMBER_THREADS, sizeof(lock_context_t));
#endif

  return the_lock->lock == NULL;
}

static inline void litl_mutex_destroy(litl_lock_t *the_lock)
{
#if NEED_CONTEXT == 1
  free(the_lock->contexts);
#endif

  lock_mutex_destroy(the_lock->lock);
}

static inline void litl_mutex_lock(litl_lock_t *the_lock)
{
  lock_mutex_lock(the_lock->lock, get_me(the_lock));
}

static inline void litl_mutex_unlock(litl_lock_t *the_lock)
{
  lock_mutex_unlock(the_lock->lock, get_me(the_lock));
}

#define LOCKIF_LOCK_T litl_lock_t
#define LOCKIF_INIT litl_mutex_init
#define LOCKIF_DESTROY litl_mutex_destroy
#define LOCKIF_LOCK litl_mutex_lock
#define LOCKIF_UNLOCK litl_mutex_unlock
#define LOCKIF_INITIALIZER MCS_INITIALIZER

static inline int litl_cond_init(lock_cond_t *cond)
{
  return lock_cond_init(cond, NULL);
}

static inline int litl_cond_wait(lock_cond_t *cond, litl_lock_t *the_lock)
{
  return lock_cond_wait(cond, the_lock->lock, get_me(the_lock));
}

static inline int litl_cond_timedwait(lock_cond_t *cond, litl_lock_t *the_lock, const struct timespec *ts)
{
  return lock_cond_timedwait(cond, the_lock->lock, get_me(the_lock), ts);
}

#define LOCKIF_COND_T lock_cond_t
#define LOCKIF_COND_INIT litl_cond_init
#define LOCKIF_COND_DESTROY lock_cond_destroy
#define LOCKIF_COND_WAIT litl_cond_wait
#define LOCKIF_COND_TIMEDWAIT litl_cond_timedwait
#define LOCKIF_COND_SIGNAL lock_cond_signal
#define LOCKIF_COND_BROADCAST lock_cond_broadcast
#define LOCKIF_COND_INITIALIZER MCS_COND_INITIALIZER

#endif