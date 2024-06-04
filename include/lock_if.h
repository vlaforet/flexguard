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
#elif defined(USE_SPINLOCK_LOCKS)
#include "spinlock.h"
#elif defined(USE_HYBRIDLOCK_LOCKS)
#include "hybridlock.h"
#elif defined(USE_TICKET_LOCKS)
#include "ticket.h"
#elif defined(USE_MUTEX_LOCKS)
#include "mutex.h"
#elif defined(USE_FUTEX_LOCKS)
#include "futex.h"
#elif defined(USE_CLH_LOCKS)
#include "clh.h"
#else
#error "No type of locks given"
#endif

#if LOCAL_NEEDED == 1
#ifdef LOCAL_DATA_T
typedef LOCAL_DATA_T lock_local_data;
#else
#error "LOCAL_DATA_T not defined"
#endif
#endif

#ifdef GLOBAL_DATA_T
typedef struct libslock_t
{
    GLOBAL_DATA_T global;

#if LOCAL_NEEDED == 1
    lock_local_data *local[MAX_NUMBER_THREADS];
#endif
} libslock_t;
#else
#error "GLOBAL_DATA_T not defined"
#endif

#if LOCAL_NEEDED == 1
static __thread int libslock_thread_id = -1;
static volatile int libslock_thread_count = 0;
static lock_local_data *get_local(libslock_t *lock)
{
    if (libslock_thread_id < 0)
    {
        libslock_thread_id = __sync_fetch_and_add(&libslock_thread_count, 1);
        CHECK_NUMBER_THREADS_FATAL(libslock_thread_id);
    }

    if (lock->local[libslock_thread_id] == NULL)
    {
        lock->local[libslock_thread_id] = (lock_local_data *)malloc(sizeof(lock_local_data));
        INIT_LOCAL_DATA(&lock->global, lock->local[libslock_thread_id]);
    }
    return lock->local[libslock_thread_id];
}
#endif

#ifdef LOCK_GLOBAL_INITIALIZER
#define LIBSLOCK_INITIALIZER LOCK_GLOBAL_INITIALIZER
#else
#define LIBSLOCK_INITIALIZER \
    {                        \
    }
#endif

#ifdef COND_INITIALIZER
#define LIBSLOCK_COND_INITIALIZER COND_INITIALIZER
#else
#define LIBSLOCK_COND_INITIALIZER \
    {                             \
    }
#endif

/*
 *  Declarations
 */

// lock acquire/release operations
static inline void libslock_lock(libslock_t *lock);
static inline int libslock_trylock(libslock_t *lock);
static inline void libslock_unlock(libslock_t *lock);

// initialization of a lock
static inline int libslock_init(libslock_t *lock);
static inline void libslock_destroy(libslock_t *lock);

/*
 *  Lock Functions
 */

static inline void libslock_lock(libslock_t *lock)
{
#if LOCAL_NEEDED == 1
    lock_local_data *local_d = get_local(lock);
    ACQUIRE_LOCK(&lock->global, local_d);
#else
    ACQUIRE_LOCK(&lock->global);
#endif
}

static inline int libslock_trylock(libslock_t *lock)
{
#ifdef ACQUIRE_TRYLOCK
#if LOCAL_NEEDED == 1
    lock_local_data *local_d = get_local(lock);
    return ACQUIRE_TRYLOCK(&lock->global, local_d);
#else
    return ACQUIRE_TRYLOCK(&lock->global);
#endif
#else
    return 1;
#endif
}

static inline void libslock_unlock(libslock_t *lock)
{
#if LOCAL_NEEDED == 1
    lock_local_data *local_d = get_local(lock);
    RELEASE_LOCK(&lock->global, local_d);
#else
    RELEASE_LOCK(&lock->global);
#endif
}

static inline int libslock_init(libslock_t *lock)
{
    return INIT_GLOBAL_DATA(&lock->global);
}

static inline void libslock_destroy(libslock_t *lock)
{
    DESTROY_GLOBAL_DATA(&lock->global);
}

/*
 *  Condition Variables Functions
 */

#ifdef CONDVAR_DATA_T
typedef CONDVAR_DATA_T libslock_cond_t;
#else
typedef int libslock_cond_t;
#endif

static inline int libslock_cond_init(libslock_cond_t *cond)
{
#ifdef COND_INIT
    return COND_INIT(cond);
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_wait(libslock_cond_t *cond, libslock_t *lock)
{
#ifdef COND_WAIT
#if LOCAL_NEEDED == 1
    lock_local_data *local_d = get_local(lock);
    return COND_WAIT(cond, &lock->global, local_d);
#else
    return COND_WAIT(cond, &lock->global);
#endif
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_timedwait(libslock_cond_t *cond, libslock_t *lock, const struct timespec *ts)
{
#ifdef COND_TIMEDWAIT
#if LOCAL_NEEDED == 1
    lock_local_data *local_d = get_local(lock);
    return COND_TIMEDWAIT(cond, &lock->global, local_d, ts);
#else
    return COND_TIMEDWAIT(cond, &lock->global, ts);
#endif
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_signal(libslock_cond_t *cond)
{
#ifdef COND_SIGNAL
    return COND_SIGNAL(cond);
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_broadcast(libslock_cond_t *cond)
{
#ifdef COND_BROADCAST
    return COND_BROADCAST(cond);
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

static inline int libslock_cond_destroy(libslock_cond_t *cond)
{
#ifdef COND_DESTROY
    return COND_DESTROY(cond);
#else
    fprintf(stderr, "condvar not supported by this lock.\n");
    exit(EXIT_FAILURE);
#endif
}

#endif