/*
 * File: uscl.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of u-scl lock from
 *      https://github.com/scheduler-cooperative-locks/proportional-share/blob/master/u-scl/fairlock.h
 *      Paper: https://research.cs.wisc.edu/adsl/Publications/eurosys20-scl.pdf
 *
 * The MIT License (MIT)
 *
 * Copyright (c) Victor Laforet
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

#ifndef __USCL_H__
#define __USCL_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include "utils.h"
#include "atomic_ops.h"
#include <sys/resource.h>

#define CYCLE_PER_US 2200L

#ifndef SPIN_LIMIT
#define SPIN_LIMIT 20
#endif
#define SLEEP_GRANULARITY 8

#ifndef CYCLE_PER_US
#error Must define CYCLE_PER_US for the current machine in Makefile or elsewhere
#endif
#define CYCLE_PER_MS (CYCLE_PER_US * 1000L)
#define CYCLE_PER_S (CYCLE_PER_MS * 1000L)
#define FAIRLOCK_GRANULARITY (CYCLE_PER_MS * 2L)

#define readvol(lvalue) (*(volatile typeof(lvalue) *)(&lvalue))

#define spin_then_yield(limit, expr)            \
  while (1)                                     \
  {                                             \
    int val, counter = 0;                       \
    while ((val = (expr)) && counter++ < limit) \
      PAUSE;                                    \
    if (!val)                                   \
      break;                                    \
    sched_yield();                              \
  }

static const int prio_to_weight[40] = {
    /* -20 */ 88761,
    71755,
    56483,
    46273,
    36291,
    /* -15 */ 29154,
    23254,
    18705,
    14949,
    11916,
    /* -10 */ 9548,
    7620,
    6100,
    4904,
    3906,
    /*  -5 */ 3121,
    2501,
    1991,
    1586,
    1277,
    /*   0 */ 1024,
    820,
    655,
    526,
    423,
    /*   5 */ 335,
    272,
    215,
    172,
    137,
    /*  10 */ 110,
    87,
    70,
    56,
    45,
    /*  15 */ 36,
    29,
    23,
    18,
    15,
};

#ifdef DEBUG
typedef struct stats
{
  unsigned long long reenter;
  unsigned long long banned_time;
  unsigned long long start;
  unsigned long long next_runnable_wait;
  unsigned long long prev_slice_wait;
  unsigned long long own_slice_wait;
  unsigned long long runnable_wait;
  unsigned long long succ_wait;
  unsigned long long release_succ_wait;
} stats_t;
#endif

typedef struct flthread_info
{
  unsigned long long banned_until;
  unsigned long long weight;
  unsigned long long slice;
  unsigned long long start_ticks;
  int banned;
#ifdef DEBUG
  stats_t stat;
#endif
} flthread_info_t;

enum qnode_state
{
  INIT = 0, // not waiting or after next runnable node
  NEXT,
  RUNNABLE,
  RUNNING
};

typedef struct uscl_qnode
{
  int state __attribute__((aligned(CACHE_LINE_SIZE)));
  struct uscl_qnode *next __attribute__((aligned(CACHE_LINE_SIZE)));
} uscl_qnode_t __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct uscl_lock
{
  uscl_qnode_t *qtail __attribute__((aligned(CACHE_LINE_SIZE)));
  uscl_qnode_t *qnext __attribute__((aligned(CACHE_LINE_SIZE)));
  unsigned long long slice __attribute__((aligned(CACHE_LINE_SIZE)));
  int slice_valid __attribute__((aligned(CACHE_LINE_SIZE)));
  pthread_key_t flthread_info_key;
  unsigned long long total_weight;
} uscl_lock_t __attribute__((aligned(CACHE_LINE_SIZE)));
#define USCL_INITIALIZER {NULL, NULL, NULL, NULL, NULL, NULL}

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
} uscl_cond_t;
#define USCL_COND_INITIALIZER {{0, 0}}

/*
 * Declarations
 */
int uscl_init(uscl_lock_t *the_lock);
void uscl_destroy(uscl_lock_t *the_lock);
void uscl_lock(uscl_lock_t *the_lock);
void uscl_unlock(uscl_lock_t *the_lock);

int uscl_cond_init(uscl_cond_t *cond);
int uscl_cond_wait(uscl_cond_t *cond, uscl_lock_t *the_lock);
int uscl_cond_timedwait(uscl_cond_t *cond, uscl_lock_t *the_lock, const struct timespec *ts);
int uscl_cond_signal(uscl_cond_t *cond);
int uscl_cond_broadcast(uscl_cond_t *cond);
int uscl_cond_destroy(uscl_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T uscl_lock_t
#define LOCKIF_INIT uscl_init
#define LOCKIF_DESTROY uscl_destroy
#define LOCKIF_LOCK uscl_lock
#define LOCKIF_UNLOCK uscl_unlock
#define LOCKIF_INITIALIZER USCL_INITIALIZER

#define LOCKIF_COND_T uscl_cond_t
#define LOCKIF_COND_INIT uscl_cond_init
#define LOCKIF_COND_DESTROY uscl_cond_destroy
#define LOCKIF_COND_WAIT uscl_cond_wait
#define LOCKIF_COND_TIMEDWAIT uscl_cond_timedwait
#define LOCKIF_COND_SIGNAL uscl_cond_signal
#define LOCKIF_COND_BROADCAST uscl_cond_broadcast
#define LOCKIF_COND_INITIALIZER USCL_COND_INITIALIZER

#endif
