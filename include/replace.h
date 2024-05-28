/*
 * File: replace.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Header to include to replace pthread calls to libslock.
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

#ifndef _REPLACE_H_
#define _REPLACE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "lock_if.h"

  typedef struct replace_pthread_mutex_t
  {
    lock_global_data lock;
    lock_local_data local[MAX_NUMBER_THREADS];
  } replace_pthread_mutex_t;

  static __thread int replace_thread_id = -1;
  static volatile int replace_thread_count = 0;
  static lock_local_data *get_me(replace_pthread_mutex_t *r)
  {
    if (UNLIKELY(replace_thread_id < 0))
    {
      replace_thread_id = __sync_fetch_and_add(&replace_thread_count, 1);
      CHECK_NUMBER_THREADS_FATAL(replace_thread_id);
    }

    if (UNLIKELY(r->local[replace_thread_id] == NULL) && init_lock_local(&r->lock, &r->local[replace_thread_id]) != 0)
    {
      fprintf(stderr, "init_lock_local failed.");
      exit(EXIT_FAILURE);
    }

    return &r->local[replace_thread_id];
  }

#define pthread_mutex_t replace_pthread_mutex_t
#define pthread_mutex_init replace_pthread_mutex_init
#define pthread_mutex_destroy replace_pthread_mutex_destroy
#define pthread_mutex_lock replace_pthread_mutex_lock
#define pthread_mutex_timedlock replace_pthread_mutex_timedlock
#define pthread_mutex_unlock replace_pthread_mutex_unlock
#define pthread_mutex_trylock replace_pthread_mutex_trylock

#define pthread_cond_init replace_pthread_cond_init
#define pthread_cond_destroy replace_pthread_cond_destroy
#define pthread_cond_signal replace_pthread_cond_signal
#define pthread_cond_broadcast replace_pthread_cond_broadcast
#define pthread_cond_wait replace_pthread_cond_wait
#define pthread_cond_timedwait replace_pthread_cond_timedwait
#define pthread_cond_t lock_condvar_t

#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER LOCK_GLOBAL_INITIALIZER

#undef PTHREAD_COND_INITIALIZER
#define PTHREAD_COND_INITIALIZER COND_INITIALIZER

  /*
   * Mutex
   */

  static inline int replace_pthread_mutex_init(pthread_mutex_t *mutex, void *attr)
  {
    return init_lock_global(&mutex->lock);
  }

  static inline int replace_pthread_mutex_destroy(pthread_mutex_t *mutex)
  {
    free_lock_global(mutex->lock);
    return 0;
  }

  static inline int replace_pthread_mutex_lock(pthread_mutex_t *mutex)
  {
    acquire_lock(get_me(mutex), &mutex->lock);
    return 0;
  }

  static inline int replace_pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
  {
    fprintf(stderr, "Timed locks not supported\n");
    exit(EXIT_FAILURE);
  }

  static inline int replace_pthread_mutex_unlock(pthread_mutex_t *mutex)
  {
    release_lock(get_me(mutex), &mutex->lock);
    return 0;
  }

  static inline int replace_pthread_mutex_trylock(pthread_mutex_t *mutex)
  {
    return acquire_trylock(get_me(mutex), &mutex->lock);
  }

  /*
   * Condition Variables
   */

  static inline int replace_pthread_cond_init(pthread_cond_t *cond, void *attr)
  {
    return condvar_init(cond);
  }

  static inline int replace_pthread_cond_destroy(pthread_cond_t *cond)
  {
    return condvar_destroy(cond);
  }

  static inline int replace_pthread_cond_signal(pthread_cond_t *cond)
  {
    return condvar_signal(cond);
  }

  static inline int replace_pthread_cond_broadcast(pthread_cond_t *cond)
  {
    return condvar_broadcast(cond);
  }

  static inline int replace_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
  {
    return condvar_wait(cond, get_me(mutex), &mutex->lock);
  }

  static inline int replace_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
  {
    return condvar_timedwait(cond, get_me(mutex), &mutex->lock, abstime);
  }

#ifdef __cplusplus
}

#endif

#endif