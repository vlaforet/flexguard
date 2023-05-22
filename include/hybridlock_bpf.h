/*
 * File: hybridlock_bpf.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Shared header between the BPF and userspace code for the MCS/Futex hybrid lock.
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

#define LOCK_TYPE_MCS (uint32_t)0
#define LOCK_TYPE_FUTEX (uint32_t)1

#define LOCK_LAST_TYPE(history) (uint32_t)(history >> 32)
#define LOCK_CURR_TYPE(history) (uint32_t) history

#define LOCK_TRANSITION(last, curr) (curr | (uint64_t)(last) << 32)
#define LOCK_HISTORY(type) LOCK_TRANSITION(type, type)

typedef uint32_t lock_type_t;
typedef uint64_t lock_type_history_t;

typedef struct mcs_qnode_t
{
  union
  {
    struct
    {
      volatile uint8_t waiting;
      volatile struct mcs_qnode_t *volatile next;
    };
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
  };
} mcs_qnode_t;

#endif