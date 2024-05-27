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

unsigned int last_thread_id;
__thread unsigned int cur_thread_id;

struct routine
{
  void *(*fct)(void *);
  void *arg;
};

int (*REAL(pthread_create))(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) __attribute__((aligned(CACHE_LINE_SIZE)));

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

static void __attribute__((constructor)) REAL(interpose_init)(void)
{
  static volatile uint8_t init_lock = 0;
  if (exactly_once(&init_lock) != 0)
    return;

  cur_thread_id = __sync_fetch_and_add(&last_thread_id, 1);
  CHECK_NUMBER_THREADS_FATAL(cur_thread_id);

  LOAD_FUNC(pthread_create, 1);

#if USE_REAL_PTHREAD == 1
  LOAD_FUNC(pthread_mutex_lock, 1);
  LOAD_FUNC(pthread_mutex_trylock, 1);
  LOAD_FUNC(pthread_mutex_unlock, 1);
  LOAD_FUNC(pthread_mutex_init, 1);
  LOAD_FUNC(pthread_mutex_destroy, 1);
  LOAD_FUNC_VERSIONED(pthread_cond_timedwait, 1, GLIBC_2_3_2);
  LOAD_FUNC_VERSIONED(pthread_cond_wait, 1, GLIBC_2_3_2);
  LOAD_FUNC_VERSIONED(pthread_cond_broadcast, 1, GLIBC_2_3_2);
  LOAD_FUNC_VERSIONED(pthread_cond_destroy, 1, GLIBC_2_3_2);
  LOAD_FUNC_VERSIONED(pthread_cond_init, 1, GLIBC_2_3_2);
  LOAD_FUNC_VERSIONED(pthread_cond_signal, 1, GLIBC_2_3_2);
#endif

  __sync_synchronize();
  init_lock = 2;
}

static void __attribute__((destructor)) REAL(interpose_exit)(void)
{
}

static lock_local_data *get_me(lock_as_t *lock)
{
  lock_as_context_t *ctx = &lock->contexts[cur_thread_id];
  if (UNLIKELY(ctx->status != 1))
  {
    init_lock_local(lock->lock, &ctx->me);
    ctx->status = 1;
  }

  return &ctx->me;
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

  lock->lock = (lock_global_data *)malloc((sizeof(lock_global_data)));

  lock->contexts = (lock_as_context_t *)malloc((sizeof(lock_as_context_t) * MAX_NUMBER_THREADS));
  for (int i = 0; i < MAX_NUMBER_THREADS; i++)
    lock->contexts[i].status = 0;

  int res = init_lock_global(lock->lock);
  lock->status = 2;
  return res;
}

static int interpose_lock_destroy(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);
  if (LIKELY(lock->status == 2))
  {
    free_lock_global(*lock->lock);
    free(lock->lock);
    free(lock->contexts);
    lock->status = 0;
  }

  return 0;
}

static int interpose_lock_lock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  lock_local_data *me = get_me(lock);
  acquire_lock(me, lock->lock);
  return 0;
}

static int interpose_lock_trylock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  lock_local_data *me = get_me(lock);

  if (acquire_trylock(me, lock->lock) == 0)
    return 0;
  else
    return EBUSY;
}

static int interpose_lock_unlock(void *raw_lock)
{
  lock_as_t *lock = CAST_TO_LOCK(raw_lock);

  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  lock_local_data *me = get_me(lock);
  release_lock(me, lock->lock);
  return 0;
}

static int interpose_cond_init(void *raw_cond, bool force)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);

  if (force)
    cond->status = 0;

  if (exactly_once(&cond->status) != 0)
    return 0;

  cond->cond = (lock_condvar_t *)malloc((sizeof(lock_condvar_t)));

  int res = condvar_init(cond->cond);
  cond->status = 2;
  return res;
}

static int interpose_cond_destroy(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);

  if (LIKELY(cond->status == 2))
    return condvar_destroy(cond->cond);
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

  lock_local_data *me = get_me(lock);
  return condvar_timedwait(cond->cond, me, lock->lock, abstime);
}

static int interpose_cond_wait(void *raw_cond, void *raw_lock)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  lock_as_t *lock = CAST_TO_LOCK(raw_lock);
  if (UNLIKELY(lock->status != 2))
    interpose_lock_init(raw_lock, false);

  lock_local_data *me = get_me(lock);
  return condvar_wait(cond->cond, me, lock->lock);
}

static int interpose_cond_signal(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  return condvar_signal(cond->cond);
}

static int interpose_cond_broadcast(void *raw_cond)
{
  condvar_as_t *cond = CAST_TO_COND(raw_cond);
  if (UNLIKELY(cond->status != 2))
    interpose_cond_init(raw_cond, false);

  return condvar_broadcast(cond->cond);
}

static void *lp_start_routine(void *_arg)
{
  struct routine *r = _arg;
  void *(*fct)(void *) = r->fct;
  void *arg = r->arg;
  void *res;
  free(r);

  cur_thread_id = __sync_fetch_and_add(&last_thread_id, 1);
  CHECK_NUMBER_THREADS_FATAL(cur_thread_id);
  __sync_synchronize();

  res = fct(arg);
  return res;
}

int __pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
  struct routine *r = malloc(sizeof(struct routine));

  r->fct = start_routine;
  r->arg = arg;

  return REAL(pthread_create)(thread, attr, lp_start_routine, r);
}
__asm__(".symver __pthread_create,pthread_create@@" GLIBC_2_2_5);
__asm__(".symver __pthread_create,pthread_create@" GLIBC_2_34);

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  DASSERT(sizeof(pthread_mutex_t) > sizeof(lock_as_t));
  return interpose_lock_init(mutex, true);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  return interpose_lock_destroy(mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  return interpose_lock_lock(mutex);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  return interpose_lock_trylock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  return interpose_lock_unlock(mutex);
}

int __pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  DASSERT(sizeof(pthread_cond_t) > sizeof(condvar_as_t));
  return interpose_cond_init(cond, true);
}
__asm__(".symver __pthread_cond_init,pthread_cond_init@@" GLIBC_2_3_2);

int __pthread_cond_destroy(pthread_cond_t *cond)
{
  return interpose_cond_destroy(cond);
}
__asm__(".symver __pthread_cond_destroy,pthread_cond_destroy@@" GLIBC_2_3_2);

int __pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
  return interpose_cond_timedwait(cond, mutex, abstime);
}
__asm__(".symver __pthread_cond_timedwait,pthread_cond_timedwait@@" GLIBC_2_3_2);

int __pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  return interpose_cond_wait(cond, mutex);
}
__asm__(".symver __pthread_cond_wait,pthread_cond_wait@@" GLIBC_2_3_2);

int __pthread_cond_signal(pthread_cond_t *cond)
{
  return interpose_cond_signal(cond);
}
__asm__(".symver __pthread_cond_signal,pthread_cond_signal@@" GLIBC_2_3_2);

int __pthread_cond_broadcast(pthread_cond_t *cond)
{
  return interpose_cond_broadcast(cond);
}
__asm__(".symver __pthread_cond_broadcast,pthread_cond_broadcast@@" GLIBC_2_3_2);

// Spinlocks
#if INTERPOSE_SPINLOCK
int pthread_spin_init(pthread_spinlock_t *spin, int pshared)
{
  DASSERT(sizeof(pthread_spinlock_t) > sizeof(lock_as_t));
  return interpose_lock_init((void *)spin, true);
}

int pthread_spin_destroy(pthread_spinlock_t *spin)
{
  return interpose_lock_destroy((void *)spin);
}

int pthread_spin_lock(pthread_spinlock_t *spin)
{
  return interpose_lock_lock((void *)spin);
}

int pthread_spin_trylock(pthread_spinlock_t *spin)
{
  return interpose_lock_trylock((void *)spin);
}

int pthread_spin_unlock(pthread_spinlock_t *spin)
{
  return interpose_lock_unlock((void *)spin);
}
#endif

// Rw locks
#if INTERPOSE_RWLOCK
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
  DASSERT(sizeof(pthread_rwlock_t) > sizeof(lock_as_t));
  return interpose_lock_init((void *)rwlock, true);
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
  return interpose_lock_destroy((void *)rwlock);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
  return interpose_lock_lock((void *)rwlock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
  return interpose_lock_lock((void *)rwlock);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *lcok, const struct timespec *abstime)
{
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock, const struct timespec *abstime)
{
  fprintf(stderr, "Timed locks not supported\n");
  exit(EXIT_FAILURE);
}

int pthread_rwlock_rdtrylock(pthread_rwlock_t *rwlock)
{
  return interpose_lock_trylock((void *)rwlock);
}

int pthread_rwlock_wrtrylock(pthread_rwlock_t *rwlock)
{
  return interpose_lock_trylock((void *)rwlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
  return interpose_lock_unlock((void *)rwlock);
}
#endif
