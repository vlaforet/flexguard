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

#ifdef BPF
#include "hybridlock.skel.h"
#endif

static void futex_wait(void *addr, int val)
{
    int ret = 0;
    // Wait if *addr == val.
    while ((ret = syscall(SYS_futex, addr, FUTEX_WAIT, val, NULL, 0, 0)) != 0)
        if (ret == -1 && errno != EINTR)
        {
            if (errno != EAGAIN)
                fprintf(stderr, "Failed to futex_wait, errno: %d\n", errno);
            break;
        }
}

static void futex_wake(void *addr, int nb_threads)
{
    int ret = 0;
    while ((ret = syscall(SYS_futex, addr, FUTEX_WAKE, nb_threads, NULL, NULL, 0)) == -1 &&
           errno == EINTR)
        ;

    if (ret == -1)
        fprintf(stderr, "Failed to futex_wake, errno: %d\n", errno);
}

int hybridlock_trylock(hybridlock_lock_t *the_lock, mcs_qnode_ptr my_qnode)
{
    my_qnode->next = NULL;
    if (CAS_PTR(the_lock->data.mcs_lock, NULL, my_qnode) == NULL)
        return 0;
    return 1;
}

void hybridlock_lock(hybridlock_lock_t *the_lock, mcs_qnode_ptr my_qnode)
{
    my_qnode->next = NULL;
    my_qnode->locking = 1;

    mcs_qnode_ptr pred = (mcs_qnode *)SWAP_PTR((volatile void *)the_lock->data.mcs_lock, (void *)my_qnode);

    if (pred == NULL) /* lock was free */
        return;

    my_qnode->waiting = 1; // word on which to spin
    MEM_BARRIER;
    pred->next = my_qnode; // make pred point to me

    while (my_qnode->waiting != 0)
    {
        if (the_lock->data.spinning)
            PAUSE;
        else
            futex_wait((void *)&my_qnode->waiting, 1);
    }
}

void hybridlock_unlock(hybridlock_lock_t *the_lock, mcs_qnode_ptr my_qnode)
{
    mcs_qnode_ptr succ;
    if (!(succ = my_qnode->next)) /* I seem to have no succ. */
    {
        /* try to fix global pointer */
        if (CAS_PTR(the_lock->data.mcs_lock, my_qnode, NULL) == my_qnode)
        {
            my_qnode->locking = 0;
            return;
        }
        do
        {
            succ = my_qnode->next;
            PAUSE;
        } while (!succ); // wait for successor
    }
    succ->waiting = 0;
    futex_wake((void *)&succ->waiting, 1);
    my_qnode->locking = 0;
}

int is_free_hybridlock(hybridlock_lock_t *the_lock)
{
    if ((*the_lock->data.mcs_lock) == NULL)
        return 1;
    return 0;
}

/*
   Some methods for easy lock array manipulation
   */

hybridlock_lock_t *init_hybridlock_array_global(uint32_t size)
{
    hybridlock_lock_t *the_locks = (hybridlock_lock_t *)malloc(size * sizeof(hybridlock_lock_t));
    for (uint32_t i = 0; i < size; i++)
    {
        the_locks[i].data.mcs_lock = (mcs_lock *)malloc(sizeof(mcs_lock));
        *(the_locks[i].data.mcs_lock) = 0;

        the_locks[i].data.spinning = 1;
    }

    MEM_BARRIER;
    return the_locks;
}

hybridlock_local_params *init_hybridlock_array_local(uint32_t thread_num, uint32_t size)
{
    set_cpu(thread_num);

    hybridlock_local_params *local_params = (hybridlock_local_params *)malloc(size * sizeof(mcs_qnode *));
    MEM_BARRIER;
    return local_params;
}

void end_hybridlock_array_local(hybridlock_local_params *local_params)
{
    free(local_params);
}

void end_hybridlock_array_global(hybridlock_lock_t *the_locks, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        free(the_locks[i].data.mcs_lock);
    free(the_locks);
}

#ifdef BPF
static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    DPRINT(format, args);
    return 0;
}

static int get_max_pid()
{
    int max_pid;
    FILE *f;

    f = fopen("/proc/sys/kernel/pid_max", "r");
    if (!f)
        return -1;
    if (fscanf(f, "%d\n", &max_pid) != 1)
        max_pid = -1;
    fclose(f);
    return max_pid;
}
#endif

int init_hybridlock_global(hybridlock_lock_t *the_lock)
{
    // Initialize all lock objects
    the_lock->data.mcs_lock = (mcs_lock *)malloc(sizeof(mcs_lock));
    *(the_lock->data.mcs_lock) = 0;

    the_lock->data.spinning = 1;

#ifdef BPF
    struct hybridlock_bpf *skel;
    int err;

    // Open BPF skeleton
    libbpf_set_print(libbpf_print_fn);
    skel = hybridlock_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    // Set max size of map to max pid
    int max_pid = get_max_pid();
    if (max_pid < 0)
    {
        fprintf(stderr, "Failed to get max_pid\n");
        return 1;
    }
    bpf_map__set_max_entries(skel->maps.nodes_map, max_pid);

    // Set pointer to spinning variable for BPF
    skel->bss->input_spinning = &the_lock->data.spinning;

    // Load BPF skeleton
    err = hybridlock_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }

    // Store map
    the_lock->data.nodes_map = skel->maps.nodes_map;

    // Attach BPF skeleton
    err = hybridlock_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        hybridlock_bpf__destroy(skel);
        return 1;
    }
#endif

    MEM_BARRIER;
    return 0;
}

int init_hybridlock_local(uint32_t thread_num, hybridlock_local_params *my_qnode, hybridlock_lock_t *the_lock)
{
    set_cpu(thread_num);

    (*my_qnode) = malloc(sizeof(mcs_qnode));

    (*my_qnode)->locking = 0;
    (*my_qnode)->waiting = 0;

#ifdef BPF
    // Register thread in BPF map
    __u32 tid = gettid();
    int err = bpf_map__update_elem(the_lock->data.nodes_map, &tid, sizeof(tid), my_qnode, sizeof(mcs_qnode *), BPF_ANY);
    if (err)
    {
        fprintf(stderr, "Failed to register thread with BPF: %d\n", err);
        return 1;
    }
#endif

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
