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
#undef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER LIBSLOCK_INITIALIZER
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER

#define pthread_cond_t libslock_cond_t
#undef PTHREAD_COND_INITIALIZER
#define PTHREAD_COND_INITIALIZER LIBSLOCK_COND_INITIALIZER

  /*
   * Mutex
   */

  static inline int replace_pthread_mutex_init(pthread_mutex_t *mutex, __attribute__((unused)) const pthread_mutexattr_t *attr) noexcept
  {
    return libslock_init(mutex);
  }
#define pthread_mutex_init replace_pthread_mutex_init

  static inline int replace_pthread_mutex_destroy(pthread_mutex_t *mutex) noexcept
  {
    libslock_destroy(mutex);
    return 0;
  }
#define pthread_mutex_destroy replace_pthread_mutex_destroy

  static inline int replace_pthread_mutex_lock(pthread_mutex_t *mutex) noexcept
  {
    libslock_lock(mutex);
    return 0;
  }
#define pthread_mutex_lock replace_pthread_mutex_lock

  static inline int replace_pthread_mutex_unlock(pthread_mutex_t *mutex) noexcept
  {
    libslock_unlock(mutex);
    return 0;
  }
#define pthread_mutex_unlock replace_pthread_mutex_unlock

  static inline int replace_pthread_mutex_clocklock(__attribute__((unused)) pthread_mutex_t *mutex, __attribute__((unused)) clockid_t clock_id, __attribute__((unused)) const struct timespec *abstime) noexcept
  {
    fprintf(stderr, "Clock locks not supported\n");
    exit(EXIT_FAILURE);
  }
#define pthread_mutex_clocklock replace_pthread_mutex_clocklock

  static inline int replace_pthread_mutex_timedlock(__attribute__((unused)) pthread_mutex_t *mutex, __attribute__((unused)) const struct timespec *abstime) noexcept
  {
    fprintf(stderr, "Timed locks not supported\n");
    exit(EXIT_FAILURE);
  }
#define pthread_mutex_timedlock replace_pthread_mutex_timedlock

  static inline int replace_pthread_mutex_trylock(pthread_mutex_t *mutex) noexcept
  {
    return libslock_trylock(mutex);
  }
#define pthread_mutex_trylock replace_pthread_mutex_trylock

  /*
   * Condition Variables
   */

  static inline int replace_pthread_cond_init(pthread_cond_t *cond, __attribute__((unused)) const pthread_condattr_t *attr) noexcept
  {
    return libslock_cond_init(cond);
  }
#define pthread_cond_init replace_pthread_cond_init

  static inline int replace_pthread_cond_destroy(pthread_cond_t *cond) noexcept
  {
    return libslock_cond_destroy(cond);
  }
#define pthread_cond_destroy replace_pthread_cond_destroy

  static inline int replace_pthread_cond_signal(pthread_cond_t *cond) noexcept
  {
    return libslock_cond_signal(cond);
  }
#define pthread_cond_signal replace_pthread_cond_signal

  static inline int replace_pthread_cond_broadcast(pthread_cond_t *cond) noexcept
  {
    return libslock_cond_broadcast(cond);
  }
#define pthread_cond_broadcast replace_pthread_cond_broadcast

  static inline int replace_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
  {
    return libslock_cond_wait(cond, mutex);
  }
#define pthread_cond_wait replace_pthread_cond_wait

  static inline int replace_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const timespec *ts)
  {
    return libslock_cond_timedwait(cond, mutex, ts);
  }
#define pthread_cond_timedwait replace_pthread_cond_timedwait

  static inline int replace_pthread_cond_clockwait(__attribute__((unused)) pthread_cond_t *cond, __attribute__((unused)) pthread_mutex_t *mutex, __attribute__((unused)) clockid_t clk, __attribute__((unused)) const struct timespec *abstime)
  {
    fprintf(stderr, "Clock condition variables not supported\n");
    exit(EXIT_FAILURE);
  }
#define pthread_cond_clockwait replace_pthread_cond_clockwait

#ifdef __cplusplus
}

#endif

#endif