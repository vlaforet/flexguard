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
#define DEFAULT_USE_LOCKS 1
#define DEFAULT_LAUNCH_DELAY_MS 1000
#define DEFAULT_COMPUTE_CYCLES 100

#ifdef USE_HYBRIDLOCK_LOCKS
#define DEFAULT_SWITCH_THREAD_COUNT 48
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define DURATION(t1, t2) (t2.tv_sec * 1000 + t2.tv_usec / (double)1000) - (t1.tv_sec * 1000 + t1.tv_usec / (double)1000)

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int use_locks = DEFAULT_USE_LOCKS;
int compute_cycles = DEFAULT_COMPUTE_CYCLES;

struct timeval start;
_Atomic int thread_count = 0;
int needed_threads;

#define DUMMY_ARRAYS_SIZE CACHE_LINE_SIZE * 2

int shared_counter = 0;
uint8_t arr1[DUMMY_ARRAYS_SIZE];
uint8_t arr2[DUMMY_ARRAYS_SIZE];

__thread uint32_t phys_id;
lock_global_data the_lock;
__attribute__((aligned(CACHE_LINE_SIZE))) lock_local_data *local_th_data;

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
    struct timeval t1, t2;
    int stop = 0, cs_count = 0, last_cs_count = 0;

    thread_data_t *d = (thread_data_t *)data;
    init_lock_local(INT_MAX, &the_lock, &(local_th_data[d->id]));
    thread_count++;

    while (!stop)
    {
        if (cs_count % 100 == 0)
            gettimeofday(&t1, NULL);

        if (use_locks)
            acquire_write(&(local_th_data[d->id]), &the_lock);

        shared_counter++;
        if (shared_counter % 2 == 0)
            memcpy(arr1, arr2, DUMMY_ARRAYS_SIZE);
        else
            memcpy(arr2, arr1, DUMMY_ARRAYS_SIZE);

        if (thread_count > needed_threads)
        {
            thread_count--;
            stop = 1;
        }

        if (use_locks)
            release_write(&(local_th_data[d->id]), &the_lock);

        if (d->reset)
        {
            cs_count = 0, last_cs_count = 0;
            d->cs_time = 0;
            d->reset = 0;
        }
        cs_count++;

        if (cs_count % 100 == 0 || stop)
        {
            gettimeofday(&t2, NULL);
            d->cs_time = (d->cs_time * (last_cs_count) + DURATION(t1, t2)) / cs_count;
            last_cs_count = cs_count;
        }

        cpause(compute_cycles);
    }

    d->cs_time = 0;
    free_lock_local(local_th_data[d->id]);
    return NULL;
}

void measurement(thread_data_t *data, int len)
{
    static struct timeval last_measurement, current;
    static int tc;
    static double tmp;
    static double sum;

    sum = 0;
    tc = 0;

    if (thread_count <= 0)
        return;

    if (last_measurement.tv_sec == 0 && last_measurement.tv_usec == 0)
        last_measurement = start;
    gettimeofday(&current, NULL);

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

    if (tc == 0)
        printf("%d, %f, %f\n", tc, DURATION(start, current), .0);
    else
        printf("%d, %f, %f\n", tc, DURATION(start, current), sum / tc);
}

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
        {"use-locks", required_argument, NULL, 'l'},
        {"num-threads", required_argument, NULL, 'n'},
#ifdef USE_HYBRIDLOCK_LOCKS
        {"switch-thread-count", required_argument, NULL, 's'},
#endif
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hb:c:d:l:n:s:", long_options, &i);

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
            printf("  -l, --use-locks <int>\n");
            printf("        Use locks or not (default=" XSTR(DEFAULT_USE_LOCKS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n");
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
        case 'l':
            use_locks = atoi(optarg);
            break;
        case 'n':
            max_nb_threads = atoi(optarg);
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
    printf("Use locks: %d\n", use_locks);
    printf("Launch delay: %d\n", launch_delay);
    printf("Compute cycles: %d\n", compute_cycles);
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

    // Fill dummy arrays
    for (i = 0; i < DUMMY_ARRAYS_SIZE; i++)
    {
        arr1[i] = (DUMMY_ARRAYS_SIZE - i) % 255;
        arr2[i] = i % 255;
    }

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
    gettimeofday(&start, NULL);

    for (i = 0; i < max_nb_threads; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (i == switch_thread_count)
        {
            DPRINT("Switching to FUTEX hybrid lock\n");
            the_lock.lock_type = FUTEX;
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
            the_lock.lock_type = MCS;
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

    printf("arr1 = ");
    for (i = 0; i < DUMMY_ARRAYS_SIZE; i++)
        printf("%d ", arr1[i]);
    printf("\narr2 = ");
    for (i = 0; i < DUMMY_ARRAYS_SIZE; i++)
        printf("%d ", arr2[i]);
    printf("\n");

    free_lock_global(the_lock);

    return EXIT_SUCCESS;
}
