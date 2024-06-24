/*
 * File: mutex.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Using pthread_mutex in libslock
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

#include "mutex.h"

#if USE_REAL_PTHREAD == 1
#include "interpose.h"
#endif

void mutex_lock(mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  (*REAL(pthread_mutex_lock))(the_lock);
#else
  pthread_mutex_lock(the_lock);
#endif
}

int mutex_trylock(mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  return (*REAL(pthread_mutex_trylock))(the_lock);
#else
  return pthread_mutex_trylock(the_lock);
#endif
}

void mutex_unlock(mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  (REAL(pthread_mutex_unlock))(the_lock);
#else
  pthread_mutex_unlock(the_lock);
#endif
}

int mutex_init(mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_mutex_init))(the_lock, NULL);
#else
  return pthread_mutex_init(the_lock, NULL);
#endif
}

void mutex_destroy(mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  (REAL(pthread_mutex_destroy))(the_lock);
#else
  pthread_mutex_destroy(the_lock);
#endif
}

/*
 *  Condition Variables
 */

int mutex_cond_init(mutex_cond_t *cond)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_init))(cond, NULL);
#else
  return pthread_cond_init(cond, NULL);
#endif
}

int mutex_cond_wait(mutex_cond_t *cond, mutex_lock_t *the_lock)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_wait))(cond, the_lock);
#else
  return pthread_cond_wait(cond, the_lock);
#endif
}

int mutex_cond_timedwait(mutex_cond_t *cond, mutex_lock_t *the_lock, const struct timespec *ts)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_timedwait))(cond, the_lock, ts);
#else
  return pthread_cond_timedwait(cond, the_lock, ts);
#endif
}

int mutex_cond_signal(mutex_cond_t *cond)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_signal))(cond);
#else
  return pthread_cond_signal(cond);
#endif
}

int mutex_cond_broadcast(mutex_cond_t *cond)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_broadcast))(cond);
#else
  return pthread_cond_broadcast(cond);
#endif
}

int mutex_cond_destroy(mutex_cond_t *cond)
{
#if USE_REAL_PTHREAD == 1
  return (REAL(pthread_cond_destroy))(cond);
#else
  return pthread_cond_destroy(cond);
#endif
}