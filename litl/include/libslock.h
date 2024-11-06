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
#define NEED_CONTEXT 0
#define SUPPORT_WAITING 0
#define LIBSLOCK_COND_VAR 0

typedef struct litllibslock_mutex_t {
#if COND_VAR && !LIBSLOCK_COND_VAR
    pthread_mutex_t posix_lock;
    char __pad[pad_to_cache_line(sizeof(pthread_mutex_t))];
#endif
    libslock_t lock __attribute__((aligned(CACHE_LINE_SIZE)));
} litllibslock_mutex_t __attribute__((aligned(CACHE_LINE_SIZE)));

typedef void *litllibslock_context_t; // Unused, take the less space as possible

#if LIBSLOCK_COND_VAR
typedef libslock_cond_t litllibslock_cond_t;
#else
typedef pthread_cond_t litllibslock_cond_t;
#endif

litllibslock_mutex_t *
litllibslock_mutex_create(const pthread_mutexattr_t *attr);
int litllibslock_mutex_lock(litllibslock_mutex_t *impl,
                            litllibslock_context_t *me);
int litllibslock_mutex_trylock(litllibslock_mutex_t *impl,
                                   litllibslock_context_t *me);
void litllibslock_mutex_unlock(litllibslock_mutex_t *impl,
                               litllibslock_context_t *me);
int litllibslock_mutex_destroy(litllibslock_mutex_t *lock);
int litllibslock_cond_init(litllibslock_cond_t *cond,
                           const pthread_condattr_t *attr);
int litllibslock_cond_timedwait(litllibslock_cond_t *cond,
                                litllibslock_mutex_t *lock,
                                litllibslock_context_t *me,
                                const struct timespec *ts);
int litllibslock_cond_wait(litllibslock_cond_t *cond,
                           litllibslock_mutex_t *lock,
                           litllibslock_context_t *me);
int litllibslock_cond_signal(litllibslock_cond_t *cond);
int litllibslock_cond_broadcast(litllibslock_cond_t *cond);
int litllibslock_cond_destroy(litllibslock_cond_t *cond);
void litllibslock_thread_start(void);
void litllibslock_thread_exit(void);
void litllibslock_application_init(void);
void litllibslock_application_exit(void);

typedef litllibslock_mutex_t lock_mutex_t;
typedef litllibslock_context_t lock_context_t;
typedef litllibslock_cond_t lock_cond_t;

#define lock_mutex_create litllibslock_mutex_create
#define lock_mutex_lock litllibslock_mutex_lock
#define lock_mutex_trylock litllibslock_mutex_trylock
#define lock_mutex_unlock litllibslock_mutex_unlock
#define lock_mutex_destroy litllibslock_mutex_destroy
#define lock_cond_init litllibslock_cond_init
#define lock_cond_timedwait litllibslock_cond_timedwait
#define lock_cond_wait litllibslock_cond_wait
#define lock_cond_signal litllibslock_cond_signal
#define lock_cond_broadcast litllibslock_cond_broadcast
#define lock_cond_destroy litllibslock_cond_destroy
#define lock_thread_start litllibslock_thread_start
#define lock_thread_exit litllibslock_thread_exit
#define lock_application_init litllibslock_application_init
#define lock_application_exit litllibslock_application_exit

#endif // __LIBSLOCK_H__
