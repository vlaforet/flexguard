/*
 * File: flexguard.h
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

#ifndef _FLEXGUARD_H_
#define _FLEXGUARD_H_

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

#include "atomic_ops.h"
#include "utils.h"
#include "flexguard_bpf.h"

#ifdef TRACING
#define TRACING_EVENT_ACQUIRED_SPIN 0
#define TRACING_EVENT_ACQUIRED_BLOCK 1
#define TRACING_EVENT_ACQUIRED_STOLEN 2
#endif

#ifdef FLEXGUARD_EXTEND
#include "extend.h"
#endif

typedef struct flexguard_lock_t
{
  union
  {
    struct
    {
      int id;

#ifdef TRACING
      void *tracing_fn_data;
      void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data);
#endif
    };
#ifdef ADD_PADDING
    uint8_t padding1[CACHE_LINE_SIZE];
#endif
  };

  union
  {
    volatile int lock_value;
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
      flexguard_qnode_ptr queue;
#endif
    };

#ifdef ADD_PADDING
    uint8_t padding3[CACHE_LINE_SIZE];
#endif
  };
} flexguard_lock_t;
#define FLEXGUARD_INITIALIZER \
  {                           \
  }

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
} flexguard_cond_t;
#define FLEXGUARD_COND_INITIALIZER \
  {                                \
    {                              \
      0, 0                         \
    }                              \
  }

/*
 *  Declarations
 */

int flexguard_init(flexguard_lock_t *the_lock);
void flexguard_destroy(flexguard_lock_t *the_lock);
void flexguard_lock(flexguard_lock_t *the_lock);
void flexguard_unlock(flexguard_lock_t *the_lock);

#ifdef TRACING
void set_tracing_fn(flexguard_lock_t *the_lock, void (*tracing_fn)(ticks rtsp, int event_type, void *event_data, void *fn_data), void *tracing_fn_data);
#endif

int flexguard_cond_init(flexguard_cond_t *cond);
int flexguard_cond_wait(flexguard_cond_t *cond, flexguard_lock_t *the_lock);
int flexguard_cond_timedwait(flexguard_cond_t *cond, flexguard_lock_t *the_lock, const struct timespec *ts);
int flexguard_cond_signal(flexguard_cond_t *cond);
int flexguard_cond_broadcast(flexguard_cond_t *cond);
int flexguard_cond_destroy(flexguard_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T flexguard_lock_t
#define LOCKIF_INIT flexguard_init
#define LOCKIF_DESTROY flexguard_destroy
#define LOCKIF_LOCK flexguard_lock
#define LOCKIF_UNLOCK flexguard_unlock
#define LOCKIF_INITIALIZER FLEXGUARD_INITIALIZER

#define LOCKIF_COND_T flexguard_cond_t
#define LOCKIF_COND_INIT flexguard_cond_init
#define LOCKIF_COND_DESTROY flexguard_cond_destroy
#define LOCKIF_COND_WAIT flexguard_cond_wait
#define LOCKIF_COND_TIMEDWAIT flexguard_cond_timedwait
#define LOCKIF_COND_SIGNAL flexguard_cond_signal
#define LOCKIF_COND_BROADCAST flexguard_cond_broadcast
#define LOCKIF_COND_INITIALIZER FLEXGUARD_COND_INITIALIZER

#endif
