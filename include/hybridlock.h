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

#include <fcntl.h>
#include <pthread.h>

#ifdef BPF
#include <bpf/libbpf.h>
#endif

#include "atomic_ops.h"
#include "utils.h"
#include "hybridlock_bpf.h"

#ifdef TRACING
#define TRACING_EVENT_SWITCH_BLOCK 0
#define TRACING_EVENT_SWITCH_SPIN 1
#endif

#define LOCK_TYPE_SPIN (lock_type_t)0
#define LOCK_TYPE_FUTEX (lock_type_t)1
typedef uint32_t lock_type_t;

#define LOCK_LAST_TYPE(state) (lock_type_t)(state >> 32)
#define LOCK_CURR_TYPE(state) (lock_type_t) state

#define LOCK_TRANSITION(last, curr) ((lock_state_t)curr | (lock_state_t)(last) << 32)
#define LOCK_STABLE(type) LOCK_TRANSITION(type, type)

typedef uint64_t lock_state_t;

typedef struct hybridlock_local_params_t
{
  union
  {
    hybrid_qnode_ptr qnode;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} hybridlock_local_params_t;

typedef struct hybridlock_lock_t
{
  union
  {
    struct
    {
#ifdef BPF
#ifndef HYBRID_EPOCH
      volatile _Atomic(unsigned long) last_switched_at;
      unsigned long *preempted_at;
#endif

      hybrid_addresses_t *addresses;
      struct bpf_map *nodes_map;
      hybrid_qnode_ptr qnode_allocation_array;
#endif

#ifdef HYBRID_EPOCH
      hybridlock_local_params_t *dummy_params;
      uint64_t *dummy_node_enqueued;
      uint64_t *blocking_nodes;
#else
      lock_state_t lock_state;
#endif

#ifdef TRACING
      void *tracing_fn_data;
      void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data);
#endif

      _Atomic(int) thread_count;
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
    struct
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

#ifndef HYBRID_EPOCH
      volatile _Atomic(unsigned long) last_waiter_at;
#endif
    };

#ifdef ADD_PADDING
    uint8_t padding3[CACHE_LINE_SIZE];
#endif
  };
} hybridlock_lock_t;

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

#ifdef TRACING
void set_tracing_fn(hybridlock_lock_t *the_lock, void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data), void *tracing_fn_data);
#endif

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
