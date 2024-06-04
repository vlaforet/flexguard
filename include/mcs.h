/*
 * File: mcs.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description:
 *      Implementation of an MCS lock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David
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

#ifndef _MCS_H_
#define _MCS_H_

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

typedef struct mcs_qnode
{
    volatile uint8_t waiting;
    volatile struct mcs_qnode *volatile next;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    uint8_t padding[CACHE_LINE_SIZE - 16];
#endif
#endif
} mcs_qnode;

typedef mcs_qnode mcs_local_params;

typedef struct mcs_global_params
{
    volatile mcs_qnode *tail;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif
} mcs_global_params;
#define MCS_GLOBAL_INITIALIZER \
    {                          \
        .tail = 0              \
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
} mcs_condvar_t;
#define MCS_COND_INITIALIZER  \
    {                         \
        .seq = 0, .target = 0 \
    }

/*
 * Methods for single lock manipulation
 */
int init_mcs_global(mcs_global_params *global);
int init_mcs_local(mcs_global_params *global, mcs_local_params *local);
void end_mcs_local(mcs_local_params *local);
void end_mcs_global(mcs_global_params *global);

/*
 *  Acquire and release methods
 */
void mcs_lock(mcs_global_params *global, mcs_local_params *local);
int mcs_trylock(mcs_global_params *global, mcs_local_params *local);
void mcs_unlock(mcs_global_params *global, mcs_local_params *local);

/*
 *  Condition Variables
 */
int mcs_condvar_init(mcs_condvar_t *cond);
int mcs_condvar_wait(mcs_condvar_t *cond, mcs_global_params *global, mcs_local_params *local);
int mcs_condvar_timedwait(mcs_condvar_t *cond, mcs_global_params *global, mcs_local_params *local, const struct timespec *ts);
int mcs_condvar_signal(mcs_condvar_t *cond);
int mcs_condvar_broadcast(mcs_condvar_t *cond);
int mcs_condvar_destroy(mcs_condvar_t *cond);

#define LOCAL_NEEDED 1

#define GLOBAL_DATA_T mcs_global_params
#define LOCAL_DATA_T mcs_local_params
#define CONDVAR_DATA_T mcs_condvar_t

#define INIT_LOCAL_DATA init_mcs_local
#define INIT_GLOBAL_DATA init_mcs_global
#define DESTROY_GLOBAL_DATA end_mcs_global

#define ACQUIRE_LOCK mcs_lock
#define RELEASE_LOCK mcs_unlock
#define ACQUIRE_TRYLOCK mcs_trylock

#define COND_INIT mcs_condvar_init
#define COND_WAIT mcs_condvar_wait
#define COND_TIMEDWAIT mcs_condvar_timedwait
#define COND_SIGNAL mcs_condvar_signal
#define COND_BROADCAST mcs_condvar_broadcast
#define COND_DESTROY mcs_condvar_destroy

#define LOCK_GLOBAL_INITIALIZER MCS_GLOBAL_INITIALIZER
#define COND_INITIALIZER MCS_COND_INITIALIZER

#endif
