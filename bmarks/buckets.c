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
#include <assert.h>
#include <sched.h>
#include "atomic_ops.h"
#include "utils.h"
#include "lock_if.h"
#include "hash_map.h"
#ifdef USE_HYBRIDLOCK_LOCKS
#include "hybridlock.h"
#endif

#define DEFAULT_MAX_THREADS 10
#define DEFAULT_DURATION_MS 10000
#define DEFAULT_MAX_VALUE 100000
#define DEFAULT_BUCKET_COUNT 100
#define DEFAULT_OFFSET_CHANGES 40
#define DEFAULT_TRACING false
#define DEFAULT_NON_CRITICAL_CYCLES 0
#define DEFAULT_PIN_THREADS false

#define XSTR(s) STR(s)
#define STR(s) #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int bucket_count = DEFAULT_BUCKET_COUNT;
int max_value = DEFAULT_MAX_VALUE;
int value_offset = 0;
bool tracing = false;
int non_critical_cycles = DEFAULT_NON_CRITICAL_CYCLES;
bool pin_threads = DEFAULT_PIN_THREADS;

typedef struct bucket_t
{
    int id;
    libslock_t lock;
    hash_map map;

    int reads;
    int writes;
    int successful_reads;
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
    int zipf_value = 0;              // Computed exponential value to be returned
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

#ifdef TRACING
void lock_tracing_fn(ticks rtsp, int event_type, void *event_data, void *fn_data)
{
    bucket_t *bucket = (bucket_t *)fn_data;

#ifdef USE_HYBRIDLOCK_LOCKS
    if (event_type == TRACING_EVENT_SWITCH_SPIN || event_type == TRACING_EVENT_SWITCH_BLOCK)
        printf("%s, %ld, %d\n", event_type == TRACING_EVENT_SWITCH_SPIN ? "switch_spin" : "switch_block", rtsp, bucket->id);
#endif
}
#endif

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
            ticks cs_ticks;
            int cs_count;

            volatile bool *stop;
            volatile bool *start;
        };
        uint8_t padding[CACHE_LINE_SIZE];
    };
} thread_data_t;

void *test(void *data)
{
    ticks t1;
    int i = 0, bucket_id = 0, *value;
    thread_data_t *d = (thread_data_t *)data;

    if (pin_threads)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(1 + (d->id) % (63), &mask);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
        {
            perror("sched_setaffinity");
            assert(false);
        }
    }

    while (!*d->start)
        futex_wait((void *)d->start, false);

    while (!*d->stop)
    {
        value = malloc(sizeof(int));
        *value = (zipf(10, max_value - 1) + value_offset) % max_value;
        bucket_id = *value * bucket_count / max_value;
        DASSERT(bucket_id < bucket_count);
        i = rand();

        t1 = getticks();
        libslock_lock(&buckets[bucket_id].lock);

        if (i % 2 == 0)
        {
            if (hash_map_put(&buckets[bucket_id].map, value, value, 0))
            {
                perror("hash_map_put");
                exit(1);
            }
            buckets[bucket_id].writes++;
        }
        else
        {
            if (hash_map_get(&buckets[bucket_id].map, value, 0))
                buckets[bucket_id].successful_reads++;
            buckets[bucket_id].reads++;
        }

        libslock_unlock(&buckets[bucket_id].lock);
        d->cs_ticks += getticks() - t1;
        d->cs_count++;

        if (tracing)
            printf("accessed_value, %ld, %d\n", t1, *value);

        if (non_critical_cycles)
            cpause(non_critical_cycles);
    }

    return NULL;
}

/* ################################################################### *
 * SETUP
 * ################################################################### */

int main(int argc, char **argv)
{
    int i, c;

    int max_threads = DEFAULT_MAX_THREADS;
    int duration = DEFAULT_DURATION_MS;
    int offset_changes = DEFAULT_OFFSET_CHANGES;

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"num-threads", required_argument, NULL, 'n'},
        {"buckets", required_argument, NULL, 'b'},
        {"max-value", required_argument, NULL, 'm'},
        {"offset-changes", required_argument, NULL, 'o'},
        {"trace", no_argument, NULL, 't'},
        {"non-critical-cycles", required_argument, NULL, 'c'},
        {"pin-threads", required_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hd:n:b:m:o:tc:p:", long_options, &i);

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
            printf("  -d, --duration <int>\n");
            printf("        Duration of the experiment in ms (default=" XSTR(DEFAULT_DURATION_MS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Maximum number of threads (default=" XSTR(DEFAULT_MAX_THREADS) ")\n");
            printf("  -b, --buckets <int>\n");
            printf("        Number of buckets (default=" XSTR(DEFAULT_BUCKET_COUNT) ")\n");
            printf("  -m, --max-value <int>\n");
            printf("        Maximum value (default=" XSTR(DEFAULT_MAX_VALUE) ")\n");
            printf("  -o, --offset-changes <int>\n");
            printf("        Number of time to change the offset (default=" XSTR(DEFAULT_OFFSET_CHANGES) ")\n");
            printf("  -c, --non-critical-cycles <int>\n");
            printf("        Number of cycles between critical sections (default=" XSTR(DEFAULT_NON_CRITICAL_CYCLES) ")\n");
            printf("  -p, --pin-threads <int>\n");
            printf("        Enable thread pinning (default=" XSTR(DEFAULT_PIN_THREADS) ")\n");
            printf("  -t, --trace\n");
            printf("        Enable tracing (default=" XSTR(DEFAULT_TRACING) ")\n");
#ifndef TRACING
            printf("        Lock tracing is disabled. If you use that option only the benchmark will be traced.\n");
            printf("        Recompile with TRACING=1 to enable lock tracing.\n");
#endif
            exit(0);
        case 'd':
            duration = atoi(optarg);
            break;
        case 'n':
            max_threads = atoi(optarg);
            break;
        case 'b':
            bucket_count = atoi(optarg);
            break;
        case 'm':
            max_value = atoi(optarg);
            break;
        case 'o':
            offset_changes = atoi(optarg);
            break;
        case 'c':
            non_critical_cycles = atoi(optarg);
            break;
        case 'p':
            pin_threads = atoi(optarg) != 0;
            break;
        case 't':
            tracing = true;
#ifndef TRACING
            printf("#Warning: Lock tracing is disabled. Only the benchmark will be traced.\n");
            printf("#         Recompile with TRACING=1 to enable lock tracing.\n\n");
#endif
            break;
        case '?':
            printf("Use -h or --help for help\n");
            exit(0);
        default:
            exit(1);
        }
    }

    printf("#Duration: %dms\n", duration);
    printf("#Threads: %d\n", max_threads);
    printf("#Buckets: %d\n", bucket_count);
    printf("#Max value: %d\n", max_value);
    printf("#Offset changes: %d\n", offset_changes);
    printf("#Non critical cycles: %d\n", non_critical_cycles);
    printf("#Thread pinning: %s\n", pin_threads ? "enabled" : "disabled");
    printf("#Tracing: %s\n", tracing ? "enabled" : "disabled");
    printf("#TSC frequency: %ld\n", get_tsc_frequency());

    if (pin_threads)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(0, &mask);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
        {
            perror("sched_setaffinity");
            assert(false);
        }
    }

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
    zipf(10, max_value - 1);

    // Initialize buckets
    buckets = malloc(sizeof(bucket_t) * bucket_count);
    for (i = 0; i < bucket_count; i++)
    {
        buckets[i].id = i;
        libslock_init(&buckets[i].lock);

#if defined(TRACING)
        if (tracing)
            set_tracing_fn(&buckets[i].lock, &lock_tracing_fn, &buckets[i]);
#endif

        if (hash_map_init(&buckets[i].map, &hash_fn, &eq_fn, max_value / bucket_count, 0.75))
        {
            perror("hash_map_init");
            exit(1);
        }
        buckets[i].reads = 0;
        buckets[i].writes = 0;
        buckets[i].successful_reads = 0;
    }

    volatile bool start = false;
    volatile bool stop = false;

    for (i = 0; i < max_threads; i++)
    {
        data[i].id = i;
        data[i].cs_ticks = 0;
        data[i].cs_count = 0;

        data[i].stop = &stop;
        data[i].start = &start;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Creating threads
    for (i = 0; i < max_threads; i++)
    {
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
        {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }

    nanosleep((const struct timespec[]){{1, 0L}}, NULL); // Wait for all threads to be ready.
    const struct timespec timeout = {duration / offset_changes / 1000, ((duration / offset_changes) % 1000) * 1000000L};

    DPRINT("Starting experiment\n");
    start = true;
    futex_wake((void *)&start, INT_MAX);

    for (i = 0; i < offset_changes; i++)
    {
        value_offset = rand() % max_value;
        nanosleep(&timeout, NULL);
    }

    stop = true;
    DPRINT("Stopped experiment\n");

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

    for (i = 0; i < bucket_count; i++)
        printf("#Bucket %4d: %10d / %11d successful reads, %11d writes\n",
               i, buckets[i].successful_reads, buckets[i].reads, buckets[i].writes);

    double sum = 0;
    double local_result = 0;
    for (i = 0; i < max_threads; i++)
    {
        local_result = (double)data[i].cs_ticks / (double)data[i].cs_count;
        sum += local_result;

        printf("#Local result for Thread %3d: %10f CS/s (%d iterations)\n", i,
               ((double)1000 * get_tsc_frequency()) / local_result, data[i].cs_count);
    }

    double result = max_threads * (((double)1000 * get_tsc_frequency()) / sum);
    printf("#Throughput: %f CS/s\n", result);

    for (i = 0; i < bucket_count; i++)
        libslock_destroy(&buckets[i].lock);

    return EXIT_SUCCESS;
}
