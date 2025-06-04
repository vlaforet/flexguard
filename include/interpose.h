/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Victor Laforet <victor.laforet@inria.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of his software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __INTERPOSE_H__
#define __INTERPOSE_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include "utils.h"
#include "atomic_ops.h"
#include "lock_if.h"

#define INTERPOSE_RWLOCK 1
#define INTERPOSE_SPINLOCK 0
#define INTERPOSE_BARRIERS 1

#define PASTER(x, y) real_##x##_##y
#define EVALUATOR(x, y) PASTER(x, y)

#define FCT_LINK_SUFFIX interpose
#define REAL(name) EVALUATOR(name, FCT_LINK_SUFFIX)
#define S(_) #_

#define LOAD_FUNC(name, E)                                               \
  do                                                                     \
  {                                                                      \
    REAL(name) = dlsym(RTLD_NEXT, S(name));                              \
    if (E && !REAL(name))                                                \
      fprintf(stderr, "WARNING: unable to find symbol: %s.\n", S(name)); \
  } while (0)

#define LOAD_FUNC_VERSIONED(name, E, version)                            \
  do                                                                     \
  {                                                                      \
    REAL(name) = dlvsym(RTLD_NEXT, S(name), version);                    \
    if (E && !REAL(name))                                                \
      fprintf(stderr, "WARNING: unable to find symbol: %s.\n", S(name)); \
  } while (0)

#define GLIBC_2_2_5 "GLIBC_2.2.5"
#define GLIBC_2_3_2 "GLIBC_2.3.2"
#define GLIBC_2_34 "GLIBC_2.34"

#if USE_REAL_PTHREAD == 1
extern int (*REAL(pthread_mutex_init))(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
extern int (*REAL(pthread_mutex_destroy))(pthread_mutex_t *mutex);
extern int (*REAL(pthread_mutex_lock))(pthread_mutex_t *mutex);
extern int (*REAL(pthread_mutex_timedlock))(pthread_mutex_t *mutex, const struct timespec *abstime);
extern int (*REAL(pthread_mutex_trylock))(pthread_mutex_t *mutex);
extern int (*REAL(pthread_mutex_unlock))(pthread_mutex_t *mutex);
extern int (*REAL(pthread_cond_init))(pthread_cond_t *cond, const pthread_condattr_t *attr);
extern int (*REAL(pthread_cond_destroy))(pthread_cond_t *cond);
extern int (*REAL(pthread_cond_timedwait))(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
extern int (*REAL(pthread_cond_wait))(pthread_cond_t *cond, pthread_mutex_t *mutex);
extern int (*REAL(pthread_cond_signal))(pthread_cond_t *cond);
extern int (*REAL(pthread_cond_broadcast))(pthread_cond_t *cond);
#endif

#define CAST_TO_LOCK(input) ((lock_as_t *)input)
#define CAST_TO_COND(input) ((condvar_as_t *)input)

typedef struct lock_as_t
{
  volatile uint8_t status;
  libslock_t *lock;
} lock_as_t;

typedef struct condvar_as_t
{
  volatile uint8_t status;
  libslock_cond_t *cond;
} condvar_as_t;

#if INTERPOSE_BARRIERS
typedef struct barrier_as_t
{
  volatile uint8_t status;
  libslock_barrier_t *barrier;
} barrier_as_t;
#endif

#endif // __INTERPOSE_H__
