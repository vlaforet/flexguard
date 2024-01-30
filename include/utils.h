/*
 * File: utils.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description:
 *      Some utility functions
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

#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include <numa.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#ifdef DEBUG
#include <assert.h>
#endif

#include "platform_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PAUSE _mm_pause()
    static inline void pause_rep(uint32_t num_reps)
    {
        uint32_t i;
        for (i = 0; i < num_reps; i++)
        {
            PAUSE;
            /* PAUSE; */
            /* asm volatile ("NOP"); */
        }
    }

    static inline void nop_rep(uint32_t num_reps)
    {
        uint32_t i;
        for (i = 0; i < num_reps; i++)
        {
            asm volatile("NOP");
        }
    }

    /*
     * Retrieves current TSC frequency.
     * Calls are cached.
     */
    static inline unsigned long get_tsc_frequency()
    {
        static unsigned long frequency = 0;
        if (!frequency)
        {
            FILE *file;
            char text[32];
            file = popen("sudo bpftrace -e 'BEGIN { printf(\"%u\", *kaddr(\"tsc_khz\")); exit(); }' | sed -n 2p", "r");
            if (file == NULL)
            {
                fprintf(stderr, "popen frequency");
                exit(1);
            }

            fgets(text, 32, file);
            if (!(frequency = atoi(text)))
            {
                fprintf(stderr, "unable to retrieve TSC frequency, check that bpftrace is properly installed");
                exit(1);
            }
        }
        return frequency;
    }

    /*
     * FUTEX_WAIT_PRIVATE syscall.
     * addr: Address to wait on.
     * val: value that addr should point to.
     */
    static inline long futex_wait(void *addr, int val)
    {
        return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0); /* Wait if *addr == val. */
    }

    /*
     * FUTEX_WAKE_PRIVATE syscall.
     * addr: Address waiters are waiting on.
     * nb_threads: number of threads to wake up.
     */
    static inline long futex_wake(void *addr, int nb_threads)
    {
        return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, nb_threads, NULL, NULL, 0);
    }

// debugging functions
#ifdef DEBUG
#define DPRINT(args...) fprintf(stderr, args);
#define DASSERT(args...) assert(args)
#else
#define DPRINT(...)
#define DASSERT(...)
#endif

    typedef uint64_t ticks;

    static inline double wtime(void)
    {
        struct timeval t;
        gettimeofday(&t, NULL);
        return (double)t.tv_sec + ((double)t.tv_usec) / 1000000.0;
    }

    static inline void set_cpu(int cpu)
    {
        // INT_MAX cpu id disables cpu pinning (Useful for some benchmarks)
        if (cpu >= INT_MAX)
            return;
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        numa_set_preferred(get_cluster(cpu));
        pthread_t thread = pthread_self();
        if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask) != 0)
        {
            fprintf(stderr, "Error setting thread affinity\n");
        }
    }

    static inline ticks getticks(void)
    {
        return __builtin_ia32_rdtsc();
    }

    static inline void cdelay(ticks cycles)
    {
        ticks __ts_end = getticks() + (ticks)cycles;
        while (getticks() < __ts_end)
            ;
    }

    static inline void cpause(ticks cycles)
    {
        ticks i;
        for (i = 0; i < cycles; i++)
        {
            __asm__ __volatile__("nop");
        }
    }

    static inline void udelay(unsigned int micros)
    {
        double __ts_end = wtime() + ((double)micros / 1000000);
        while (wtime() < __ts_end)
            ;
    }

    static inline ticks get_noop_duration()
    {
#define NOOP_CALC_REPS 1000000
        ticks noop_dur = 0;
        uint32_t i;
        ticks start;
        ticks end;
        start = getticks();
        for (i = 0; i < NOOP_CALC_REPS; i++)
        {
            __asm__ __volatile__("nop");
        }
        end = getticks();
        noop_dur = (ticks)((end - start) / (double)NOOP_CALC_REPS);
        return noop_dur;
    }

    /// Round up to next higher power of 2 (return x if it's already a power
    /// of 2) for 32-bit numbers
    static inline uint32_t pow2roundup(uint32_t x)
    {
        if (x == 0)
            return 1;
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }
#define my_random xorshf96

    /*
     * Returns a pseudo-random value in [1;range).
     * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
     * the granularity of rand() could be lower-bounded by the 32767^th which might
     * be too high for given values of range and initial.
     */
    static inline long rand_range(long r)
    {
        int m = RAND_MAX;
        long d, v = 0;

        do
        {
            d = (m > r ? r : m);
            v += 1 + (long)(d * ((double)rand() / ((double)(m) + 1.0)));
            r -= m;
        } while (r > 0);
        return v;
    }

    // fast but weak random number generator for the sparc machine
    static inline uint32_t fast_rand()
    {
        return ((getticks() & 4294967295) >> 4);
    }

    static inline unsigned long *seed_rand()
    {
        unsigned long *seeds;
        int num_seeds = CACHE_LINE_SIZE / sizeof(unsigned long);
        if (num_seeds < 3)
            num_seeds = 3;
        seeds = (unsigned long *)memalign(CACHE_LINE_SIZE, num_seeds * sizeof(unsigned long));
        seeds[0] = getticks() % 123456789;
        seeds[1] = getticks() % 362436069;
        seeds[2] = getticks() % 521288629;
        return seeds;
    }

    // Marsaglia's xorshf generator
    static inline unsigned long xorshf96(unsigned long *x, unsigned long *y, unsigned long *z)
    { // period 2^96-1
        unsigned long t;
        (*x) ^= (*x) << 16;
        (*x) ^= (*x) >> 5;
        (*x) ^= (*x) << 1;

        t = *x;
        (*x) = *y;
        (*y) = *z;
        (*z) = t ^ (*x) ^ (*y);

        return *z;
    }

#ifdef __cplusplus
}

#endif

#endif
