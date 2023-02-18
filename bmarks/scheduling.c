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

#define DEFAULT_NB_THREADS 10
#define DEFAULT_USE_LOCKS 1
#define DEFAULT_LAUNCH_DELAY_MS 1000
#define DEFAULT_COMPUTE_CYCLES 10000
#define DEFAULT_SWITCH_THREAD_COUNT 48

#define XSTR(s) STR(s)
#define STR(s) #s

#define DURATION(t1, t2) (t2.tv_sec * 1000 + t2.tv_usec / (double)1000) - (t1.tv_sec * 1000 + t1.tv_usec / (double)1000)

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int use_locks = DEFAULT_USE_LOCKS;
int compute_cycles;

struct timeval start;
_Atomic int thread_count = 0;
int needed_threads;

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
    int stop = 0;
    int cs_count = 0;

    thread_data_t *d = (thread_data_t *)data;
    init_lock_local(INT_MAX, &the_lock, &(local_th_data[d->id]));
    thread_count++;

    while (!stop)
    {
        gettimeofday(&t1, NULL);
        if (use_locks)
            acquire_write(&(local_th_data[d->id]), &the_lock);

        if (thread_count > needed_threads)
        {
            thread_count--;
            stop = 1;
        }

        if (use_locks)
            release_write(&(local_th_data[d->id]), &the_lock);

        if (d->reset)
        {
            cs_count = 0;
            d->cs_time = 0;
            d->reset = 0;
        }
        cs_count++;
        gettimeofday(&t2, NULL);
        d->cs_time = (d->cs_time * (cs_count - 1) + DURATION(t1, t2)) / cs_count;

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
    static double sum = 0;

    sum = 0;
    tc = thread_count;

    if (tc <= 0)
        return;

    if (last_measurement.tv_sec == 0 && last_measurement.tv_usec == 0)
        last_measurement = start;
    gettimeofday(&current, NULL);

    // Always go through all threads as they won't stop in any particular order.
    for (int i = 0; i < len; i++)
    {
        tmp = data[i].cs_time;
        if (tmp > 0)
        {
            sum += tmp;
            data[i].reset = 1;
        }
    }

    printf("%d, %f, %f\n", tc, DURATION(start, current), sum / tc);
}

int main(int argc, char **argv)
{
    int i, c;
    int max_nb_threads = DEFAULT_NB_THREADS;
    int launch_delay = DEFAULT_LAUNCH_DELAY_MS;
    int switch_thread_count = DEFAULT_SWITCH_THREAD_COUNT;

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"cs-cycles", required_argument, NULL, 'c'},
        {"launch-delay", required_argument, NULL, 'd'},
        {"use-locks", required_argument, NULL, 'l'},
        {"num-threads", required_argument, NULL, 'n'},
        {"switch-thread-count", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hc:d:l:n:s:", long_options, &i);

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
            printf("  -c, --compute-cycles <int>\n");
            printf("        Compute delay between critical sections, in cycles (default=" XSTR(DEFAULT_COMPUTE_CYCLES) ")\n");
            printf("  -d, --launch-delay <int>\n");
            printf("        Delay between thread creations in milliseconds (default=" XSTR(DEFAULT_LAUNCH_DELAY_MS) ")\n");
            printf("  -l, --use-locks <int>\n");
            printf("        Use locks or not (default=" XSTR(DEFAULT_USE_LOCKS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n");
            printf("  -s, --switch-thread-count <int>\n");
            printf("        Core count after which the lock will be blocking (default=" XSTR(DEFAULT_SWITCH_THREAD_COUNT) ")\n");
            printf("        A value of -1 will disable the switch (always spin) and with a value of 0 the lock will never spin.\n");
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
        case 's':
            switch_thread_count = atoi(optarg);
            break;
        case '?':
            printf("Use -h or --help for help\n");
            exit(0);
        default:
            exit(1);
        }
    }

    printf("Nb threads max : %d\n", max_nb_threads);
    printf("Use locks  : %d\n", use_locks);

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

    /* Init locks */
    DPRINT("Initializing locks\n");
    init_lock_global_nt(max_nb_threads, &the_lock);

    for (i = 0; i < max_nb_threads; i++)
    {
        data[i].id = i;
        data[i].cs_time = 0;
        data[i].reset = 0;
    }

#ifdef USE_HYBRIDLOCK_LOCKS
    if (switch_thread_count == 0)
        set_blocking(&the_lock, 1);
#endif

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
        if (switch_thread_count > 0 && i == switch_thread_count)
            set_blocking(&the_lock, 1);
#endif

        DPRINT("Creating thread %d\n", i);
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
        {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        measurement(data, max_nb_threads);
        nanosleep(&launch_timeout, NULL);
    }
    pthread_attr_destroy(&attr);

    for (i = 0; i < 10; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (switch_thread_count > 0 && i == 5)
            set_blocking(&the_lock, 0);
#endif

        measurement(data, max_nb_threads);
        nanosleep(&launch_timeout, NULL);
    }

    while (needed_threads > 0)
    {
        if (thread_count == needed_threads)
            needed_threads--;

        measurement(data, max_nb_threads);
        nanosleep(&launch_timeout, NULL);
    }

    /* Wait for thread completion */
    for (i = 0; i < max_nb_threads; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    return EXIT_SUCCESS;
}
