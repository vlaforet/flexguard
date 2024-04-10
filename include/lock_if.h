/*
 * File: lock_if.h
 * Authors: Tudor David <tudor.david@epfl.ch>, Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Common interface to various locking algorithms;
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

#ifdef USE_MCS_LOCKS
#include "mcs.h"
#elif defined(USE_SPINLOCK_LOCKS)
#include "spinlock.h"
#elif defined(USE_HYBRIDLOCK_LOCKS)
#include "hybridlock.h"
#elif defined(USE_HYBRIDSPIN_LOCKS)
#include "hybridspin.h"
#elif defined(USE_TICKET_LOCKS)
#include "ticket.h"
#elif defined(USE_MUTEX_LOCKS)
#include <pthread.h>
#include "utils.h"
#elif defined(USE_FUTEX_LOCKS)
#include "futex.h"
#elif defined(USE_CLH_LOCKS)
#include "clh.h"
#else
#error "No type of locks given"
#endif

// lock globals
#ifdef USE_MCS_LOCKS
typedef mcs_global_params lock_global_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef spinlock_lock_t lock_global_data;
#elif defined(USE_HYBRIDLOCK_LOCKS)
typedef hybridlock_lock_t lock_global_data;
#elif defined(USE_HYBRIDSPIN_LOCKS)
typedef hybridspin_lock_t lock_global_data;
#elif defined(USE_TICKET_LOCKS)
typedef ticketlock_t lock_global_data;
#elif defined(USE_MUTEX_LOCKS)
typedef pthread_mutex_t lock_global_data;
#elif defined(USE_FUTEX_LOCKS)
typedef futex_lock_t lock_global_data;
#elif defined(USE_CLH_LOCKS)
typedef clh_lock_t lock_global_data;
#endif

typedef lock_global_data *global_data;

// typedefs for thread local data
#ifdef USE_MCS_LOCKS
typedef mcs_local_params lock_local_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef unsigned int lock_local_data;
#elif defined(USE_HYBRIDLOCK_LOCKS)
typedef hybridlock_local_params_t lock_local_data;
#elif defined(USE_HYBRIDSPIN_LOCKS)
typedef void *lock_local_data; // no local data for hybridspin
#elif defined(USE_TICKET_LOCKS)
typedef void *lock_local_data; // no local data for ticket locks
#elif defined(USE_MUTEX_LOCKS)
typedef void *lock_local_data; // no local data for mutexes
#elif defined(USE_FUTEX_LOCKS)
typedef void *lock_local_data; // no local data for futexes
#elif defined(USE_CLH_LOCKS)
typedef clh_local_params_t lock_local_data;
#endif

typedef lock_local_data *local_data;

/*
 *  Declarations
 */

// lock acquire operation
static inline void acquire_lock(lock_local_data *local_d, lock_global_data *global_d);
static inline int acquire_trylock(lock_local_data *local_d, lock_global_data *global_d);

// lock release operation
static inline void release_lock(lock_local_data *local_d, lock_global_data *global_d);
static inline void release_trylock(lock_local_data *local_d, lock_global_data *global_d);

// initialization of local data for a lock.
static inline int init_lock_local(lock_global_data *the_locks, lock_local_data *the_data);

// initialization of global data for a lock
static inline int init_lock_global(lock_global_data *the_locks);

// removal of global data for a lock
static inline void free_lock_global(lock_global_data the_lock);

// removal of local data for a lock
static inline void free_lock_local(lock_local_data local_d);

/*
 *  Functions
 */
static inline void acquire_lock(lock_local_data *local_d, lock_global_data *global_d)
{
#ifdef USE_MCS_LOCKS
    mcs_acquire(global_d->the_lock, *local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(global_d, local_d);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    hybridlock_lock(global_d, local_d);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    hybridspin_lock(global_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(global_d);
#elif defined(USE_FUTEX_LOCKS)
    futex_lock(global_d);
#elif defined(USE_CLH_LOCKS)
    clh_lock(global_d, local_d);
#endif
}

static inline void release_lock(lock_local_data *local_d, lock_global_data *global_d)
{
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock, *local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    hybridlock_unlock(global_d, local_d);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    hybridspin_unlock(global_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_FUTEX_LOCKS)
    futex_unlock(global_d);
#elif defined(USE_CLH_LOCKS)
    clh_unlock(global_d, local_d);
#endif
}

static inline int init_lock_local(lock_global_data *the_lock, lock_local_data *local_data)
{
#ifdef USE_MCS_LOCKS
    return init_mcs_local(local_data);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_local(local_data);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    return init_hybridlock_local(local_data, the_lock);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    return 0;
#elif defined(USE_TICKET_LOCKS)
    return 0;
#elif defined(USE_MUTEX_LOCKS)
    return 0;
#elif defined(USE_FUTEX_LOCKS)
    return 0;
#elif defined(USE_CLH_LOCKS)
    return init_clh_local(local_data, the_lock);
#endif
}

static inline void free_lock_local(lock_local_data local_d)
{
#ifdef USE_MCS_LOCKS
    end_mcs_local(local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    //    end_spinlock_local(local_d);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    // end_hybridlock_local(local_d);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    // end_hybridspin_local(local_d);
#elif defined(USE_TICKET_LOCKS)
    // nothing to be done
#elif defined(USE_MUTEX_LOCKS)
    // nothing to be done
#elif defined(USE_FUTEX_LOCKS)
    // nothing to be done
#endif
}

static inline int init_lock_global(lock_global_data *the_lock)
{
#ifdef USE_MCS_LOCKS
    return init_mcs_global(the_lock);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_global(the_lock);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    return init_hybridlock_global(the_lock);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    return init_hybridspin_global(the_lock);
#elif defined(USE_TICKET_LOCKS)
    return create_ticketlock(the_lock);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_init(the_lock, NULL);
    return 0;
#elif defined(USE_FUTEX_LOCKS)
    return init_futex_global(the_lock);
#elif defined(USE_CLH_LOCKS)
    return init_clh_global(the_lock);
#endif
}

static inline int init_lock_global_nt(int num_threads, lock_global_data *the_lock)
{
    return init_lock_global(the_lock);
}

static inline void free_lock_global(lock_global_data the_lock)
{
#ifdef USE_MCS_LOCKS
    end_mcs_global(the_lock);
#elif defined(USE_SPINLOCK_LOCKS)
    end_spinlock_global(the_lock);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    // end_hybridlock_global(the_lock);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    end_hybridspin_global(the_lock);
#elif defined(USE_TICKET_LOCKS)
    // free_ticketlocks(the_lock);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_destroy(&the_lock);
#elif defined(USE_FUTEX_LOCKS)
    // nothing to be done
#endif
}

// checks whether the lock is free; if it is, acquire it;
// we use this in memcached to simulate trylocks
// return 0 on success, 1 otherwise
static inline int acquire_trylock(lock_local_data *local_d, lock_global_data *global_d)
{
#ifdef USE_MCS_LOCKS
    return mcs_trylock(global_d->the_lock, *local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    return spinlock_trylock(global_d, local_d);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    return 1; // Unsupported
#elif defined(USE_HYBRIDSPIN_LOCKS)
    return hybridspin_trylock(global_d);
#elif defined(USE_TICKET_LOCKS)
    return ticket_trylock(global_d);
#elif defined(USE_MUTEX_LOCKS)
    return pthread_mutex_trylock(global_d);
#elif defined(USE_FUTEX_LOCKS)
    return futex_trylock(global_d);
#elif defined(USE_CLH_LOCKS)
    return clh_trylock(global_d, local_d);
#endif
}

static inline void release_trylock(lock_local_data *local_d, lock_global_data *global_d)
{
#ifdef USE_MCS_LOCKS
    mcs_release(global_d->the_lock, *local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(global_d);
#elif defined(USE_HYBRIDLOCK_LOCKS)
    hybridlock_unlock(global_d, local_d);
#elif defined(USE_HYBRIDSPIN_LOCKS)
    hybridspin_unlock(global_d);
#elif defined(USE_TICKET_LOCKS)
    ticket_release(global_d);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(global_d);
#elif defined(USE_FUTEX_LOCKS)
    futex_unlock(global_d);
#elif defined(USE_CLH_LOCKS)
    clh_unlock(global_d, local_d);
#endif
}

/* Condition Variables */
#ifdef USE_HYBRIDLOCK_LOCKS
typedef hybridlock_condvar_t lock_condvar_t;
#elif defined(USE_MCS_LOCKS)
typedef mcs_condvar_t lock_condvar_t;
#elif defined(USE_FUTEX_LOCKS)
typedef futex_condvar_t lock_condvar_t;
#else
typedef int lock_condvar_t;
#endif

static inline int condvar_init(lock_condvar_t *cond)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_init(cond);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_init(cond);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_init(cond);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}

static inline int condvar_wait(lock_condvar_t *cond, lock_local_data *local_d, lock_global_data *global_d)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_wait(cond, local_d, global_d);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_wait(cond, *local_d, global_d->the_lock);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_wait(cond, global_d);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}

static inline int condvar_timedwait(lock_condvar_t *cond, lock_local_data *local_d, lock_global_data *global_d, const struct timespec *ts)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_timedwait(cond, local_d, global_d, ts);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_timedwait(cond, *local_d, global_d->the_lock, ts);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_timedwait(cond, global_d, ts);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}

static inline int condvar_signal(lock_condvar_t *cond)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_signal(cond);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_signal(cond);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_signal(cond);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}

static inline int condvar_broadcast(lock_condvar_t *cond)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_broadcast(cond);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_broadcast(cond);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_broadcast(cond);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}

static inline int condvar_destroy(lock_condvar_t *cond)
{
#ifdef USE_HYBRIDLOCK_LOCKS
    return hybridlock_condvar_destroy(cond);
#elif defined(USE_MCS_LOCKS)
    return mcs_condvar_destroy(cond);
#elif defined(USE_FUTEX_LOCKS)
    return futex_condvar_destroy(cond);
#else
    perror("condvar not supported by this lock.");
    return 1;
#endif
}