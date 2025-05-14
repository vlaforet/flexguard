/*
 * File: spinpark.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a spin-then-park lock
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

#include "spinpark.h"

#ifndef SPINPARK_SPIN_TIME
#define SPINPARK_SPIN_TIME 2700 // Default LiTL value
#endif

#ifdef DEBUG
__thread uint8_t locked_thread = 0;
#endif

int spinpark_trylock(spinpark_lock_t *lock)
{
  if (__sync_val_compare_and_swap(&lock->data, 0, 1) == 0)
    return 0;   // Success
  return EBUSY; // Locked
}

void spinpark_lock(spinpark_lock_t *lock)
{
#ifdef DEBUG
  if (locked_thread)
    DPRINT("Nested locking.");

  locked_thread = 1;
#endif

  int i = 0;
  int state;
  while ((state = __sync_val_compare_and_swap(&lock->data, 0, 1)) != 0 && i++ < SPINPARK_SPIN_TIME)
    PAUSE;

  if (state != 0)
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

void spinpark_unlock(spinpark_lock_t *lock)
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

int spinpark_init(spinpark_lock_t *the_lock)
{
  the_lock->data = 0;
  MEM_BARRIER;
  return 0;
}

void spinpark_destroy(spinpark_lock_t *the_lock)
{
}

/*
 *  Condition Variables
 */

int spinpark_cond_init(spinpark_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}

int spinpark_cond_wait(spinpark_cond_t *cond, spinpark_lock_t *the_lock)
{
  // No need for atomic operations, I have the lock
  uint32_t target = ++cond->target;
  uint32_t seq = cond->seq;
  spinpark_unlock(the_lock);

  while (target > seq)
  {
    futex_wait(&cond->seq, seq);
    seq = cond->seq;
  }
  spinpark_lock(the_lock);
  return 0;
}

int spinpark_cond_timedwait(spinpark_cond_t *cond, spinpark_lock_t *the_lock, const struct timespec *ts)
{
  fprintf(stderr, "Timedwait not supported yet.\n");
  exit(EXIT_FAILURE);
}

int spinpark_cond_signal(spinpark_cond_t *cond)
{
  cond->seq++;
  futex_wake(&cond->seq, 1);
  return 0;
}

int spinpark_cond_broadcast(spinpark_cond_t *cond)
{
  cond->seq = cond->target;
  futex_wake(&cond->seq, INT_MAX);
  return 0;
}

int spinpark_cond_destroy(spinpark_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}