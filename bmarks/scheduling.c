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
#include "atomic_ops.h"
#include "utils.h"
#include "lock_if.h"

#ifdef USE_HYBRIDLOCK_LOCKS
#include "hybridlock.h"
#endif

#define DEFAULT_BASE_THREADS 1
#define DEFAULT_MAX_THREADS 10
#define DEFAULT_STEP_DURATION_MS 1000
#define DEFAULT_CONTENTION 100
#define DEFAULT_DUMMY_ARRAY_SIZE 1

#ifdef USE_HYBRIDLOCK_LOCKS
#define DEFAULT_SWITCH_THREAD_COUNT -1
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define FREQUENCY_CMD "sudo bpftrace -e 'BEGIN { printf(\"%u\", *kaddr(\"tsc_khz\")); exit(); }' | sed -n 2p"

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int contention = DEFAULT_CONTENTION;
int dummy_array_size = DEFAULT_DUMMY_ARRAY_SIZE;

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
            int stop;
        };
        uint8_t padding[CACHE_LINE_SIZE];
    };
} thread_data_t;

void *test(void *data)
{
    long long unsigned int t1, t2;
    int cs_count = 0, i = 0;
    dummy_array_t *arr = &dummy_array[rand() % dummy_array_size];

    thread_data_t *d = (thread_data_t *)data;
    init_lock_local(INT_MAX, &the_lock, &(local_th_data[d->id]));

    while (!d->stop)
    {
        t1 = __builtin_ia32_rdtsc();
        acquire_lock(&(local_th_data[d->id]), &the_lock);

        for (i = 0; i < dummy_array_size; i++)
        {
            arr->counter++;
            arr = arr->next;
        }

        release_lock(&(local_th_data[d->id]), &the_lock);
        t2 = __builtin_ia32_rdtsc();

        if (d->reset)
        {
            cs_count = 0;
            d->cs_time = 0;
            d->reset = 0;
        }
        cs_count++;

        d->cs_time = (d->cs_time * (cs_count - 1) + (t2 - t1)) / cs_count;

        if (!d->stop)
            cpause(contention);
    }

    free_lock_local(local_th_data[d->id]);
    return NULL;
}

/* ################################################################### *
 * MEASUREMENT
 * ################################################################### */

void measurement(thread_data_t *data, int len)
{
    static int thread_count;
    static double tmp;
    static double sum;
    static int id = 0;

    sum = 0;
    thread_count = 0;

    // Always go through all threads as they won't stop in any particular order.
    for (int i = 0; i < len; i++)
    {
        tmp = data[i].cs_time;
        if (tmp > 0 && !data[i].reset && !data[i].stop)
        {
            thread_count++;
            sum += tmp;
            data[i].reset = 1;
        }
    }

    printf("%d, %d, %f\n", id++, thread_count, thread_count == 0 ? .0 : sum / thread_count / frequency);
}

/* ################################################################### *
 * SETUP
 * ################################################################### */

int main(int argc, char **argv)
{
    int i, c;

    int base_threads = DEFAULT_BASE_THREADS;
    int max_threads = DEFAULT_MAX_THREADS;
    int step_duration = DEFAULT_STEP_DURATION_MS;
#ifdef USE_HYBRIDLOCK_LOCKS
    int switch_thread_count = DEFAULT_SWITCH_THREAD_COUNT;
#endif

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"base-threads", required_argument, NULL, 'b'},
        {"contention", required_argument, NULL, 'c'},
        {"step-duration", required_argument, NULL, 'd'},
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
            printf("  -c, --contention <int>\n");
            printf("        Compute delay between critical sections, in cycles (default=" XSTR(DEFAULT_CONTENTION) ")\n");
            printf("  -d, --step-duration <int>\n");
            printf("        Duration of a step (measurement of a thread count) (default=" XSTR(DEFAULT_STEP_DURATION_MS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Maximum number of threads (default=" XSTR(DEFAULT_MAX_THREADS) ")\n");
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
            contention = atoi(optarg);
            break;
        case 'd':
            step_duration = atoi(optarg);
            break;
        case 'n':
            max_threads = atoi(optarg);
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
    printf("Max nb threads: %d\n", max_threads);
    printf("Step duration: %d\n", step_duration);
    printf("Contention: %d\n", contention);
    printf("Cache lines: %d\n", dummy_array_size);
#ifdef USE_HYBRIDLOCK_LOCKS
    printf("Switch thread count: %d\n", switch_thread_count);
#endif

    thread_data_t *data;
    pthread_t *threads;

    if ((data = (thread_data_t *)malloc(max_threads * sizeof(thread_data_t))) == NULL)
    {
        perror("malloc data");
        exit(1);
    }
    if ((threads = (pthread_t *)malloc(max_threads * sizeof(pthread_t))) == NULL)
    {
        perror("malloc threads");
        exit(1);
    }
    if ((local_th_data = (lock_local_data *)malloc(max_threads * sizeof(lock_local_data))) == NULL)
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
    arr->next = dummy_array;

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
    printf("Frequency: %ld\n", frequency);

    /* Init locks */
    DPRINT("Initializing locks\n");
    init_lock_global_nt(max_threads, &the_lock);

    for (i = 0; i < max_threads; i++)
    {
        data[i].id = i;
        data[i].cs_time = 0;
        data[i].reset = 1;
        data[i].stop = 0;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    struct timespec step_timeout;
    step_timeout.tv_sec = step_duration / 1000;
    step_timeout.tv_nsec = (step_duration % 1000) * 1000000;

    for (i = 0; i < 2 * max_threads + 10; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (i == switch_thread_count)
        {
            DPRINT("Switching to FUTEX hybrid lock\n");
            __sync_val_compare_and_swap(the_lock.lock_state,
                                        LOCK_STABLE(LOCK_TYPE_CLH),
                                        LOCK_TRANSITION(LOCK_TYPE_CLH, LOCK_TYPE_FUTEX));
        }
        else if (switch_thread_count > 0 && i == max_threads + 5)
        {
            DPRINT("Switching to CLH hybrid lock\n");
            __sync_val_compare_and_swap(the_lock.lock_state,
                                        LOCK_STABLE(LOCK_TYPE_FUTEX),
                                        LOCK_TRANSITION(LOCK_TYPE_FUTEX, LOCK_TYPE_CLH));
        }
#endif

        if (i < max_threads)
        {
            DPRINT("Creating thread %d\n", i);
            if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
            {
                fprintf(stderr, "Error creating thread\n");
                exit(1);
            }
        }
        else if (i >= max_threads + 10)
        {
            do
            {
                c = rand() % max_threads;
            } while (data[c].stop == 1);
            DPRINT("Stopping thread %d\n", c);
            data[c].stop = 1;
        }

        if (i >= base_threads - 1 && i < 2 * max_threads + 10 - base_threads)
        {
            if (i >= max_threads)
                continue;
            nanosleep(&step_timeout, NULL);
            measurement(data, max_threads);
        }
    }
    pthread_attr_destroy(&attr);

    DPRINT("Joining threads\n");
    /* Wait for thread completion */
    for (i = 0; i < max_threads; i++)
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
