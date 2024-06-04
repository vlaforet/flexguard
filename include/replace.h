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

#define pthread_mutex_t libslock_t
#define pthread_mutex_init replace_pthread_mutex_init
#define pthread_mutex_destroy replace_pthread_mutex_destroy
#define pthread_mutex_lock libslock_lock
#define pthread_mutex_timedlock replace_pthread_mutex_timedlock
#define pthread_mutex_unlock libslock_unlock
#define pthread_mutex_trylock libslock_trylock

#define pthread_cond_init replace_pthread_cond_init
#define pthread_cond_destroy libslock_cond_destroy
#define pthread_cond_signal libslock_cond_signal
#define pthread_cond_broadcast libslock_cond_broadcast
#define pthread_cond_wait libslock_cond_wait
#define pthread_cond_timedwait libslock_cond_timedwait
#define pthread_cond_t libslock_cond_t

#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER LIBSLOCK_INITIALIZER

#undef PTHREAD_COND_INITIALIZER
#define PTHREAD_COND_INITIALIZER LIBSLOCK_COND_INITIALIZER

  /*
   * Mutex
   */

  static inline int replace_pthread_mutex_init(pthread_mutex_t *mutex, void *attr)
  {
    return libslock_init(mutex);
  }

  static inline int replace_pthread_mutex_destroy(pthread_mutex_t *mutex)
  {
    libslock_destroy(mutex);
    return 0;
  }

  static inline int replace_pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
  {
    fprintf(stderr, "Timed locks not supported\n");
    exit(EXIT_FAILURE);
  }

  /*
   * Condition Variables
   */

  static inline int replace_pthread_cond_init(pthread_cond_t *cond, void *attr)
  {
    return libslock_cond_init(cond);
  }

#ifdef __cplusplus
}

#endif

#endif