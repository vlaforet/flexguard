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

#include <bpf/libbpf.h>
#include <sys/resource.h>
#include "hybridlock.skel.h"

#define UNLOCKED 0

__thread unsigned long *hybridlock_seeds;

#ifndef HYBRIDLOCK_PTHREAD_MUTEX
static long futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

static void futex_wait(void *addr, int val)
{
    futex(addr, FUTEX_WAIT, val, NULL, NULL, 0); /* Wait if *addr == val. */
}

static void futex_wake(void *addr, int nb_threads)
{
    futex(addr, FUTEX_WAKE, nb_threads, NULL, NULL, 0);
}

static void futex_lock(futex_lock_t *lock)
{
    int state;

    if ((state = __sync_val_compare_and_swap(&lock->state, 0, 1)) != 0)
    {
        if (state != 2)
            state = __sync_lock_test_and_set(&lock->state, 2);
        while (state != 0)
        {
            futex_wait((void *)&lock->state, 2);
            state = __sync_lock_test_and_set(&lock->state, 2);
        }
    }
}

static void futex_unlock(futex_lock_t *lock)
{
    if (__sync_fetch_and_sub(&lock->state, 1) != 1)
    {
        lock->state = 0;
        futex_wake((void *)&lock->state, 1);
    }
}
#endif

int hybridlock_trylock(hybridlock_lock_t *the_lock, uint32_t *limits)
{
    uint32_t pid = gettid();
    if (CAS_U32(&(the_lock->data.lock), UNLOCKED, pid) != 0)
        return 1;

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    if (pthread_mutex_trylock(&the_lock->data.mutex_lock) != 0)
        return 1;
#else
    futex_lock(&the_lock->data.futex_lock);
#endif

    return 0;
}

void hybridlock_lock(hybridlock_lock_t *the_lock, uint32_t *limits)
{
    uint32_t pid = gettid();
    volatile hybridlock_lock_type_t *l = &(the_lock->data.lock);

    while (CAS_U32(l, UNLOCKED, pid) && the_lock->data.spinning)
    {
        PAUSE;
    }

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_lock(&the_lock->data.mutex_lock);
#else
    futex_lock(&the_lock->data.futex_lock);
#endif
    SWAP_U32(l, pid);
}

void hybridlock_unlock(hybridlock_lock_t *the_lock)
{
    COMPILER_BARRIER;
#ifdef __tile__
    MEM_BARRIER;
#endif
    the_lock->data.lock = UNLOCKED;

#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_unlock(&the_lock->data.mutex_lock);
#else
    futex_unlock(&the_lock->data.futex_lock);
#endif
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
#ifdef HYBRIDLOCK_PTHREAD_MUTEX
        pthread_mutex_init(&the_locks[i].data.mutex_lock, NULL);
#endif
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

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

int init_hybridlock_global(hybridlock_lock_t *the_lock)
{
    struct hybridlock_bpf *skel;
    int err;

    libbpf_set_print(libbpf_print_fn);

    skel = hybridlock_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    the_lock->data.lock = UNLOCKED;
    the_lock->data.spinning = 1;
#ifdef HYBRIDLOCK_PTHREAD_MUTEX
    pthread_mutex_init(&the_lock->data.mutex_lock, NULL);
#endif

    skel->bss->tgid = getpid();
    skel->bss->input_pid = &the_lock->data.lock;
    skel->bss->input_spinning = &the_lock->data.spinning;

    err = hybridlock_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }

    err = hybridlock_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }

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
