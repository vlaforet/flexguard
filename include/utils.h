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

#define RAW_PAUSE _mm_pause()
#ifdef PAUSE_COUNTER
    extern long pause_counter;

#define PAUSE                                    \
    do                                           \
    {                                            \
        RAW_PAUSE;                               \
        __sync_fetch_and_add(&pause_counter, 1); \
    } while (0)
#else
#define PAUSE RAW_PAUSE
#endif

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
            __asm__ volatile("NOP");
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
                perror("popen frequency");
                exit(EXIT_FAILURE);
            }

            fgets(text, 32, file);
            if (!(frequency = atoi(text)))
            {
                fprintf(stderr, "Unable to retrieve TSC frequency, check that bpftrace is properly installed.\n");
                exit(EXIT_FAILURE);
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

    static inline long futex_wait_timeout(void *addr, int val, struct timespec *timeout)
    {
        return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, timeout, NULL, 0); /* Wait if *addr == val. */
    }

    static inline long futex_wait_timeout_abs(void *addr, int val, struct timespec *abs)
    {
        return syscall(SYS_futex, addr, FUTEX_WAIT_BITSET_PRIVATE, val, abs, NULL, FUTEX_BITSET_MATCH_ANY); /* Wait if *addr == val. */
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
#define DPRINT(...) fprintf(stderr, __VA_ARGS__);
#define DASSERT(...) assert(__VA_ARGS__)
#else
#define DPRINT(...)
#define DASSERT(...)
#endif

    typedef uint64_t ticks;

    static inline ticks getticks(void)
    {
        return __builtin_ia32_rdtsc();
    }

    static inline void cpause(ticks cycles)
    {
        ticks i;
        for (i = 0; i < cycles; i++)
        {
            __asm__ __volatile__("nop");
        }
    }

    /*
     * exactly_once
     * Ensures a block of code has been executed exactly once.
     * Spins while another thread is executing.
     *
     * Returns
     *    0 if never been executed (Code needs to be executed)
     *    1 if waited and finished
     *    2 if already executed
     *
     * Example:
```
static uint8_t init = 0;
if (exactly_once(&init) == 0)
{
    // Code to be executed exactly once
    init = 2;
}
```
     */
    static inline int exactly_once(volatile uint8_t *value)
    {
        if (*value == 2)
            return 2;
        uint8_t curr = __sync_val_compare_and_swap(value, 0, 1);
        if (curr == 1)
            while (*value == 1)
                RAW_PAUSE;

        return curr;
    }

#define CHECK_NUMBER_THREADS_FATAL(nb_thread)                                                   \
    if (nb_thread >= MAX_NUMBER_THREADS)                                                        \
    {                                                                                           \
        fprintf(stderr, "Too many threads. Increase MAX_NUMBER_THREADS in platform_defs.h.\n"); \
        exit(EXIT_FAILURE);                                                                     \
    }

#define UNLIKELY(...) __glibc_unlikely(__VA_ARGS__)
#define LIKELY(...) __glibc_likely(__VA_ARGS__)

#define UNUSED(x) UNUSED_##x __attribute__((unused))

#ifdef __cplusplus
}
#endif

#endif
