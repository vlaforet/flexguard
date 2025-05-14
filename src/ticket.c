/*
 * File: ticket.c
 * Author: Tudor David <tudor.david@epfl.ch>, Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *
 * Description:
 *      An implementation of a ticket lock with:
 *       - proportional back-off optimization
 *           Magny-Cours processors
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David, Vasileios Trigonakis
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

#include "ticket.h"

#define SUB_ABS(A, B) A > B ? A - B : B - A

int ticket_trylock(ticket_lock_t *lock)
{
  uint32_t me = lock->tail;
  uint32_t me_new = me + 1;
  uint64_t cmp = ((uint64_t)me << 32) + me_new;
  uint64_t cmp_new = ((uint64_t)me_new << 32) + me_new;
  uint64_t *la = (uint64_t *)lock;
  if (CAS_U64(la, cmp, cmp_new) == cmp)
    return 0;   // Success
  return EBUSY; // Locked
}

void ticket_lock(ticket_lock_t *lock)
{
  uint32_t my_ticket = IAF_U32(&(lock->tail));

  while (1)
  {
    uint32_t cur = lock->head;
    if (cur == my_ticket)
      break;

    uint32_t distance = SUB_ABS(cur, my_ticket);

    if (distance <= 1)
      nop_rep(TICKET_WAIT_NEXT);
    /* else if (distance > 20)
       sched_yield();*/
    else
      nop_rep(distance * TICKET_BASE_WAIT);
  }
}

void ticket_unlock(ticket_lock_t *lock)
{
  COMPILER_BARRIER();
  lock->head++;
}

int ticket_init(ticket_lock_t *the_lock)
{
  the_lock->head = 1;
  the_lock->tail = 0;
  MEM_BARRIER;
  return 0;
}

void ticket_destroy(ticket_lock_t *the_lock)
{
  // Nothing to do
}