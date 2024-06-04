/*
 * File: mutex.h
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Using pthread_mutex in libslock
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

#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <pthread.h>
#include "utils.h"

typedef pthread_mutex_t mutex_lock_t;
typedef pthread_cond_t mutex_condvar_t;

/*
 * Lock manipulation methods
 */
void mutex_lock(mutex_lock_t *the_lock);
int mutex_trylock(mutex_lock_t *the_lock);
void mutex_unlock(mutex_lock_t *the_lock);

/*
 * Methods for single lock manipulation
 */
int init_mutex_global(mutex_lock_t *the_lock);
void end_mutex_global(mutex_lock_t *the_lock);

/*
 *  Condition Variables
 */
int mutex_condvar_init(mutex_condvar_t *cond);
int mutex_condvar_wait(mutex_condvar_t *cond, mutex_lock_t *the_lock);
int mutex_condvar_timedwait(mutex_condvar_t *cond, mutex_lock_t *the_lock, const struct timespec *ts);
int mutex_condvar_signal(mutex_condvar_t *cond);
int mutex_condvar_broadcast(mutex_condvar_t *cond);
int mutex_condvar_destroy(mutex_condvar_t *cond);

#define LOCAL_NEEDED 0

#define GLOBAL_DATA_T mutex_lock_t
#define CONDVAR_DATA_T mutex_condvar_t

#define INIT_GLOBAL_DATA init_mutex_global
#define DESTROY_GLOBAL_DATA end_mutex_global

#define ACQUIRE_LOCK mutex_lock
#define RELEASE_LOCK mutex_unlock
#define ACQUIRE_TRYLOCK mutex_trylock

#define COND_INIT mutex_condvar_init
#define COND_WAIT mutex_condvar_wait
#define COND_TIMEDWAIT mutex_condvar_timedwait
#define COND_SIGNAL mutex_condvar_signal
#define COND_BROADCAST mutex_condvar_broadcast
#define COND_DESTROY mutex_condvar_destroy

#define LOCK_GLOBAL_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define COND_INITIALIZER PTHREAD_COND_INITIALIZER

#endif
