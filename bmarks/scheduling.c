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

#define DEFAULT_NB_THREADS 1
#define DEFAULT_USE_LOCKS 1
#define DEFAULT_LAUNCH_DELAY 1000000000
#define DEFAULT_CS_DELAY 1000000000

#define XSTR(s) STR(s)
#define STR(s) #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */
int use_locks;
int cs_delay;
int stop = 0;

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
    phys_id = the_cores[d->id];

    /* local initialization of locks */
    init_lock_local(phys_id, &the_lock, &(local_th_data[d->id]));
    printf("Starting thread %d\n", d->id);

    while (!stop)
    {
        acquire_write(&(local_th_data[d->id]), &the_lock);
        cpause(cs_delay);
        release_write(&(local_th_data[d->id]), &the_lock);

        cpause(cs_delay);
    }

    free_lock_local(local_th_data[d->id]);
    return NULL;
}

void catcher(int sig)
{
    static int nb = 0;

#ifdef USE_HYBRIDLOCK_LOCKS
    if (nb == 0)
    {
        printf("Hybrid Lock: Blocking\n");
        set_blocking(&the_lock);
        nb++;
        return;
    }
#endif

    printf("CAUGHT SIGNAL %d\n", sig);
    if (++nb >= 3)
        exit(1);
}

int main(int argc, char **argv)
{
    set_cpu(the_cores[0]);
    struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"cs-delay", required_argument, NULL, 'c'},
        {"launch-delay", required_argument, NULL, 'd'},
        {"use-locks", required_argument, NULL, 'l'},
        {"num-threads", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0}};

    int i, c;
    thread_data_t *data;
    pthread_t *threads;
    pthread_attr_t attr;
    int nb_threads = DEFAULT_NB_THREADS;
    use_locks = DEFAULT_USE_LOCKS;
    int launch_delay = DEFAULT_LAUNCH_DELAY;

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
            printf("  -c, --cs-delay <int>\n");
            printf("        Critical section delay in cycles (default=" XSTR(DEFAULT_CS_DELAY) ")\n");
            printf("  -d, --launch-delay <int>\n");
            printf("        Delay between thread creations in cycles (default=" XSTR(DEFAULT_LAUNCH_DELAY) ")\n");
            printf("  -l, --use-locks <int>\n");
            printf("        Use locks or not (default=" XSTR(DEFAULT_USE_LOCKS) ")\n");
            printf("  -n, --num-threads <int>\n");
            printf("        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n");
        case 'c':
            cs_delay = atoi(optarg);
            break;
        case 'd':
            launch_delay = atoi(optarg);
            break;
        case 'l':
            use_locks = atoi(optarg);
            break;
        case 'n':
            nb_threads = atoi(optarg);
            break;
        case '?':
            printf("Use -h or --help for help\n");
            exit(0);
        default:
            exit(1);
        }
    }

    printf("Nb threads : %d\n", nb_threads);
    printf("Use locks  : %d\n", use_locks);

    if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL)
    {
        perror("malloc");
        exit(1);
    }
    if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL)
    {
        perror("malloc");
        exit(1);
    }

    local_th_data = (lock_local_data *)malloc(nb_threads * sizeof(lock_local_data));

    /* Catch some signals */
    if (signal(SIGHUP, catcher) == SIG_ERR ||
        signal(SIGINT, catcher) == SIG_ERR ||
        signal(SIGTERM, catcher) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    /* Init locks */
#ifdef PRINT_OUTPUT
    printf("Initializing locks\n");
#endif
    init_lock_global_nt(nb_threads, &the_lock);

    /* Access set from all threads */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < nb_threads; i++)
    {
#ifdef PRINT_OUTPUT
        printf("Creating thread %d\n", i);
#endif
        data[i].id = i;
        if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0)
        {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        cpause(launch_delay);
    }
    pthread_attr_destroy(&attr);

    cpause(launch_delay * 10);
    stop = 1;

    /* Wait for thread completion */
    for (i = 0; i < nb_threads; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    return EXIT_SUCCESS;
}
