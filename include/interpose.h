/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Hugo Guiroux <hugo.guiroux at gmail dot com>
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

#define INTERPOSE_RWLOCK 0
#define INTERPOSE_SPINLOCK 1

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

extern int (*REAL(pthread_create))(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

#define CAST_TO_LOCK(input) ((lock_as_t *)input)
#define CAST_TO_COND(input) ((condvar_as_t *)input)

typedef struct lock_as_context_t
{
  uint8_t status;
  lock_local_data me __attribute__((aligned(CACHE_LINE_SIZE)));
} lock_as_context_t __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct lock_as_t
{
  volatile uint8_t status;
  lock_global_data *lock;
  lock_as_context_t *contexts;
} lock_as_t;

typedef struct condvar_as_t
{
  volatile uint8_t status;
  lock_condvar_t *cond;
} condvar_as_t;

#endif // __INTERPOSE_H__
