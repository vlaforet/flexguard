/*
 * File: mcstas.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of an MCS/TAS lock
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

#ifndef _MCSTAS_H_
#define _MCSTAS_H_

#include <stdatomic.h>
#include "utils.h"
#include "atomic_ops.h"

typedef struct mcstas_qnode
{
    union
    {
        struct
        {
            volatile uint8_t waiting;
            volatile struct mcstas_qnode *volatile next;
        };
#ifdef ADD_PADDING
        uint8_t padding[CACHE_LINE_SIZE];
#endif
    };
} mcstas_qnode;

typedef union
{
    struct
    {
        volatile mcstas_qnode *tail;
        volatile uint8_t lock;
    };
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#endif
} mcstas_lock_t;
#define MCSTAS_INITIALIZER   \
    {                        \
        .tail = 0, .lock = 0 \
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
} mcstas_cond_t;
#define MCSTAS_COND_INITIALIZER \
    {                           \
        .seq = 0, .target = 0   \
    }

/*
 * Declarations
 */
int mcstas_init(mcstas_lock_t *the_lock);
void mcstas_destroy(mcstas_lock_t *the_lock);
void mcstas_lock(mcstas_lock_t *the_lock);
int mcstas_trylock(mcstas_lock_t *the_lock);
void mcstas_unlock(mcstas_lock_t *the_lock);

int mcstas_cond_init(mcstas_cond_t *cond);
int mcstas_cond_wait(mcstas_cond_t *cond, mcstas_lock_t *the_lock);
int mcstas_cond_timedwait(mcstas_cond_t *cond, mcstas_lock_t *the_lock, const struct timespec *ts);
int mcstas_cond_signal(mcstas_cond_t *cond);
int mcstas_cond_broadcast(mcstas_cond_t *cond);
int mcstas_cond_destroy(mcstas_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T mcstas_lock_t
#define LOCKIF_INIT mcstas_init
#define LOCKIF_DESTROY mcstas_destroy
#define LOCKIF_LOCK mcstas_lock
#define LOCKIF_UNLOCK mcstas_unlock
#define LOCKIF_INITIALIZER MCSTAS_INITIALIZER

#define LOCKIF_COND_T mcstas_cond_t
#define LOCKIF_COND_INIT mcstas_cond_init
#define LOCKIF_COND_DESTROY mcstas_cond_destroy
#define LOCKIF_COND_WAIT mcstas_cond_wait
#define LOCKIF_COND_TIMEDWAIT mcstas_cond_timedwait
#define LOCKIF_COND_SIGNAL mcstas_cond_signal
#define LOCKIF_COND_BROADCAST mcstas_cond_broadcast
#define LOCKIF_COND_INITIALIZER MCSTAS_COND_INITIALIZER

#endif
