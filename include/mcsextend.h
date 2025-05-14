/*
 * File: mcsextend.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of an mcs lock extending its timeslice
 *      (https://lore.kernel.org/lkml/20231025054219.1acaa3dd@gandalf.local.home/)
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Victor Laforet
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

#ifndef _MCSEXTEND_H_
#define _MCSEXTEND_H_

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
#include "extend.h"

typedef struct mcsextend_qnode
{
    volatile uint8_t waiting;
    volatile struct mcsextend_qnode *volatile next;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    uint8_t padding[CACHE_LINE_SIZE - 16];
#endif
#endif
} mcsextend_qnode;

typedef mcsextend_qnode mcsextend_local_params;

typedef struct mcsextend_lock_t
{
    volatile mcsextend_qnode *tail;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE - 8];
#endif

    volatile mcsextend_qnode *qnodes;
} mcsextend_lock_t;
#define MCSEXTEND_INITIALIZER {NULL, NULL}

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
} mcsextend_cond_t;
#define MCSEXTEND_COND_INITIALIZER \
    {                              \
        {                          \
            0, 0                   \
        }                          \
    }

/*
 * Declarations
 */
int mcsextend_init(mcsextend_lock_t *the_lock);
void mcsextend_destroy(mcsextend_lock_t *the_lock);
void mcsextend_lock(mcsextend_lock_t *the_lock);
int mcsextend_trylock(mcsextend_lock_t *the_lock);
void mcsextend_unlock(mcsextend_lock_t *the_lock);

int mcsextend_cond_init(mcsextend_cond_t *cond);
int mcsextend_cond_wait(mcsextend_cond_t *cond, mcsextend_lock_t *the_lock);
int mcsextend_cond_timedwait(mcsextend_cond_t *cond, mcsextend_lock_t *the_lock, const struct timespec *ts);
int mcsextend_cond_signal(mcsextend_cond_t *cond);
int mcsextend_cond_broadcast(mcsextend_cond_t *cond);
int mcsextend_cond_destroy(mcsextend_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T mcsextend_lock_t
#define LOCKIF_INIT mcsextend_init
#define LOCKIF_DESTROY mcsextend_destroy
#define LOCKIF_LOCK mcsextend_lock
#define LOCKIF_TRYLOCK mcsextend_trylock
#define LOCKIF_UNLOCK mcsextend_unlock
#define LOCKIF_INITIALIZER MCSEXTEND_INITIALIZER

#define LOCKIF_COND_T mcsextend_cond_t
#define LOCKIF_COND_INIT mcsextend_cond_init
#define LOCKIF_COND_DESTROY mcsextend_cond_destroy
#define LOCKIF_COND_WAIT mcsextend_cond_wait
#define LOCKIF_COND_TIMEDWAIT mcsextend_cond_timedwait
#define LOCKIF_COND_SIGNAL mcsextend_cond_signal
#define LOCKIF_COND_BROADCAST mcsextend_cond_broadcast
#define LOCKIF_COND_INITIALIZER MCSEXTEND_COND_INITIALIZER

#endif
