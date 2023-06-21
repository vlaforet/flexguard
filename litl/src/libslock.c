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

libslock_mutex_t *libslock_mutex_create(const pthread_mutexattr_t *attr) {
    libslock_mutex_t *impl =
        (libslock_mutex_t *)alloc_cache_align(sizeof(libslock_mutex_t));
    init_lock_global(&impl->lock);
    printf("CREATE impl:%p\n", (void *)impl);

#if COND_VAR
    REAL(pthread_mutex_init)
    (&impl->posix_lock, attr);
#endif

    return impl;
}

int libslock_mutex_lock(libslock_mutex_t *impl, libslock_context_t *me) {
    printf("[%d] Locking lock me:%p impl:%p\n", cur_thread_id, (void *)me,
           (void *)&impl->lock);
    acquire_write(me, &impl->lock);
    printf("[%d] Locking lock OUT me:%p impl:%p\n", cur_thread_id, (void *)me,
           (void *)&impl->lock);

#if COND_VAR
    int ret = REAL(pthread_mutex_lock)(&impl->posix_lock);

    assert(ret == 0);
#endif
    return 0;
}

int libslock_mutex_trylock(libslock_mutex_t *impl, libslock_context_t *me) {
    printf("[%d] TRYLOCK!!! me:%p impl:%p\n", cur_thread_id, (void *)me,
           (void *)impl);
    if (acquire_trylock(me, &impl->lock) == 0) {
#if COND_VAR
        int ret = 0;
        while ((ret = REAL(pthread_mutex_trylock)(&impl->posix_lock)) == EBUSY)
            CPU_PAUSE();

        assert(ret == 0);
#endif
        return 0;
    }

    return EBUSY;
}

void __libslock_mutex_unlock(libslock_mutex_t *impl, libslock_context_t *me) {
    printf("[%d] Unlocking lock me:%p impl:%p\n", cur_thread_id, (void *)me,
           (void *)&impl->lock);
    release_write(me, &impl->lock);
    printf("[%d] Unlocking lock OUT me:%p impl:%p\n", cur_thread_id, (void *)me,
           (void *)&impl->lock);
}

void libslock_mutex_unlock(libslock_mutex_t *impl, libslock_context_t *me) {
#if COND_VAR
    int ret = REAL(pthread_mutex_unlock)(&impl->posix_lock);
    assert(ret == 0);
#endif
    __libslock_mutex_unlock(impl, me);
}

int libslock_mutex_destroy(libslock_mutex_t *impl) {
#if COND_VAR
    REAL(pthread_mutex_destroy)
    (&impl->posix_lock);
#endif

    free_lock_global(impl->lock);
    return 0;
}

int libslock_cond_init(libslock_cond_t *cond, const pthread_condattr_t *attr) {
#if COND_VAR
    return REAL(pthread_cond_init)(cond, attr);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_timedwait(libslock_cond_t *cond, libslock_mutex_t *lock,
                            libslock_context_t *me, const struct timespec *ts) {
#if COND_VAR
    int res;

    __libslock_mutex_unlock(lock, me);

    if (ts)
        res = REAL(pthread_cond_timedwait)(cond, &lock->posix_lock, ts);
    else
        res = REAL(pthread_cond_wait)(cond, &lock->posix_lock);

    if (res != 0 && res != ETIMEDOUT) {
        fprintf(stderr, "Error on cond_{timed,}wait %d\n", res);
        assert(0);
    }

    int ret = 0;
    if ((ret = REAL(pthread_mutex_unlock)(&lock->posix_lock)) != 0) {
        fprintf(stderr, "Error on mutex_unlock %d\n", ret == EPERM);
        assert(0);
    }

    libslock_mutex_lock(lock, me);

    return res;
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_wait(libslock_cond_t *cond, libslock_mutex_t *lock,
                       libslock_context_t *me) {
    return libslock_cond_timedwait(cond, lock, me, 0);
}

int libslock_cond_signal(libslock_cond_t *cond) {
#if COND_VAR
    return REAL(pthread_cond_signal)(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_broadcast(libslock_cond_t *cond) {
#if COND_VAR
    return REAL(pthread_cond_broadcast)(cond);
#else
    fprintf(stderr, "Error cond_var not supported.");
    assert(0);
#endif
}

int libslock_cond_destroy(libslock_cond_t *cond) {
#if COND_VAR
    return REAL(pthread_cond_destroy)(cond);
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
                           int UNUSED(number)) {
    init_lock_local(INT_MAX, &impl->lock, context);
    printf("[%d] Init context me:%p impl:%p\n", cur_thread_id, (void *)context,
           (void *)impl);
}
