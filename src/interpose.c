/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Victor Laforet <victor.laforet@inria.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of his software and associated documentation files (the "Software"), to deal
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "interpose.h"

#include <assert.h>
#include <atomic_ops.h>
#include <dlfcn.h>
#include <signal.h>

#if USE_REAL_PTHREAD == 1
int (*REAL(pthread_mutex_init))(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_mutex_destroy))(pthread_mutex_t *mutex) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_mutex_lock))(pthread_mutex_t *mutex) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_mutex_timedlock))(pthread_mutex_t *mutex, const struct timespec *abstime) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_mutex_trylock))(pthread_mutex_t *mutex) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_mutex_unlock))(pthread_mutex_t *mutex) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_init))(pthread_cond_t *cond, const pthread_condattr_t *attr) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_destroy))(pthread_cond_t *cond) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_timedwait))(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_wait))(pthread_cond_t *cond, pthread_mutex_t *mutex) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_signal))(pthread_cond_t *cond) __attribute__((aligned(CACHE_LINE_SIZE)));
int (*REAL(pthread_cond_broadcast))(pthread_cond_t *cond) __attribute__((aligned(CACHE_LINE_SIZE)));
#endif

#if TEST_INTERPOSE == 1
int test_interpose_counter = 42;
#define TEST_INTERPOSITION() \
  return test_interpose_counter++;
#else
#define TEST_INTERPOSITION()
#endif

static void __attribute__((constructor)) REAL(interpose_init)(void)
{
  static volatile uint8_t init_lock = 0;
  if (exactly_once(&init_lock) != 0)
    return;

#if USE_REAL_PTHREAD == 1
  LOAD_FUNC(pthread_mutex_lock, 1);
  LOAD_FUNC(pthread_mutex_trylock, 1);
  LOAD_FUNC(pthread_mutex_unlock, 1);
  LOAD_FUNC(pthread_mutex_init, 1);
  LOAD_FUNC(pthread_mutex_destroy, 1);
  LOAD_FUNC(pthread_cond_timedwait, 1);
  LOAD_FUNC(pthread_cond_wait, 1);
  LOAD_FUNC(pthread_cond_broadcast, 1);
  LOAD_FUNC(pthread_cond_destroy, 1);
  LOAD_FUNC(pthread_cond_init, 1);
  LOAD_FUNC(pthread_cond_signal, 1);
#endif

  __sync_synchronize();
  init_lock = 2;
}

static void __attribute__((destructor)) REAL(interpose_exit)(void)
{
}

/*
 * Lock functions
 */
static int interpose_lock_init(void *raw_lock, bool force)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (force)
    lock->status = 0;

  if (exactly_once(&lock->status) != 0)
    return 0;

  lock->lock = (libslock_t *)malloc((sizeof(libslock_t)));

  int res = libslock_init(lock->lock);
  lock->status = 2;
  return res;
}

static int interpose_lock_destroy(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);
  if (LIKELY(lock->status == 2))
  {
    libslock_destroy(lock->lock);
    free(lock->lock);
    lock->status = 0;
  }

  return 0;
}

static int interpose_lock_lock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  libslock_lock(lock->lock);
  return 0;
}

static int interpose_lock_trylock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  return libslock_trylock(lock->lock);
}

static int interpose_lock_unlock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  libslock_unlock(lock->lock);
  return 0;
}

static int interpose_cond_init(void *raw_cond, bool force)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);

  if (force)
    cond->status = 0;

  if (exactly_once(&cond->status) != 0)
    return 0;

  cond->cond = (libslock_cond_t *)malloc((sizeof(libslock_cond_t)));

  int res = libslock_cond_init(cond->cond);
  cond->status = 2;
  return res;
}

static int interpose_cond_destroy(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);

  if (LIKELY(cond->status == 2))
    return libslock_cond_destroy(cond->cond);
  return 0;
}

static int interpose_cond_timedwait(void *raw_cond, void *raw_lock, const struct timespec *abstime)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  lock_as_t *lock = CAST_TO_LOCK(raw_lock);
  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  return libslock_cond_timedwait(cond->cond, lock->lock, abstime);
}

static int interpose_cond_wait(void *raw_cond, void *raw_lock)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  lock_as_t *lock = CAST_TO_LOCK(raw_lock);
  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  return libslock_cond_wait(cond->cond, lock->lock);
}

static int interpose_cond_signal(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  return libslock_cond_signal(cond->cond);
}

static int interpose_cond_broadcast(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  return libslock_cond_broadcast(cond->cond);
}

static int interpose_barrier_init(void *raw_barrier, void *attr, int count)
{
  barrier_as_t *barrier = (barrier_as_t *)raw_barrier;

  if (exactly_once(&barrier->status) != 0)
    return 0;

  barrier->barrier = (libslock_barrier_t *)malloc((sizeof(libslock_barrier_t)));

  int res = libslock_barrier_init(barrier->barrier, (libslock_barrierattr_t *)attr, count);
  barrier->status = 2;
  return res;
}

static int interpose_barrier_destroy(void *raw_barrier)
{
  barrier_as_t *barrier = (barrier_as_t *)raw_barrier;

  if (LIKELY(barrier->status == 2))
  {
    libslock_barrier_destroy(barrier->barrier);
    free(barrier->barrier);
    barrier->status = 0;
  }

  return 0;
}

static int interpose_barrier_wait(void *raw_barrier)
{
  barrier_as_t *barrier = (barrier_as_t *)raw_barrier;

  if (UNLIKELY(barrier->status != 2))
    return EINVAL;

  return libslock_barrier_wait(barrier->barrier);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  TEST_INTERPOSITION();
  DASSERT(sizeof(pthread_mutex_t) > sizeof(lock_as_t));
  return interpose_lock_init(mutex, true);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  TEST_INTERPOSITION();
  return interpose_lock_destroy(mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  TEST_INTERPOSITION();
  return interpose_lock_lock(mutex);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
  TEST_INTERPOSITION();
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  TEST_INTERPOSITION();
  return interpose_lock_trylock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  TEST_INTERPOSITION();
  return interpose_lock_unlock(mutex);
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  TEST_INTERPOSITION();
  DASSERT(sizeof(pthread_cond_t) > sizeof(condvar_as_t));
  return interpose_cond_init(cond, true);
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
  TEST_INTERPOSITION();
  return interpose_cond_destroy(cond);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
  TEST_INTERPOSITION();
  return interpose_cond_timedwait(cond, mutex, abstime);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  TEST_INTERPOSITION();
  return interpose_cond_wait(cond, mutex);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
  TEST_INTERPOSITION();
  return interpose_cond_signal(cond);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
  TEST_INTERPOSITION();
  return interpose_cond_broadcast(cond);
}

// Spinlocks
#if INTERPOSE_SPINLOCK
int pthread_spin_init(pthread_spinlock_t *spin, int pshared)
{
  TEST_INTERPOSITION();
  DASSERT(sizeof(pthread_spinlock_t) > sizeof(lock_as_t));
  return interpose_lock_init((void *)spin, true);
}

int pthread_spin_destroy(pthread_spinlock_t *spin)
{
  TEST_INTERPOSITION();
  return interpose_lock_destroy((void *)spin);
}

int pthread_spin_lock(pthread_spinlock_t *spin)
{
  TEST_INTERPOSITION();
  return interpose_lock_lock((void *)spin);
}

int pthread_spin_trylock(pthread_spinlock_t *spin)
{
  TEST_INTERPOSITION();
  return interpose_lock_trylock((void *)spin);
}

int pthread_spin_unlock(pthread_spinlock_t *spin)
{
  TEST_INTERPOSITION();
  return interpose_lock_unlock((void *)spin);
}
#endif

// Rw locks
#if INTERPOSE_RWLOCK
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
  TEST_INTERPOSITION();
  DASSERT(sizeof(pthread_rwlock_t) > sizeof(lock_as_t));
  return interpose_lock_init((void *)rwlock, true);
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_destroy((void *)rwlock);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_lock((void *)rwlock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_lock((void *)rwlock);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *lcok, const struct timespec *abstime)
{
  TEST_INTERPOSITION();
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *abstime)
{
  TEST_INTERPOSITION();
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_trylock((void *)rwlock);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_trylock((void *)rwlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
  TEST_INTERPOSITION();
  return interpose_lock_unlock((void *)rwlock);
}
#endif

#if INTERPOSE_BARRIERS
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count)
{
  TEST_INTERPOSITION();
  DASSERT(sizeof(pthread_barrier_t) > sizeof(barrier_as_t));
  return interpose_barrier_init((void *)barrier, (void *)attr, count);
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
  TEST_INTERPOSITION();
  return interpose_barrier_destroy((void *)barrier);
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
  TEST_INTERPOSITION();
  return interpose_barrier_wait((void *)barrier);
}

int pthread_barrierattr_init(pthread_barrierattr_t *attr)
{
  TEST_INTERPOSITION();
  return libslock_barrierattr_init((void *)attr);
}

int pthread_barrierattr_destroy(pthread_barrierattr_t *attr)
{
  TEST_INTERPOSITION();
  return libslock_barrierattr_destroy((void *)attr);
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared)
{
  TEST_INTERPOSITION();
  return libslock_barrierattr_getpshared((void *)attr, pshared);
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared)
{
  TEST_INTERPOSITION();
  return libslock_barrierattr_setpshared((void *)attr, pshared);
}
#endif
