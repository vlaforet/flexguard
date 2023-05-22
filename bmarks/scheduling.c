/*
 * File: scheduling.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Benchmark varying thread count.
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

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <malloc.h>
#ifndef __sparc__
#include <numa.h>
#endif
#include "atomic_ops.h"
#include "utils.h"
#include "lock_if.h"

#ifdef USE_HYBRIDLOCK_LOCKS
#include "hybridlock.h"
#endif

#define DEFAULT_BASE_THREADS 0
#define DEFAULT_NB_THREADS 10
#define DEFAULT_LAUNCH_DELAY_MS 1000
#define DEFAULT_COMPUTE_CYCLES 100
#define DEFAULT_DUMMY_ARRAY_SIZE 1

#ifdef USE_HYBRIDLOCK_LOCKS
#define DEFAULT_SWITCH_THREAD_COUNT 48
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define FREQUENCY_CMD "sudo bpftrace -e 'BEGIN { printf(\"%u\", *kaddr(\"tsc_khz\")); exit(); }' | sed -n 2p"

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int compute_cycles = DEFAULT_COMPUTE_CYCLES;
int dummy_array_size = DEFAULT_DUMMY_ARRAY_SIZE;

long long unsigned int start;
_Atomic int thread_count = 0;
int needed_threads;
long unsigned int frequency;

lock_global_data the_lock;
__attribute__((aligned(CACHE_LINE_SIZE))) lock_local_data *local_th_data;

typedef struct dummy_array_t
{
    union
    {
        struct
        {
            int counter;
            struct dummy_array_t *next;
        };
        uint8_t padding[CACHE_LINE_SIZE];
    };
} dummy_array_t;
__attribute__((aligned(CACHE_LINE_SIZE))) dummy_array_t *dummy_array;

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data
{
    union
    {
        struct
        {
            int id;
            double cs_time;
            int reset;
        };
        uint8_t padding[CACHE_LINE_SIZE];
    };
} thread_data_t;

void *test(void *data)
{
    long long unsigned int t1, t2;
    int stop = 0, cs_count = 0, i = 0;
    dummy_array_t *arr;

    thread_data_t *d = (thread_data_t *)data;
    init_lock_local(INT_MAX, &the_lock, &(local_th_data[d->id]));
    thread_count++;

    while (!stop)
    {
        t1 = __builtin_ia32_rdtsc();

        acquire_write(&(local_th_data[d->id]), &the_lock);

        arr = dummy_array;
        for (i = 0; i < dummy_array_size; i++)
        {
            arr->counter++;
            arr = arr->next;
        }

        if (thread_count > needed_threads)
        {
            thread_count--;
            stop = 1;
        }

        release_write(&(local_th_data[d->id]), &the_lock);

        if (d->reset)
        {
            cs_count = 0;
            d->cs_time = 0;
            d->reset = 0;
        }
        cs_count++;

        t2 = __builtin_ia32_rdtsc();
        d->cs_time = (d->cs_time * (cs_count - 1) + (t2 - t1)) / cs_count;

        if (!stop)
            cpause(compute_cycles);
    }

    d->cs_time = 0;
    free_lock_local(local_th_data[d->id]);
    return NULL;
}

/* ################################################################### *
 * MEASUREMENT
 * ################################################################### */

void measurement(thread_data_t *data, int len)
{
    static long long unsigned int current;
    static int tc;
    static double tmp;
    static double sum;

    sum = 0;
    tc = 0;

    if (thread_count <= 0)
        return;

    current = __builtin_ia32_rdtsc();

    // Always go through all threads as they won't stop in any particular order.
    for (int i = 0; i < len; i++)
    {
        tmp = data[i].cs_time;
        if (tmp > 0 && !data[i].reset)
        {
            tc++;
            sum += tmp;
            data[i].reset = 1;
        }
    }

    printf("%d, %f, %f\n", tc, (current - start) / frequency, tc == 0 ? .0 : sum / tc / frequency);
}

/* ################################################################### *
 * SETUP
 * ################################################################### */

int main(int argc, char **argv)
{
    int i, c;

    int base_threads = DEFAULT_BASE_THREADS;
    int max_nb_threads = DEFAULT_NB_THREADS;
    int launch_delay = DEFAULT_LAUNCH_DELAY_MS;
#ifdef USE_HYBRIDLOCK_LOCKS
    int switch_thread_count = DEFAULT_SWITCH_THREAD_COUNT;
#endif

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"base-threads", required_argument, NULL, 'b'},
        {"cs-cycles", required_argument, NULL, 'c'},
        {"launch-delay", required_argument, NULL, 'd'},
        {"num-threads", required_argument, NULL, 'n'},
        {"cache-lines", required_argument, NULL, 't'},
#ifdef USE_HYBRIDLOCK_LOCKS
        {"switch-thread-count", required_argument, NULL, 's'},
#endif
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hb:c:d:n:t:s:", long_options, &i);

        if (c == -1)
            break;

        if (c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch (c)
        {
        case 0:
            /* Flag is automatically set */
            break;
        case 'h':
            printf("scheduling -- lock stress test\n");
            printf("\n");
            printf("Usage:\n");
            printf("  scheduling [options...]\n");
            printf("\n");
            printf("Options:\n");
            printf("  -h, --help\n");
            printf("        Print this message\n");
            printf("  -b, --base-threads <int>\n");
            printf("        Base number of threads (default=" XSTR(DEFAULT_BASE_THREADS) ")\n");
            printf("  -c, --compute-cycles <int>\n");
            printf("        Compute delay between critical sections, in cycles (default=" XSTR(DEFAULT_COMPUTE_CYCLES) ")\n");
            printf("  -d, --launch-delay <int>\n");
            printf("        Delay between thread creations in milliseconds (default=" XSTR(DEFAULT_LAUNCH_DELAY_MS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n");
            printf("  -t, --cache-lines <int>\n");
            printf("        Number of cache lines touched in each CS (default=" XSTR(DEFAULT_DUMMY_ARRAY_SIZE) ")\n");
#ifdef USE_HYBRIDLOCK_LOCKS
            printf("  -s, --switch-thread-count <int>\n");
            printf("        Core count after which the lock will be blocking (default=" XSTR(DEFAULT_SWITCH_THREAD_COUNT) ")\n");
            printf("        A value of -1 will disable the switch (always spin) and with a value of 0 the lock will never spin.\n");
#endif
            exit(0);
        case 'b':
            base_threads = atoi(optarg);
            break;
        case 'c':
            compute_cycles = atoi(optarg);
            break;
        case 'd':
            launch_delay = atoi(optarg);
            break;
        case 'n':
            max_nb_threads = atoi(optarg);
            break;
        case 't':
            dummy_array_size = atoi(optarg);
            break;
#ifdef USE_HYBRIDLOCK_LOCKS
        case 's':
            switch_thread_count = atoi(optarg);
            break;
#endif
        case '?':
            printf("Use -h or --help for help\n");
            exit(0);
        default:
            exit(1);
        }
    }

    printf("Base nb threads: %d\n", base_threads);
    printf("Nb threads max: %d\n", max_nb_threads);
    printf("Launch delay: %d\n", launch_delay);
    printf("Compute cycles: %d\n", compute_cycles);
    printf("Cache lines: %d\n", dummy_array_size);
#ifdef USE_HYBRIDLOCK_LOCKS
    printf("Switch thread count: %d\n", switch_thread_count);
#endif

    thread_data_t *data;
    pthread_t *threads;

    if ((data = (thread_data_t *)malloc(max_nb_threads * sizeof(thread_data_t))) == NULL)
    {
        perror("malloc data");
        exit(1);
    }
    if ((threads = (pthread_t *)malloc(max_nb_threads * sizeof(pthread_t))) == NULL)
    {
        perror("malloc threads");
        exit(1);
    }
    if ((local_th_data = (lock_local_data *)malloc(max_nb_threads * sizeof(lock_local_data))) == NULL)
    {
        perror("malloc local_th_data");
        exit(1);
    }
    if ((dummy_array = (dummy_array_t *)malloc(dummy_array_size * sizeof(dummy_array_t))) == NULL)
    {
        perror("malloc dummy_array");
        exit(1);
    }

    // Fill dummy arrays
    srand(time(NULL));
    i = 0;
    dummy_array_t *arr = dummy_array;
    while (i < dummy_array_size - 1)
    {
        c = rand() % dummy_array_size;
        if (&dummy_array[c] != arr && dummy_array[c].next == NULL)
        {
            arr->counter = 0;
            arr = arr->next = &dummy_array[c];
            i++;
        }
    }

    // Retrieve current frequency
    FILE *file;
    char text[32];
    file = popen(FREQUENCY_CMD, "r");
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
    printf("Frequency: %d\n", frequency);

    /* Init locks */
    DPRINT("Initializing locks\n");
    init_lock_global_nt(max_nb_threads, &the_lock);

    for (i = 0; i < max_nb_threads; i++)
    {
        data[i].id = i;
        data[i].cs_time = 0;
        data[i].reset = 0;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    struct timespec launch_timeout;
    launch_timeout.tv_sec = launch_delay / 1000;
    launch_timeout.tv_nsec = (launch_delay % 1000) * 1000000;

    needed_threads = max_nb_threads;
    start = __builtin_ia32_rdtsc();

    for (i = 0; i < max_nb_threads; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (i == switch_thread_count)
        {
            DPRINT("Switching to FUTEX hybrid lock\n");
            __sync_val_compare_and_swap(&the_lock.lock_history,
                                        LOCK_HISTORY(LOCK_TYPE_MCS),
                                        LOCK_TRANSITION(LOCK_TYPE_MCS, LOCK_TYPE_FUTEX));
        }
#endif

        DPRINT("Creating thread %d\n", i);
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
        {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        if (i > base_threads)
            measurement(data, max_nb_threads);

        if (i >= base_threads)
            nanosleep(&launch_timeout, NULL);
    }
    pthread_attr_destroy(&attr);

    if (base_threads == max_nb_threads)
        nanosleep(&launch_timeout, NULL);

    for (i = 0; i < 10; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (switch_thread_count > 0 && i == 5)
        {
            DPRINT("Switching to MCS hybrid lock\n");
            __sync_val_compare_and_swap(&the_lock.lock_history,
                                        LOCK_HISTORY(LOCK_TYPE_FUTEX),
                                        LOCK_TRANSITION(LOCK_TYPE_FUTEX, LOCK_TYPE_MCS));
        }
#endif

        measurement(data, max_nb_threads);
        nanosleep(&launch_timeout, NULL);
    }

    while (needed_threads > 0 && thread_count >= base_threads)
    {
        if (thread_count == needed_threads)
            needed_threads--;

        measurement(data, max_nb_threads);
        nanosleep(&launch_timeout, NULL);
    }
    needed_threads = 0;

    /* Wait for thread completion */
    for (i = 0; i < max_nb_threads; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    free_lock_global(the_lock);
    return EXIT_SUCCESS;
}
