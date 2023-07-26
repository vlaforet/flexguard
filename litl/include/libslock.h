/*
 * File: libslock.h
 * Authors: Hugo Guiroux <hugo.guiroux at gmail dot com>
 *          Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of libslock into LiTL
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Victor Laforet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __LIBSLOCK_H__
#define __LIBSLOCK_H__

#include "lock_if.h"

#include "padding.h"
#define LOCK_ALGORITHM "LIBSLOCK"
#define NEED_CONTEXT 1
#define SUPPORT_WAITING 0

typedef struct libslock_mutex_t {
#if COND_VAR
    pthread_mutex_t posix_lock;
    char __pad[pad_to_cache_line(sizeof(pthread_mutex_t))];
#endif
    lock_global_data lock __attribute__((aligned(CACHE_LINE_SIZE)));
} libslock_mutex_t __attribute__((aligned(CACHE_LINE_SIZE)));

typedef lock_condvar_t libslock_cond_t;
typedef lock_local_data libslock_context_t;

libslock_mutex_t *libslock_mutex_create(const pthread_mutexattr_t *attr);
int libslock_mutex_lock(libslock_mutex_t *impl, libslock_context_t *me);
int libslock_mutex_trylock(libslock_mutex_t *impl, libslock_context_t *me);
void libslock_mutex_unlock(libslock_mutex_t *impl, libslock_context_t *me);
int libslock_mutex_destroy(libslock_mutex_t *lock);
int libslock_cond_init(libslock_cond_t *cond, const pthread_condattr_t *attr);
int libslock_cond_timedwait(libslock_cond_t *cond, libslock_mutex_t *lock,
                            libslock_context_t *me, const struct timespec *ts);
int libslock_cond_wait(libslock_cond_t *cond, libslock_mutex_t *lock,
                       libslock_context_t *me);
int libslock_cond_signal(libslock_cond_t *cond);
int libslock_cond_broadcast(libslock_cond_t *cond);
int libslock_cond_destroy(libslock_cond_t *cond);
void libslock_thread_start(void);
void libslock_thread_exit(void);
void libslock_application_init(void);
void libslock_application_exit(void);
void libslock_init_context(libslock_mutex_t *impl, libslock_context_t *context,
                           int number);

typedef libslock_mutex_t lock_mutex_t;
typedef libslock_context_t lock_context_t;
typedef libslock_cond_t lock_cond_t;

#define lock_mutex_create libslock_mutex_create
#define lock_mutex_lock libslock_mutex_lock
#define lock_mutex_trylock libslock_mutex_trylock
#define lock_mutex_unlock libslock_mutex_unlock
#define lock_mutex_destroy libslock_mutex_destroy
#define lock_cond_init libslock_cond_init
#define lock_cond_timedwait libslock_cond_timedwait
#define lock_cond_wait libslock_cond_wait
#define lock_cond_signal libslock_cond_signal
#define lock_cond_broadcast libslock_cond_broadcast
#define lock_cond_destroy libslock_cond_destroy
#define lock_thread_start libslock_thread_start
#define lock_thread_exit libslock_thread_exit
#define lock_application_init libslock_application_init
#define lock_application_exit libslock_application_exit
#define lock_init_context libslock_init_context

#endif // __LIBSLOCK_H__
