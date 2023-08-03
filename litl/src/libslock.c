/*
 * File: libslock.c
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

#include <assert.h>
#include <errno.h>
#include <libslock.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "interpose.h"
#include "utils.h"
#include "waiting_policy.h"

extern __thread unsigned int cur_thread_id;

void try_to_initialize_me(libslock_mutex_t *impl, libslock_context_t *me) {
    if (!me->init) {
        init_lock_local(INT_MAX, &impl->lock, &me->me);
        me->init = 1;
    }
}

libslock_mutex_t *libslock_mutex_create(const pthread_mutexattr_t *attr) {
    libslock_mutex_t *impl =
        (libslock_mutex_t *)alloc_cache_align(sizeof(libslock_mutex_t));
    init_lock_global(&impl->lock);

    return impl;
}

int libslock_mutex_lock(libslock_mutex_t *impl, libslock_context_t *me) {
    try_to_initialize_me(impl, me);
    acquire_write(&me->me, &impl->lock);
    return 0;
}

int libslock_mutex_trylock(libslock_mutex_t *impl, libslock_context_t *me) {
    try_to_initialize_me(impl, me);
    if (acquire_trylock(&me->me, &impl->lock) == 0)
        return 0;

    return EBUSY;
}

void libslock_mutex_unlock(libslock_mutex_t *impl, libslock_context_t *me) {
    try_to_initialize_me(impl, me);
    release_write(&me->me, &impl->lock);
}

int libslock_mutex_destroy(libslock_mutex_t *impl) {
    free_lock_global(impl->lock);
    return 0;
}

int libslock_cond_init(libslock_cond_t *cond, const pthread_condattr_t *attr) {
#if COND_VAR
    return condvar_init(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_wait(libslock_cond_t *cond, libslock_mutex_t *impl,
                       libslock_context_t *me) {
    try_to_initialize_me(impl, me);
#if COND_VAR
    return condvar_wait(cond, &me->me, &impl->lock);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_timedwait(libslock_cond_t *cond, libslock_mutex_t *impl,
                            libslock_context_t *me, const struct timespec *ts) {
    try_to_initialize_me(impl, me);
#if COND_VAR
    return condvar_timedwait(cond, &me->me, &impl->lock, ts);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_signal(libslock_cond_t *cond) {
#if COND_VAR
    return condvar_signal(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_broadcast(libslock_cond_t *cond) {
#if COND_VAR
    return condvar_broadcast(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_destroy(libslock_cond_t *cond) {
#if COND_VAR
    return condvar_destroy(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

void libslock_thread_start(void) {
}

void libslock_thread_exit(void) {
}

void libslock_application_init(void) {
}

void libslock_application_exit(void) {
}

void libslock_init_context(lock_mutex_t *impl, lock_context_t *context,
                           int number) {
    for (int i = 0; i < number; i++)
        context[i].init = 0;
}
