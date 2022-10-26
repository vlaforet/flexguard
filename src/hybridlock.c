/*
 * File: hybridlock.c
 * Author: Tudor David <tudor.david@epfl.ch>mus
 *         Victor Laforet <victor.laforet@ip-paris.fr>
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

#include "hybridlock.h"

#define UNLOCKED 0
#define LOCKED 1

__thread unsigned long *hybridlock_seeds;

int hybridlock_trylock(hybridlock_lock_t *the_lock, uint32_t *limits)
{
    if (TAS_U8(&(the_lock->data.lock)) == 0 && pthread_mutex_trylock(&the_lock->data.blocking_lock) == 0)
        return 0;
    return 1;
}

void hybridlock_lock(hybridlock_lock_t *the_lock, uint32_t *limits)
{
    volatile hybridlock_lock_type_t *l = &(the_lock->data.lock);

    while (TAS_U8(l) && the_lock->data.spinning)
    {
        PAUSE;
    }

    pthread_mutex_lock(&the_lock->data.blocking_lock);
    TAS_U8(l);
}

void hybridlock_unlock(hybridlock_lock_t *the_lock)
{
    COMPILER_BARRIER;
#ifdef __tile__
    MEM_BARRIER;
#endif
    the_lock->data.lock = UNLOCKED;
    pthread_mutex_unlock(&the_lock->data.blocking_lock);
}

int is_free_hybridlock(hybridlock_lock_t *the_lock)
{
    if (the_lock->data.lock == UNLOCKED)
        return 1;
    return 0;
}

void set_blocking(hybridlock_lock_t *the_lock, int blocking)
{
    if (blocking)
    {
        the_lock->data.spinning = 0;
        DPRINT("Hybrid Lock: Blocking\n");
    }
    else
    {
        the_lock->data.spinning = 1;
        DPRINT("Hybrid Lock: Spinning\n");
    }
}

/*
   Some methods for easy lock array manipulation
   */

hybridlock_lock_t *init_hybridlock_array_global(uint32_t num_locks)
{
    hybridlock_lock_t *the_locks;
    the_locks = (hybridlock_lock_t *)malloc(num_locks * sizeof(hybridlock_lock_t));
    uint32_t i;
    for (i = 0; i < num_locks; i++)
    {
        the_locks[i].data.lock = UNLOCKED;
        the_locks[i].data.spinning = 1;
        pthread_mutex_init(&the_locks[i].data.blocking_lock, NULL);
    }

    MEM_BARRIER;
    return the_locks;
}

uint32_t *init_hybridlock_array_local(uint32_t thread_num, uint32_t size)
{
    // assign the thread to the correct core
    set_cpu(thread_num);
    hybridlock_seeds = seed_rand();

    uint32_t *limits;
    limits = (uint32_t *)malloc(size * sizeof(uint32_t));
    uint32_t i;
    for (i = 0; i < size; i++)
    {
        limits[i] = 1;
    }
    MEM_BARRIER;
    return limits;
}

void end_hybridlock_array_local(uint32_t *limits)
{
    free(limits);
}

void end_hybridlock_array_global(hybridlock_lock_t *the_locks)
{
    free(the_locks);
}

int init_hybridlock_global(hybridlock_lock_t *the_lock)
{
    the_lock->data.lock = UNLOCKED;
    the_lock->data.spinning = 1;
    pthread_mutex_init(&the_lock->data.blocking_lock, NULL);

    MEM_BARRIER;
    return 0;
}

int init_hybridlock_local(uint32_t thread_num, uint32_t *limit)
{
    // assign the thread to the correct core
    set_cpu(thread_num);
    *limit = 1;
    hybridlock_seeds = seed_rand();
    MEM_BARRIER;
    return 0;
}

void end_hybridlock_local()
{
    // function not needed
}

void end_hybridlock_global()
{
    // function not needed
}
