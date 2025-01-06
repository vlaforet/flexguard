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

static __thread struct extend_map *extend_map = NULL;

static inline void init_extend_map()
{
    int fd = open(EXTEND_SCHED_FS, O_RDWR);
    if (fd < 0)
    {
        perror("Open(" EXTEND_SCHED_FS ")");
        exit(-1);
    }

    void *map = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
    {
        perror("mmap(" EXTEND_SCHED_FS ")");
        exit(-1);
    }

    extend_map = map;
}

static inline void extend()
{
    if (UNLIKELY(!extend_map))
        init_extend_map();

    extend_map->flags = 1;
}

static inline void unextend()
{
    if (UNLIKELY(!extend_map))
        init_extend_map();

    unsigned long prev = atomic_exchange(&extend_map->flags, 0);
    if (prev & 2)
        sched_yield();
}

int spinextend_trylock(spinextend_lock_t *the_lock)
{
    extend();
    if (TAS_U8(&(the_lock->lock)) == UNLOCKED)
        return UNLOCKED;

    unextend(); // Unextend on failure to acquire
    return LOCKED;
}
void spinextend_lock(spinextend_lock_t *the_lock)
{
    while (the_lock->lock != UNLOCKED || spinextend_trylock(the_lock) != UNLOCKED)
        PAUSE;
}

void spinextend_unlock(spinextend_lock_t *the_lock)
{
    COMPILER_BARRIER;
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
