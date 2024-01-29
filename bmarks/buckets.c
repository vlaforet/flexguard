/*
 * File: buckets.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Benchmark storing numbers in buckets each protected by a lock.
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

#include <math.h>
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
#include "hash_map.h"

#define DEFAULT_BASE_THREADS 1
#define DEFAULT_MAX_THREADS 10
#define DEFAULT_STEP_DURATION_MS 1000
#define DEFAULT_MAX_VALUE 100000
#define DEFAULT_BUCKET_COUNT 100

#define XSTR(s) STR(s)
#define STR(s) #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int bucket_count = DEFAULT_BUCKET_COUNT;
int max_value = DEFAULT_MAX_VALUE;
int value_offset = 0;

typedef struct bucket_t
{
    lock_global_data lock;
    __attribute__((aligned(CACHE_LINE_SIZE))) lock_local_data *local_th_data;
    hash_map map;
    int usage;
} bucket_t;
__attribute__((aligned(CACHE_LINE_SIZE))) bucket_t *buckets;

/* ################################################################### *
 * Functions
 * ################################################################### */

int zipf(double alpha, int n)
{
    static double c = 0;             // Normalization constant
    static double *sum_probs = NULL; // Pre-calculated sum of probabilities
    double z;                        // Uniform random number (0 < z < 1)
    int zipf_value;                  // Computed exponential value to be returned
    int i;                           // Loop counter
    int low, high, mid;              // Binary-search bounds

    // Compute normalization constant on first call only
    if (!sum_probs)
    {
        for (i = 1; i <= n; i++)
            c = c + (1.0 / pow((double)i, alpha));
        c = 1.0 / c;

        sum_probs = malloc((n + 1) * sizeof(*sum_probs));
        sum_probs[0] = 0;
        for (i = 1; i <= n; i++)
            sum_probs[i] = sum_probs[i - 1] + c / pow((double)i, alpha);
    }

    // Pull a uniform random number (0 < z < 1)
    do
    {
        z = ((double)rand()) / RAND_MAX;

    } while ((z == 0) || (z == 1));

    // Map z to the value
    low = 1, high = n;
    do
    {
        mid = floor((low + high) / 2);
        if (sum_probs[mid] >= z && sum_probs[mid - 1] < z)
        {
            zipf_value = mid;
            break;
        }
        else if (sum_probs[mid] >= z)
            high = mid - 1;
        else
            low = mid + 1;
    } while (low <= high);

    // Assert that zipf_value is between 1 and N
    DASSERT((zipf_value >= 1) && (zipf_value <= n));
    return zipf_value;
}

unsigned long int hash_fn(void *key)
{
    return (unsigned long int)*((int *)key);
}

bool eq_fn(void *p1, void *p2)
{
    return hash_fn(p1) == hash_fn(p2);
}

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
    int cs_count = 0, i = 0, bucket_id = 0;

    thread_data_t *d = (thread_data_t *)data;

    for (i = 0; i < bucket_count; i++)
        init_lock_local(INT_MAX, &buckets[i].lock, &(buckets[i].local_th_data[d->id]));

    while (!d->stop)
    {
        int *value = malloc(sizeof(int));
        *value = (zipf(1.4, max_value - 1) + value_offset) % max_value;
        bucket_id = *value * bucket_count / max_value;
        DASSERT(bucket_id < bucket_count);

        t1 = __builtin_ia32_rdtsc();
        acquire_lock(&(buckets[bucket_id].local_th_data[d->id]), &buckets[bucket_id].lock);

        if (hash_map_put(&buckets[bucket_id].map, value, value, 0))
        {
            perror("hash_map_put");
            exit(1);
        }
        buckets[bucket_id].usage++;

        release_lock(&(buckets[bucket_id].local_th_data[d->id]), &buckets[bucket_id].lock);
        t2 = __builtin_ia32_rdtsc();

        if (d->reset)
        {
            cs_count = 0;
            d->cs_time = 0;
            d->reset = 0;
        }
        cs_count++;

        d->cs_time = (d->cs_time * (cs_count - 1) + (t2 - t1)) / cs_count;
    }

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

    printf("%d, %d, %f\n", id++, thread_count, thread_count == 0 ? .0 : sum / thread_count / get_tsc_frequency());
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

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"base-threads", required_argument, NULL, 'b'},
        {"contention", required_argument, NULL, 'c'},
        {"step-duration", required_argument, NULL, 'd'},
        {"num-threads", required_argument, NULL, 'n'},
        {"buckets", required_argument, NULL, 'l'},
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hb:c:d:n:l:", long_options, &i);

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
            printf("buckets -- lock stress test\n");
            printf("\n");
            printf("Usage:\n");
            printf("  buckets [options...]\n");
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
            printf("  -l, --buckets <int>\n");
            printf("        Number of buckets (default=" XSTR(DEFAULT_BUCKET_COUNT) ")\n");
            exit(0);
        case 'b':
            base_threads = atoi(optarg);
            break;
        case 'd':
            step_duration = atoi(optarg);
            break;
        case 'n':
            max_threads = atoi(optarg);
            break;
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
    printf("TSC frequency: %ld\n", get_tsc_frequency());

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

    // Initialize zipf
    printf("Zipf: %d\n", zipf(1.4, max_value - 1));

    // Initialize buckets
    buckets = malloc(sizeof(bucket_t) * bucket_count);
    for (i = 0; i < bucket_count; i++)
    {
        init_lock_global(&buckets[i].lock);
        if ((buckets[i].local_th_data = (lock_local_data *)malloc(max_threads * sizeof(lock_local_data))) == NULL)
        {
            perror("malloc local_th_data");
            exit(1);
        }
        if (hash_map_init(&buckets[i].map, &hash_fn, &eq_fn, max_value / bucket_count, 0.75))
        {
            perror("hash_map_init");
            exit(1);
        }
        buckets[i].usage = 0;
    }

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
        value_offset = rand();
    }
    pthread_attr_destroy(&attr);

    for (i = 0; i < bucket_count; i++)
        printf("Bucket %d: %d uses\n", i, buckets[i].usage);

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
    return EXIT_SUCCESS;
}
