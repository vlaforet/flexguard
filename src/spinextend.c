/*
 * File: spinextend.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Implementation of a spin lock extending its timeslice
 *      (https://lore.kernel.org/lkml/20231025054219.1acaa3dd@gandalf.local.home/)
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Victor Laforet
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

#include "spinextend.h"

int spinextend_trylock(spinextend_lock_t *the_lock)
{
    extend();
    if (TAS_U8(&(the_lock->lock)) == UNLOCKED)
        return 0; // Success

    unextend();   // Unextend on failure to acquire
    return EBUSY; // Locked
}
void spinextend_lock(spinextend_lock_t *the_lock)
{
    while (the_lock->lock != UNLOCKED || spinextend_trylock(the_lock) != 0)
        PAUSE;
}

void spinextend_unlock(spinextend_lock_t *the_lock)
{
    COMPILER_BARRIER();
    the_lock->lock = UNLOCKED;
    unextend();
}

int spinextend_init(spinextend_lock_t *the_lock)
{
    the_lock->lock = UNLOCKED;
    MEM_BARRIER;
    return 0;
}

void spinextend_destroy(spinextend_lock_t *the_lock)
{
    // function not needed
}

/*
 *  Condition Variables
 */

int spinextend_cond_init(spinextend_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}

int spinextend_cond_wait(spinextend_cond_t *cond, spinextend_lock_t *the_lock)
{
    // No need for atomic operations, I have the lock
    uint32_t target = ++cond->target;
    uint32_t seq = cond->seq;
    spinextend_unlock(the_lock);

    while (target > seq)
    {
#if defined(CONDVARS_BLOCK)
        futex_wait(&cond->seq, seq);
#else
        PAUSE;
#endif
        seq = cond->seq;
    }
    spinextend_lock(the_lock);
    return 0;
}

int spinextend_cond_timedwait(spinextend_cond_t *cond, spinextend_lock_t *the_lock, const struct timespec *ts)
{
    fprintf(stderr, "Timedwait not supported yet.\n");
    exit(EXIT_FAILURE);
}

int spinextend_cond_signal(spinextend_cond_t *cond)
{
    cond->seq++;
    return 0;
}

int spinextend_cond_broadcast(spinextend_cond_t *cond)
{
    cond->seq = cond->target;
    return 0;
}

int spinextend_cond_destroy(spinextend_cond_t *cond)
{
    cond->seq = 0;
    cond->target = 0;
    return 0;
}