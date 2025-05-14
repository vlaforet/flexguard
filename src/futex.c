/*
 * File: futex.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a futex lock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Victor Laforet
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

#include "futex.h"

#ifdef DEBUG
__thread uint8_t locked_thread = 0;

int trylock_counter = 0;
int lock_counter = 0;
#endif

int futex_trylock(futex_lock_t *lock)
{
  if (__sync_val_compare_and_swap(&lock->data, 0, 1) != 0)
    return EBUSY; // Locked

#ifdef DEBUG
  if (locked_thread)
    DPRINT("Nested locking.");

  locked_thread = 1;
  __sync_fetch_and_add(&trylock_counter, 1);
#endif

  return 0; // Success
}

void futex_lock(futex_lock_t *lock)
{
#ifdef DEBUG
  if (locked_thread)
    DPRINT("Nested locking.");

  locked_thread = 1;
  __sync_fetch_and_add(&lock_counter, 1);
#endif

  int state;
  if ((state = __sync_val_compare_and_swap(&lock->data, 0, 1)) != 0)
  {
    if (state != 2)
      state = __sync_lock_test_and_set(&lock->data, 2);
    while (state != 0)
    {
      futex_wait((void *)&lock->data, 2);
      state = __sync_lock_test_and_set(&lock->data, 2);
    }
  }
}

void futex_unlock(futex_lock_t *lock)
{
#ifdef DEBUG
  locked_thread = 0;
#endif
  if (__sync_fetch_and_sub(&lock->data, 1) != 1)
  {
    lock->data = 0;
    futex_wake((void *)&lock->data, 1);
  }
}

int futex_init(futex_lock_t *the_lock)
{
  the_lock->data = 0;
  MEM_BARRIER;
  return 0;
}

void futex_destroy(futex_lock_t *the_lock)
{
#ifdef DEBUG
  printf("Trylock: %d\nLock: %d\n", trylock_counter, lock_counter);
#endif
}

/*
 *  Condition Variables
 */

int futex_cond_init(futex_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}

int futex_cond_wait(futex_cond_t *cond, futex_lock_t *the_lock)
{
  // No need for atomic operations, I have the lock
  uint32_t target = ++cond->target;
  uint32_t seq = cond->seq;
  futex_unlock(the_lock);

  while (target > seq)
  {
    futex_wait(&cond->seq, seq);
    seq = cond->seq;
  }
  futex_lock(the_lock);
  return 0;
}

int futex_cond_timedwait(futex_cond_t *cond, futex_lock_t *the_lock, const struct timespec *ts)
{
  fprintf(stderr, "Timedwait not supported yet.\n");
  exit(EXIT_FAILURE);
}

int futex_cond_signal(futex_cond_t *cond)
{
  cond->seq++;
  futex_wake(&cond->seq, 1);
  return 0;
}

int futex_cond_broadcast(futex_cond_t *cond)
{
  cond->seq = cond->target;
  futex_wake(&cond->seq, INT_MAX);
  return 0;
}

int futex_cond_destroy(futex_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}