/*
 * File: hybridlock.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a hybrid lock with MCS, CLH or Ticket spin locks.
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

#ifndef _HYBRIDLOCK_H_
#define _HYBRIDLOCK_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <linux/futex.h>

#include <fcntl.h>
#include <pthread.h>

#ifdef BPF
#include <bpf/libbpf.h>
#endif

#include "atomic_ops.h"
#include "utils.h"
#include "hybridlock_bpf.h"

#define LOCK_TYPE_SPIN (uint32_t)0
#define LOCK_TYPE_FUTEX (uint32_t)1

#define LOCK_LAST_TYPE(state) (uint32_t)(state >> 32)
#define LOCK_CURR_TYPE(state) (uint32_t) state

#define LOCK_TRANSITION(last, curr) ((lock_state_t)curr | (lock_state_t)(last) << 32)
#define LOCK_STABLE(type) LOCK_TRANSITION(type, type)

typedef uint32_t lock_type_t;
typedef uint64_t lock_state_t;

typedef volatile hybrid_qnode_t *hybrid_qnode_ptr;

typedef struct hybridlock_lock_t
{
  union
  {
    struct
    {
#ifdef BPF
      volatile _Atomic(unsigned long) last_waiter_at;
      uint64_t *preempted_at;
      hybrid_addresses_t *addresses;
#endif

      lock_state_t lock_state;
      struct bpf_map *nodes_map;
    };
#ifdef ADD_PADDING
    uint8_t padding1[CACHE_LINE_SIZE];
#endif
  };

  union
  {
    volatile int futex_lock;
#ifdef ADD_PADDING
    uint8_t padding2[CACHE_LINE_SIZE];
#endif
  };

  union
  {
#ifdef HYBRID_TICKET
    struct ticketlock_t
    {
      volatile uint32_t next;
      volatile uint32_t calling;
    } ticket_lock;
#elif defined(HYBRID_CLH) || defined(HYBRID_MCS)
    hybrid_qnode_ptr *queue_lock;
#endif

#ifdef ADD_PADDING
    uint8_t padding3[CACHE_LINE_SIZE];
#endif
  };
} hybridlock_lock_t;

typedef struct hybridlock_local_params_t
{
  union
  {
    volatile hybrid_qnode_t *qnode;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} hybridlock_local_params_t;

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
} hybridlock_condvar_t;

/*
 *  Lock manipulation methods
 */

void hybridlock_lock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params);
void hybridlock_unlock(hybridlock_lock_t *the_lock, hybridlock_local_params_t *local_params);

/*
 *  Methods for single lock manipulation
 */

int init_hybridlock_global(hybridlock_lock_t *the_lock);
int init_hybridlock_local(uint32_t thread_num, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock);

/*
 *  Condition Variables
 */

int hybridlock_condvar_init(hybridlock_condvar_t *cond);
int hybridlock_condvar_wait(hybridlock_condvar_t *cond, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock);
int hybridlock_condvar_timedwait(hybridlock_condvar_t *cond, hybridlock_local_params_t *local_params, hybridlock_lock_t *the_lock, const struct timespec *ts);
int hybridlock_condvar_signal(hybridlock_condvar_t *cond);
int hybridlock_condvar_broadcast(hybridlock_condvar_t *cond);
int hybridlock_condvar_destroy(hybridlock_condvar_t *cond);

#endif
