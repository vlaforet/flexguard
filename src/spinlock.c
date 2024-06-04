/*
 * File: spinlock.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description:
 *      Simple test-and-set spinlock
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David
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

#include "spinlock.h"

int spinlock_trylock(spinlock_lock_t *the_lock)
{
    if (TAS_U8(&(the_lock->lock)) == 0)
        return 0;
    return 1;
}
void spinlock_lock(spinlock_lock_t *the_lock)
{
    volatile spinlock_lock_data_t *l = &(the_lock->lock);
    while (TAS_U8(l))
    {
        PAUSE;
    }
}

void spinlock_unlock(spinlock_lock_t *the_lock)
{
    COMPILER_BARRIER;
    the_lock->lock = UNLOCKED;
}

int init_spinlock_global(spinlock_lock_t *the_lock)
{
    the_lock->lock = UNLOCKED;
    MEM_BARRIER;
    return 0;
}

void end_spinlock_global()
{
    // function not needed
}
