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

static void futex_wait(void *addr, int val)
{
  syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0); /* Wait if *addr == val. */
}

static void futex_wake(void *addr, int nb_threads)
{
  syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, nb_threads, NULL, NULL, 0);
}

int futex_trylock(futex_lock_t *lock)
{
  if (__sync_val_compare_and_swap(&lock->data, 0, 1) != 0)
    return 1; // Fail
  return 0;   // Success
}

void futex_lock(futex_lock_t *lock)
{
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
  if (__sync_fetch_and_sub(&lock->data, 1) != 1)
  {
    lock->data = 0;
    futex_wake((void *)&lock->data, 1);
  }
}

int is_free_futex(futex_lock_t *lock)
{
  if (lock->data == 0)
    return 1;
  return 0;
}

/*
 * Some methods for easy lock array manipulation
 */
futex_lock_t *init_futex_array_global(uint32_t num_locks)
{
  futex_lock_t *the_locks;
  the_locks = (futex_lock_t *)malloc(num_locks * sizeof(futex_lock_t));
  uint32_t i;
  for (i = 0; i < num_locks; i++)
  {
    the_locks[i].data = 0;
  }

  MEM_BARRIER;
  return the_locks;
}

void init_futex_array_local(uint32_t thread_num)
{
  // assign the thread to the correct core
  set_cpu(thread_num);
}

void end_futex_array_global(futex_lock_t *the_locks)
{
  free(the_locks);
}

int init_futex_global(futex_lock_t *the_lock)
{
  the_lock->data = 0;
  MEM_BARRIER;
  return 0;
}

int init_futex_local(uint32_t thread_num)
{
  // assign the thread to the correct core
  set_cpu(thread_num);
  return 0;
}
