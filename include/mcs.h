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

typedef struct mcs_lock_t
{
    volatile mcs_qnode *tail;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif

    volatile mcs_qnode qnodes[MAX_NUMBER_THREADS];
} mcs_lock_t;
#define MCS_INITIALIZER \
    {                   \
        .tail = 0       \
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
} mcs_cond_t;
#define MCS_COND_INITIALIZER  \
    {                         \
        .seq = 0, .target = 0 \
    }

/*
 * Declarations
 */
int mcs_init(mcs_lock_t *the_lock);
void mcs_destroy(mcs_lock_t *the_lock);
void mcs_lock(mcs_lock_t *the_lock);
int mcs_trylock(mcs_lock_t *the_lock);
void mcs_unlock(mcs_lock_t *the_lock);

int mcs_cond_init(mcs_cond_t *cond);
int mcs_cond_wait(mcs_cond_t *cond, mcs_lock_t *the_lock);
int mcs_cond_timedwait(mcs_cond_t *cond, mcs_lock_t *the_lock, const struct timespec *ts);
int mcs_cond_signal(mcs_cond_t *cond);
int mcs_cond_broadcast(mcs_cond_t *cond);
int mcs_cond_destroy(mcs_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T mcs_lock_t
#define LOCKIF_INIT mcs_init
#define LOCKIF_DESTROY mcs_destroy
#define LOCKIF_LOCK mcs_lock
#define LOCKIF_UNLOCK mcs_unlock
#define LOCKIF_INITIALIZER MCS_INITIALIZER

#define LOCKIF_COND_T mcs_cond_t
#define LOCKIF_COND_INIT mcs_cond_init
#define LOCKIF_COND_DESTROY mcs_cond_destroy
#define LOCKIF_COND_WAIT mcs_cond_wait
#define LOCKIF_COND_TIMEDWAIT mcs_cond_timedwait
#define LOCKIF_COND_SIGNAL mcs_cond_signal
#define LOCKIF_COND_BROADCAST mcs_cond_broadcast
#define LOCKIF_COND_INITIALIZER MCS_COND_INITIALIZER

#endif
