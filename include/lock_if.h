/*
 * File: lock_if.h
 * Authors: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Common interface to various locking algorithms;
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

#ifndef __LOCK_IF_H__
#define __LOCK_IF_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef USE_MCS_LOCKS
#include "mcs.h"
#elif defined(USE_MCSEXTEND_LOCKS)
#include "mcsextend.h"
#elif defined(USE_MCSBLOCK_LOCKS)
#include "mcsblock.h"
#elif defined(USE_MCSTAS_LOCKS)
#include "mcstas.h"
#elif defined(USE_SPINLOCK_LOCKS)
#include "spinlock.h"
#elif defined(USE_SPINEXTEND_LOCKS)
#include "spinextend.h"
#elif defined(USE_HYBRIDLOCK_LOCKS)
#include "hybridlock.h"
#elif defined(USE_FLEXGUARD_LOCKS)
#include "flexguard.h"
#elif defined(USE_TICKET_LOCKS)
#include "ticket.h"
#elif defined(USE_MUTEX_LOCKS)
#include "mutex.h"
#elif defined(USE_FUTEX_LOCKS)
#include "futex.h"
#elif defined(USE_SPINPARK_LOCKS)
#include "spinpark.h"
#elif defined(USE_CLH_LOCKS)
#include "clh.h"
#elif defined(USE_USCL_LOCKS)
#include "uscl.h"

#elif defined(USE_SHUFFLE_LOCKS) // LiTL locks
#include "shuffle.h"
#elif defined(USE_MALTHUSIAN_LOCKS) // LiTL locks
#include "malthusian.h"
#elif defined(USE_MCSTP_LOCKS) // LiTL locks
#include "mcstp.h"
#else
#error "No type of locks given"
#endif

/*
 * Types
 */

#ifdef LOCKIF_LOCK_T
typedef LOCKIF_LOCK_T libslock_t;
#else
#error "LOCKIF_LOCK_T not defined"
#endif

#ifdef LOCKIF_INITIALIZER
#define LIBSLOCK_INITIALIZER LOCKIF_INITIALIZER
#else
#define LIBSLOCK_INITIALIZER \
    {                        \
    }
#endif

#ifdef LOCKIF_COND_T
typedef LOCKIF_COND_T libslock_cond_t;
#else
typedef int libslock_cond_t;
#endif

#ifdef LOCKIF_COND_INITIALIZER
#define LIBSLOCK_COND_INITIALIZER LOCKIF_COND_INITIALIZER
#else
#define LIBSLOCK_COND_INITIALIZER \
    {                             \
    }
#endif

#ifdef LOCKIF_BARRIERATTR_T
typedef LOCKIF_BARRIERATTR_T libslock_barrierattr_t;
#else
typedef int libslock_barrierattr_t;
#endif

#ifdef LOCKIF_BARRIER_T
typedef LOCKIF_BARRIER_T libslock_barrier_t;
#else
typedef struct
{
    libslock_t lock;
    libslock_cond_t cond;
    unsigned max;
    volatile unsigned n;
    volatile int is_arrival_phase;
} libslock_barrier_t;
#endif

/*
 *  Declarations
 */

static inline int libslock_init(libslock_t *lock);
static inline void libslock_destroy(libslock_t *lock);
static inline void libslock_lock(libslock_t *lock);
static inline int libslock_trylock(libslock_t *lock);
static inline void libslock_unlock(libslock_t *lock);

static inline int libslock_cond_init(libslock_cond_t *cond);
static inline int libslock_cond_destroy(libslock_cond_t *cond);
static inline int libslock_cond_wait(libslock_cond_t *cond, libslock_t *lock);
static inline int libslock_cond_timedwait(libslock_cond_t *cond, libslock_t *lock, const struct timespec *ts);
static inline int libslock_cond_signal(libslock_cond_t *cond);
static inline int libslock_cond_broadcast(libslock_cond_t *cond);

static inline int libslock_barrier_init(libslock_barrier_t *barrier, const libslock_barrierattr_t *attr, unsigned count);
static inline int libslock_barrier_destroy(libslock_barrier_t *barrier);
static inline int libslock_barrier_wait(libslock_barrier_t *barrier);
static inline int libslock_barrierattr_init(libslock_barrierattr_t *attr);
static inline int libslock_barrierattr_destroy(libslock_barrierattr_t *attr);
static inline int libslock_barrierattr_getpshared(const libslock_barrierattr_t *attr, int *pshared);
static inline int libslock_barrierattr_setpshared(libslock_barrierattr_t *attr, int pshared);

/*
 *  Lock Functions
 */

static inline int libslock_init(libslock_t *lock)
{
    return LOCKIF_INIT(lock);
}

static inline void libslock_destroy(libslock_t *lock)
{
#ifdef PAUSE_COUNTER
    printf("Pauses: %ld\n", pause_counter);
#endif
    LOCKIF_DESTROY(lock);
}

static inline void libslock_lock(libslock_t *lock)
{
    return LOCKIF_LOCK(lock);
}

static inline int libslock_trylock(libslock_t *lock)
{
#ifdef LOCKIF_TRYLOCK
    return LOCKIF_TRYLOCK(lock);
#else
    fprintf(stderr, "Trylock not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline void libslock_unlock(libslock_t *lock)
{
    LOCKIF_UNLOCK(lock);
}

/*
 *  Condition Variables Functions
 */

static inline int libslock_cond_init(libslock_cond_t *cond)
{
#ifdef LOCKIF_COND_INIT
    return LOCKIF_COND_INIT(cond);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_destroy(libslock_cond_t *cond)
{
#ifdef LOCKIF_COND_DESTROY
    return LOCKIF_COND_DESTROY(cond);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_wait(libslock_cond_t *cond, libslock_t *lock)
{
#ifdef LOCKIF_COND_WAIT
    return LOCKIF_COND_WAIT(cond, lock);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_timedwait(libslock_cond_t *cond, libslock_t *lock, const struct timespec *ts)
{
#ifdef LOCKIF_COND_TIMEDWAIT
    return LOCKIF_COND_TIMEDWAIT(cond, lock, ts);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_signal(libslock_cond_t *cond)
{
#ifdef LOCKIF_COND_SIGNAL
    return LOCKIF_COND_SIGNAL(cond);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_broadcast(libslock_cond_t *cond)
{
#ifdef LOCKIF_COND_BROADCAST
    return LOCKIF_COND_BROADCAST(cond);
#else
    fprintf(stderr, "Condition variables not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

/*
 *  Barrier Functions
 */

static inline int libslock_barrier_init(libslock_barrier_t *barrier, const libslock_barrierattr_t *attr, unsigned count)
{
#ifdef LOCKIF_BARRIER_INIT
    return LOCKIF_BARRIER_INIT(barrier, attr, count);
#else
    int rv;

    if (barrier == NULL || count <= 0)
        return EINVAL;

    if (attr != NULL && *attr != PTHREAD_PROCESS_PRIVATE)
    {
        fprintf(stderr, "Only private barriers are supported.\n");
        return EINVAL;
    }

    barrier->max = count;
    barrier->n = 0;
    barrier->is_arrival_phase = 1;

    if ((rv = libslock_init(&barrier->lock)) != 0)
        return rv;
    return libslock_cond_init(&barrier->cond);
#endif
}

static inline int libslock_barrier_destroy(libslock_barrier_t *barrier)
{
#ifdef LOCKIF_BARRIER_DESTROY
    return LOCKIF_BARRIER_DESTROY(barrier);
#else
    int rv;

    if (barrier == NULL)
        return EINVAL;

    libslock_destroy(&barrier->lock);
    rv = libslock_cond_destroy(&barrier->cond);
    if (rv != 0)
        return rv;

    // If the barrier is still in use then the other destroy functions should
    // have returned an error, but we check anyway to catch any other unexpected errors.
    if (barrier->n != 0)
        return EBUSY;

    return 0;
#endif
}

static inline int libslock_barrier_wait(libslock_barrier_t *barrier)
{
#ifdef LOCKIF_BARRIER_WAIT
    return LOCKIF_BARRIER_WAIT(barrier);
#else
    int master, rv;

    if (barrier == NULL)
        return EINVAL;

    libslock_lock(&barrier->lock);

    while (!barrier->is_arrival_phase)
    {
        rv = libslock_cond_wait(&barrier->cond, &barrier->lock);
        if (rv != 0)
        {
            libslock_unlock(&barrier->lock);
            return rv;
        }
    }

    master = (barrier->n == 0);
    barrier->n++;
    if (barrier->n >= barrier->max)
    {
        barrier->is_arrival_phase = 0;
        libslock_cond_broadcast(&barrier->cond);
    }
    else
    {
        while (barrier->is_arrival_phase)
        {
            rv = libslock_cond_wait(&barrier->cond, &barrier->lock);
            if (rv != 0)
            {
                libslock_unlock(&barrier->lock);
                return rv;
            }
        }
    }
    barrier->n--;
    if (barrier->n == 0)
    {
        barrier->is_arrival_phase = 1;
        libslock_cond_broadcast(&barrier->cond);
    }
    libslock_unlock(&barrier->lock);

    return master ? PTHREAD_BARRIER_SERIAL_THREAD : 0;
#endif
}

static inline int libslock_barrierattr_init(libslock_barrierattr_t *attr)
{
#ifdef LOCKIF_BARRIERATTR_INIT
    return LOCKIF_BARRIERATTR_INIT(attr);
#else
    if (attr == NULL)
        return EINVAL;
    return 0;
#endif
}

static inline int libslock_barrierattr_destroy(libslock_barrierattr_t *attr)
{
#ifdef LOCKIF_BARRIERATTR_DESTROY
    return LOCKIF_BARRIERATTR_DESTROY(attr);
#else
    if (attr == NULL)
        return EINVAL;
    return 0;
#endif
}

static inline int libslock_barrierattr_getpshared(const libslock_barrierattr_t *attr, int *pshared)
{
#ifdef LOCKIF_BARRIERATTR_GETPSHARED
    return LOCKIF_BARRIERATTR_GETPSHARED(attr, pshared);
#else
    if (attr == NULL || pshared == NULL)
        return EINVAL;
    *pshared = *attr;
    return 0;
#endif
}

static inline int libslock_barrierattr_setpshared(libslock_barrierattr_t *attr, int pshared)
{
#ifdef LOCKIF_BARRIERATTR_SETPHARED
    return LOCKIF_BARRIERATTR_SETPHARED(attr, pshared);
#else
    if (attr == NULL)
        return EINVAL;
    // Currently we only support private barriers (the default)
    if (pshared != PTHREAD_PROCESS_PRIVATE)
    {
        fprintf(stderr, "Only private barriers are supported.\n");
        return EINVAL;
    }
    *attr = pshared;
    return 0;
#endif
}

#endif