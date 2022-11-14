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
#include "hybridlock.h"

#define DEFAULT_NB_THREADS 10
#define DEFAULT_USE_LOCKS 1
#define DEFAULT_LAUNCH_DELAY_MS 1000
#define DEFAULT_CS_CYCLES 10000

#define XSTR(s) STR(s)
#define STR(s) #s

#define DURATION(t1, t2) (t2.tv_sec * 1000 + t2.tv_usec / 1000) - (t1.tv_sec * 1000 + t1.tv_usec / 1000)

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int use_locks = DEFAULT_USE_LOCKS;
int cs_cycles;

struct timeval start;
unsigned long cs_count = 0;
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
        };
        uint8_t padding[CACHE_LINE_SIZE];
    };
} thread_data_t;

void *test(void *data)
{
    thread_data_t *d = (thread_data_t *)data;
    init_lock_local(INT_MAX, &the_lock, &(local_th_data[d->id]));
    thread_count++;
    int stop = 0;

    while (!stop)
    {
        if (use_locks)
            acquire_write(&(local_th_data[d->id]), &the_lock);

        cs_count++;
        cpause(cs_cycles);

        if (thread_count > needed_threads)
        {
            thread_count--;
            stop = 1;
        }

        if (use_locks)
            release_write(&(local_th_data[d->id]), &the_lock);

        cpause(cs_cycles);
    }

    free_lock_local(local_th_data[d->id]);
    return NULL;
}

void measurement()
{
    static unsigned long last_cs_count = 0;
    static struct timeval last_measurement, current;
    static unsigned int diff;
    static double duration;

    if (thread_count <= 0)
        return;

    if (last_measurement.tv_sec == 0 && last_measurement.tv_usec == 0)
        last_measurement = start;

    gettimeofday(&current, NULL);
    duration = DURATION(last_measurement, current);
    diff = cs_count - last_cs_count;

    last_measurement = current;
    last_cs_count += diff;

    if (diff > 0)
        printf("%d, %f, %f\n", thread_count, (double)DURATION(start, current), diff / duration);
}

int main(int argc, char **argv)
{
    int i, c;
    int max_nb_threads = DEFAULT_NB_THREADS;
    int launch_delay = DEFAULT_LAUNCH_DELAY_MS;

    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"cs-cycles", required_argument, NULL, 'c'},
        {"launch-delay", required_argument, NULL, 'd'},
        {"use-locks", required_argument, NULL, 'l'},
        {"num-threads", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0}};

    while (1)
    {
        i = 0;
        c = getopt_long(argc, argv, "hc:d:l:n:", long_options, &i);

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
            printf("  -c, --cs-cycles <int>\n");
            printf("        Critical section length in cycles (default=" XSTR(DEFAULT_CS_CYCLES) ")\n");
            printf("  -d, --launch-delay <int>\n");
            printf("        Delay between thread creations in milliseconds (default=" XSTR(DEFAULT_LAUNCH_DELAY_MS) ")\n");
            printf("  -l, --use-locks <int>\n");
            printf("        Use locks or not (default=" XSTR(DEFAULT_USE_LOCKS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n");
        case 'c':
            cs_cycles = atoi(optarg);
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
        if (i == 48)
            set_blocking(&the_lock, 1);
#endif

        DPRINT("Creating thread %d\n", i);

        data[i].id = i;
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
        {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        measurement();
        nanosleep(&launch_timeout, NULL);
    }
    pthread_attr_destroy(&attr);

    for (i = 0; i < 10; i++)
    {
#ifdef USE_HYBRIDLOCK_LOCKS
        if (i == 5)
            set_blocking(&the_lock, 0);
#endif

        measurement();
        nanosleep(&launch_timeout, NULL);
    }

    while (needed_threads > 0)
    {
        if (thread_count == needed_threads)
            needed_threads--;

        measurement();
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
