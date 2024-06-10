/*
 * File: hybridlock_bpf.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Shared header between the BPF and userspace code for
 *      the hybrid lock with MCS, CLH or Ticket spin locks.
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

#ifndef _HYBRIDLOCK_BPF_H_
#define _HYBRIDLOCK_BPF_H_

typedef struct hybrid_qnode_t
{
  union
  {
    struct
    {
      int lock_id;

#ifdef HYBRID_TICKET
      uint32_t ticket;
#elif defined(HYBRID_CLH)
      volatile uint8_t done;
      volatile struct hybrid_qnode_t *pred;
#elif defined(HYBRID_MCS)
      volatile uint8_t waiting;
      volatile struct hybrid_qnode_t *volatile next;
#endif

#ifdef HYBRID_EPOCH
      uint8_t should_block;
#endif
    };
  };
} hybrid_qnode_t;
typedef volatile hybrid_qnode_t *hybrid_qnode_ptr;

typedef hybrid_qnode_t hybrid_qnode_thread[MAX_NUMBER_LOCKS];

#ifdef BPF
typedef struct hybrid_addresses_t
{
  union
  {
    struct
    {
      void *lock;
      void *lock_check_rcx_null;
      void *lock_spin;
      void *lock_end;

      void *unlock_check_zero_flag1;
      void *unlock_check_zero_flag2;
      void *unlock_end;
      void *unlock_end_b;
    };
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} hybrid_addresses_t;
#endif

typedef struct hybrid_thread_info_t
{
  union
  {
    struct
    {
      volatile int locking_id;
      volatile uint8_t is_running;
      uint8_t is_holder_preempted;
    };
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} hybrid_thread_info_t;

typedef struct hybrid_lock_info_t
{
  union
  {
    struct
    {
#ifdef HYBRID_MCS
      hybrid_qnode_ptr queue_lock;
#ifdef HYBRID_EPOCH
      uint64_t dummy_node_enqueued;
      uint64_t blocking_nodes;
#endif
#endif

#ifndef HYBRID_EPOCH
      unsigned long preempted_at;
#endif
    };
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} hybrid_lock_info_t;
#endif