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

litllibslock_mutex_t *
litllibslock_mutex_create(const pthread_mutexattr_t *attr) {
    litllibslock_mutex_t *impl =
        (litllibslock_mutex_t *)alloc_cache_align(sizeof(litllibslock_mutex_t));
    libslock_init(&impl->lock);

#if COND_VAR && !LIBSLOCK_COND_VAR
    REAL(pthread_mutex_init)(&impl->posix_lock, attr);
#endif

    return impl;
}

int litllibslock_mutex_lock(litllibslock_mutex_t *impl,
                            litllibslock_context_t *me) {
    libslock_lock(&impl->lock);

#if COND_VAR && !LIBSLOCK_COND_VAR
    assert(REAL(pthread_mutex_lock)(&impl->posix_lock) == 0);
#endif
    return 0;
}

int litllibslock_mutex_trylock(litllibslock_mutex_t *impl,
                               litllibslock_context_t *me) {
    if (libslock_trylock(&impl->lock) == 0) {
#if COND_VAR && !LIBSLOCK_COND_VAR
        assert(REAL(pthread_mutex_lock)(&impl->posix_lock) == 0);
#endif

        return 0;
    }

    return EBUSY;
}

void litllibslock_mutex_unlock(litllibslock_mutex_t *impl,
                               litllibslock_context_t *me) {
#if COND_VAR && !LIBSLOCK_COND_VAR
    assert(REAL(pthread_mutex_unlock)(&impl->posix_lock) == 0);
#endif

    libslock_unlock(&impl->lock);
}

int litllibslock_mutex_destroy(litllibslock_mutex_t *impl) {
    libslock_destroy(&impl->lock);

#if COND_VAR && !LIBSLOCK_COND_VAR
    REAL(pthread_mutex_destroy)(&impl->posix_lock);
#endif

    return 0;
}

int litllibslock_cond_init(litllibslock_cond_t *cond,
                           const pthread_condattr_t *attr) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_init(cond);
#else
    return REAL(pthread_cond_init)(cond, attr);
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int litllibslock_cond_wait(litllibslock_cond_t *cond,
                           litllibslock_mutex_t *impl,
                           litllibslock_context_t *me) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_wait(cond, &impl->lock);
#else
    libslock_unlock(&impl->lock);
    int res = REAL(pthread_cond_wait)(cond, &impl->posix_lock);
    libslock_lock(&impl->lock);
    return res;
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int litllibslock_cond_timedwait(litllibslock_cond_t *cond,
                                litllibslock_mutex_t *impl,
                                litllibslock_context_t *me,
                                const struct timespec *ts) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_timedwait(cond, &impl->lock, ts);
#else
    libslock_unlock(&impl->lock);
    int res = REAL(pthread_cond_timedwait)(cond, &impl->posix_lock, ts);
    libslock_lock(&impl->lock);
    return res;
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int litllibslock_cond_signal(litllibslock_cond_t *cond) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_signal(cond);
#else
    return REAL(pthread_cond_signal)(cond);
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int litllibslock_cond_broadcast(litllibslock_cond_t *cond) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_broadcast(cond);
#else
    return REAL(pthread_cond_broadcast)(cond);
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int litllibslock_cond_destroy(litllibslock_cond_t *cond) {
#if COND_VAR
#if LIBSLOCK_COND_VAR
    return libslock_cond_destroy(cond);
#else
    return REAL(pthread_cond_destroy)(cond);
#endif
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

void litllibslock_thread_start(void) {
}

void litllibslock_thread_exit(void) {
}

void litllibslock_application_init(void) {
}

void litllibslock_application_exit(void) {
}
