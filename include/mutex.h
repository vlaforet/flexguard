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
typedef pthread_cond_t mutex_cond_t;

/*
 * Declarations
 */
int mutex_init(mutex_lock_t *the_lock);
void mutex_destroy(mutex_lock_t *the_lock);
void mutex_lock(mutex_lock_t *the_lock);
int mutex_trylock(mutex_lock_t *the_lock);
void mutex_unlock(mutex_lock_t *the_lock);

int mutex_cond_init(mutex_cond_t *cond);
int mutex_cond_wait(mutex_cond_t *cond, mutex_lock_t *the_lock);
int mutex_cond_timedwait(mutex_cond_t *cond, mutex_lock_t *the_lock, const struct timespec *ts);
int mutex_cond_signal(mutex_cond_t *cond);
int mutex_cond_broadcast(mutex_cond_t *cond);
int mutex_cond_destroy(mutex_cond_t *cond);

/*
 * lock_if.h bindings
 */

#define LOCKIF_LOCK_T mutex_lock_t
#define LOCKIF_INIT mutex_init
#define LOCKIF_DESTROY mutex_destroy
#define LOCKIF_LOCK mutex_lock
#define LOCKIF_TRYLOCK mutex_trylock
#define LOCKIF_UNLOCK mutex_unlock
#define LOCKIF_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#define LOCKIF_COND_T mutex_cond_t
#define LOCKIF_COND_INIT mutex_cond_init
#define LOCKIF_COND_DESTROY mutex_cond_destroy
#define LOCKIF_COND_WAIT mutex_cond_wait
#define LOCKIF_COND_TIMEDWAIT mutex_cond_timedwait
#define LOCKIF_COND_SIGNAL mutex_cond_signal
#define LOCKIF_COND_BROADCAST mutex_cond_broadcast
#define LOCKIF_COND_INITIALIZER PTHREAD_COND_INITIALIZER

#endif
