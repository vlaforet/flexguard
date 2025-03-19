/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Hugo Guiroux <hugo.guiroux at gmail dot com>
 *               2013 Tudor David
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
#include <litl/padding.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include "../utils.h"

#ifndef __LITL_UTILS_H__
#define __LITL_UTILS_H__

#include <litl/topology.h>

#define MAX_THREADS MAX_NUMBER_THREADS
#define CPU_PAUSE() PAUSE
#define MEMORY_BARRIER() __sync_synchronize()
#define REP_VAL 23

#define OPTERON_OPTIMIZE 1
#ifdef OPTERON_OPTIMIZE
#define PREFETCHW(x) asm volatile("prefetchw %0" ::"m"(*(unsigned long *)x))
#else
#define PREFETCHW(x)
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

#ifdef DEBUG_LITL
#define DPRINT_LITL(...) fprintf(stderr, ##__VA_ARGS__)
#define DPRINT_PTHREAD(...) fprintf(stderr, ##__VA_ARGS__)
#else
#define DPRINT_LITL(...)
#define DPRINT_PTHREAD(...)
#endif

static inline void *alloc_cache_align(size_t n)
{
    void *res = 0;
    if ((MEMALIGN(&res, CACHE_LINE_SIZE, cache_align(n)) < 0) || !res)
    {
        fprintf(stderr, "MEMALIGN(%llu, %llu)", (unsigned long long)n,
                (unsigned long long)cache_align(n));
        exit(-1);
    }
    return res;
}

static inline void *xchg_64(void *ptr, void *x)
{
    __asm__ __volatile__("xchgq %0,%1"
                         : "=r"((unsigned long long)x)
                         : "m"(*(volatile long long *)ptr),
                           "0"((unsigned long long)x)
                         : "memory");

    return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
    __asm__ __volatile__("xchgl %0,%1"
                         : "=r"((unsigned)x)
                         : "m"(*(volatile unsigned *)ptr), "0"(x)
                         : "memory");

    return x;
}

// test-and-set uint8_t, from libslock
static inline uint8_t l_tas_uint8(volatile uint8_t *addr)
{
    uint8_t oldval;
    __asm__ __volatile__("xchgb %0,%1"
                         : "=q"(oldval), "=m"(*addr)
                         : "0"((unsigned char)0xff), "m"(*addr)
                         : "memory");
    return (uint8_t)oldval;
}

static inline uint64_t rdpmc(unsigned int counter)
{
    uint32_t low, high;

    asm volatile("rdpmc" : "=a"(low), "=d"(high) : "c"(counter));

    return low | ((uint64_t)high) << 32;
}

static inline uint64_t rdtsc(void)
{
    uint32_t low, high;

    asm volatile("rdtsc" : "=a"(low), "=d"(high));

    return low | ((uint64_t)high) << 32;
}

// EPFL libslock
#define my_random xorshf96
#define getticks rdtsc
typedef uint64_t ticks;

static inline void wait_cycles(uint64_t cycles)
{
    if (cycles < 256)
    {
        cycles /= 6;
        while (cycles--)
        {
            RAW_PAUSE;
        }
    }
    else
    {
        ticks _start_ticks = getticks();
        ticks _end_ticks = _start_ticks + cycles - 130;
        while (getticks() < _end_ticks)
            ;
    }
}
#endif
