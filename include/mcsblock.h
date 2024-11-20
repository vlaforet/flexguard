/*
 * File: mcsblock.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Blocking MCS lock implementation
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Victor Laforet
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

#ifndef _MCSBLOCK_H_
#define _MCSBLOCK_H_

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

typedef struct mcsblock_qnode
{
    volatile uint8_t waiting;
    volatile struct mcsblock_qnode *volatile next;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    uint8_t padding[CACHE_LINE_SIZE - 16];
#endif
#endif
} mcsblock_qnode;

typedef mcsblock_qnode mcsblock_local_params;

typedef struct mcsblock_lock_t
{
    volatile mcsblock_qnode *tail;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif

    volatile mcsblock_qnode qnodes[MAX_NUMBER_THREADS];
} mcsblock_lock_t;
#define MCSBLOCK_INITIALIZER {NULL, NULL}

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
} mcsblock_cond_t;
#define MCSBLOCK_COND_INITIALIZER \
    {                             \
        {                         \
            0, 0                  \
        }                         \
    }

/*
 * Declarations
 */
int mcsblock_init(mcsblock_lock_t *the_lock);
void mcsblock_destroy(mcsblock_lock_t *the_lock);
void mcsblock_lock(mcsblock_lock_t *the_lock);
int mcsblock_trylock(mcsblock_lock_t *the_lock);
void mcsblock_unlock(mcsblock_lock_t *the_lock);

int mcsblock_cond_init(mcsblock_cond_t *cond);
int mcsblock_cond_wait(mcsblock_cond_t *cond, mcsblock_lock_t *the_lock);
int mcsblock_cond_timedwait(mcsblock_cond_t *cond, mcsblock_lock_t *the_lock, const struct timespec *ts);
int mcsblock_cond_signal(mcsblock_cond_t *cond);
int mcsblock_cond_broadcast(mcsblock_cond_t *cond);
int mcsblock_cond_destroy(mcsblock_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T mcsblock_lock_t
#define LOCKIF_INIT mcsblock_init
#define LOCKIF_DESTROY mcsblock_destroy
#define LOCKIF_LOCK mcsblock_lock
#define LOCKIF_UNLOCK mcsblock_unlock
#define LOCKIF_INITIALIZER MCSBLOCK_INITIALIZER

#define LOCKIF_COND_T mcsblock_cond_t
#define LOCKIF_COND_INIT mcsblock_cond_init
#define LOCKIF_COND_DESTROY mcsblock_cond_destroy
#define LOCKIF_COND_WAIT mcsblock_cond_wait
#define LOCKIF_COND_TIMEDWAIT mcsblock_cond_timedwait
#define LOCKIF_COND_SIGNAL mcsblock_cond_signal
#define LOCKIF_COND_BROADCAST mcsblock_cond_broadcast
#define LOCKIF_COND_INITIALIZER MCSBLOCK_COND_INITIALIZER

#endif
