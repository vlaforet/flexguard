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
int mutex_trylock(mutex_lock_t *the_locks);
void mutex_unlock(mutex_lock_t *the_locks);

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

#endif
