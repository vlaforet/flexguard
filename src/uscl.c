
// From https://github.com/scheduler-cooperative-locks/proportional-share/blob/master/u-scl/fairlock.h
// Paper: https://research.cs.wisc.edu/adsl/Publications/eurosys20-scl.pdf
// GCC-VERSION - Not tested for -O3 flag for GCC version above 5.

#include "uscl.h"

static inline uscl_qnode_t *flqnode(uscl_lock_t *lock)
{
  return (uscl_qnode_t *)((char *)&lock->qnext - offsetof(uscl_qnode_t, next));
}

int uscl_init(uscl_lock_t *lock)
{
  int rc;

  lock->qtail = NULL;
  lock->qnext = NULL;
  lock->total_weight = 0;
  lock->slice = 0;
  lock->slice_valid = 0;
  if (0 != (rc = pthread_key_create(&lock->flthread_info_key, NULL)))
  {
    return rc;
  }
  return 0;
}

static flthread_info_t *flthread_info_create(uscl_lock_t *lock, int weight)
{
  flthread_info_t *info;
  info = malloc(sizeof(flthread_info_t));
  info->banned_until = getticks();
  if (weight == 0)
  {
    int prio = getpriority(PRIO_PROCESS, 0);
    weight = prio_to_weight[prio + 20];
  }
  info->weight = weight;
  __sync_add_and_fetch(&lock->total_weight, weight);
  info->banned = 0;
  info->slice = 0;
  info->start_ticks = 0;
#ifdef DEBUG
  memset(&info->stat, 0, sizeof(stats_t));
  info->stat.start = info->banned_until;
#endif
  return info;
}

void uscl_lock_thread_init(uscl_lock_t *lock, int weight)
{
  flthread_info_t *info;
  info = (flthread_info_t *)pthread_getspecific(lock->flthread_info_key);
  if (NULL != info)
  {
    free(info);
  }
  info = flthread_info_create(lock, weight);
  pthread_setspecific(lock->flthread_info_key, info);
}

void uscl_destroy(uscl_lock_t *lock)
{
  // return pthread_key_delete(lock->flthread_info_key);
}

void uscl_lock(uscl_lock_t *lock)
{
  flthread_info_t *info;
  unsigned long long now;

  info = (flthread_info_t *)pthread_getspecific(lock->flthread_info_key);
  if (NULL == info)
  {
    info = flthread_info_create(lock, 0);
    pthread_setspecific(lock->flthread_info_key, info);
  }

  if (readvol(lock->slice_valid))
  {
    unsigned long long curr_slice = lock->slice;
    // If owner of current slice, try to reenter at the beginning of the queue
    if (curr_slice == info->slice && (now = getticks()) < curr_slice)
    {
      uscl_qnode_t *succ = readvol(lock->qnext);
      if (NULL == succ)
      {
        if (__sync_bool_compare_and_swap(&lock->qtail, NULL, flqnode(lock)))
          goto reenter;
        spin_then_yield(SPIN_LIMIT, (now = getticks()) < curr_slice && NULL == (succ = readvol(lock->qnext)));
#ifdef DEBUG
        info->stat.own_slice_wait += getticks() - now;
#endif
        // let the succ invalidate the slice, and don't need to wake it up because slice expires naturally
        if (now >= curr_slice)
          goto begin;
      }
      // if state < RUNNABLE, it won't become RUNNABLE unless someone releases lock,
      // but as no one is holding the lock, there is no race
      if (succ->state < RUNNABLE || __sync_bool_compare_and_swap(&succ->state, RUNNABLE, NEXT))
      {
      reenter:
#ifdef DEBUG
        info->stat.reenter++;
#endif
        info->start_ticks = now;
        return;
      }
    }
  }
begin:

  if (info->banned)
  {
    if ((now = getticks()) < info->banned_until)
    {
      unsigned long long banned_time = info->banned_until - now;
#ifdef DEBUG
      info->stat.banned_time += banned_time;
#endif
      // sleep with granularity of SLEEP_GRANULARITY us
      while (banned_time > CYCLE_PER_US * SLEEP_GRANULARITY)
      {
        struct timespec req = {
            .tv_sec = banned_time / CYCLE_PER_S,
            .tv_nsec = (banned_time % CYCLE_PER_S / CYCLE_PER_US / SLEEP_GRANULARITY) * SLEEP_GRANULARITY * 1000,
        };
        nanosleep(&req, NULL);
        if ((now = getticks()) >= info->banned_until)
          break;
        banned_time = info->banned_until - now;
      }
      // spin for the remaining (<SLEEP_GRANULARITY us)
      spin_then_yield(SPIN_LIMIT, (now = getticks()) < info->banned_until);
    }
  }

  uscl_qnode_t n = {0};
  while (1)
  {
    uscl_qnode_t *prev = readvol(lock->qtail);
    if (__sync_bool_compare_and_swap(&lock->qtail, prev, &n))
    {
      // enter the lock queue
      if (NULL == prev)
      {
        n.state = RUNNABLE;
        lock->qnext = &n;
      }
      else
      {
        if (prev == flqnode(lock))
        {
          n.state = NEXT;
          prev->next = &n;
        }
        else
        {
          prev->next = &n;
          // wait until we become the next runnable
#ifdef DEBUG
          now = getticks();
#endif
          do
          {
            futex_wait(&n.state, INIT);
          } while (INIT == readvol(n.state));
#ifdef DEBUG
          info->stat.next_runnable_wait += getticks() - now;
#endif
        }
      }
      // invariant: n.state >= NEXT

      // wait until the current slice expires
      int slice_valid;
      unsigned long long curr_slice;
      while ((slice_valid = readvol(lock->slice_valid)) && (now = getticks()) + SLEEP_GRANULARITY < (curr_slice = readvol(lock->slice)))
      {
        unsigned long long slice_left = curr_slice - now;
        struct timespec timeout = {
            .tv_sec = 0, // slice will be less then 1 sec
            .tv_nsec = (slice_left / (CYCLE_PER_US * SLEEP_GRANULARITY)) * SLEEP_GRANULARITY * 1000,
        };
        futex_wait_timeout(&lock->slice_valid, 0, &timeout);
#ifdef DEBUG
        info->stat.prev_slice_wait += getticks() - now;
#endif
      }
      if (slice_valid)
      {
        spin_then_yield(SPIN_LIMIT, (slice_valid = readvol(lock->slice_valid)) && getticks() < readvol(lock->slice));
        if (slice_valid)
          lock->slice_valid = 0;
      }
      // invariant: getticks() >= curr_slice && lock->slice_valid == 0

#ifdef DEBUG
      now = getticks();
#endif
      // spin until RUNNABLE and try to grab the lock
      spin_then_yield(SPIN_LIMIT, RUNNABLE != readvol(n.state) || 0 == __sync_bool_compare_and_swap(&n.state, RUNNABLE, RUNNING));
      // invariant: n.state == RUNNING
#ifdef DEBUG
      info->stat.runnable_wait += getticks() - now;
#endif

      // record the successor in the lock so we can notify it when we release
      uscl_qnode_t *succ = readvol(n.next);
      if (NULL == succ)
      {
        lock->qnext = NULL;
        if (0 == __sync_bool_compare_and_swap(&lock->qtail, &n, flqnode(lock)))
        {
          spin_then_yield(SPIN_LIMIT, NULL == (succ = readvol(n.next)));
#ifdef DEBUG
          info->stat.succ_wait += getticks() - now;
#endif
          lock->qnext = succ;
        }
      }
      else
      {
        lock->qnext = succ;
      }
      // invariant: NULL == succ <=> lock->qtail == flqnode(lock)

      now = getticks();
      info->start_ticks = now;
      info->slice = now + FAIRLOCK_GRANULARITY;
      lock->slice = info->slice;
      lock->slice_valid = 1;
      // wake up successor if necessary
      if (succ)
      {
        succ->state = NEXT;
        futex_wake(&succ->state, 1);
      }
      return;
    }
  }
}

void uscl_unlock(uscl_lock_t *lock)
{
  unsigned long long now, cs;
#ifdef DEBUG
  ull succ_start = 0, succ_end = 0;
#endif
  flthread_info_t *info;

  uscl_qnode_t *succ = lock->qnext;
  if (NULL == succ)
  {
    if (__sync_bool_compare_and_swap(&lock->qtail, flqnode(lock), NULL))
      goto accounting;
#ifdef DEBUG
    succ_start = getticks();
#endif
    spin_then_yield(SPIN_LIMIT, NULL == (succ = readvol(lock->qnext)));
#ifdef DEBUG
    succ_end = getticks();
#endif
  }
  succ->state = RUNNABLE;

accounting:
  // invariant: NULL == succ || succ->state = RUNNABLE
  info = (flthread_info_t *)pthread_getspecific(lock->flthread_info_key);
  now = getticks();
  cs = now - info->start_ticks;
  info->banned_until += cs * (__atomic_load_n(&lock->total_weight, __ATOMIC_RELAXED) / info->weight);
  info->banned = now < info->banned_until;

  if (info->banned)
  {
    if (__sync_bool_compare_and_swap(&lock->slice_valid, 1, 0))
    {
      futex_wake(&lock->slice_valid, 1);
    }
  }
#ifdef DEBUG
  info->stat.release_succ_wait += succ_end - succ_start;
#endif
}

/*
 *  Condition Variables
 */

int uscl_cond_init(uscl_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}

int uscl_cond_wait(uscl_cond_t *cond, uscl_lock_t *the_lock)
{
  // No need for atomic operations, I have the lock
  uint32_t target = ++cond->target;
  uint32_t seq = cond->seq;
  uscl_unlock(the_lock);

  while (target > seq)
  {
    futex_wait(&cond->seq, seq);
    seq = cond->seq;
  }
  uscl_lock(the_lock);
  return 0;
}

int uscl_cond_timedwait(uscl_cond_t *cond, uscl_lock_t *the_lock, const struct timespec *ts)
{
  fprintf(stderr, "Timedwait not supported yet.\n");
  exit(EXIT_FAILURE);
}

int uscl_cond_signal(uscl_cond_t *cond)
{
  cond->seq++;
  futex_wake(&cond->seq, 1);
  return 0;
}

int uscl_cond_broadcast(uscl_cond_t *cond)
{
  cond->seq = cond->target;
  futex_wake(&cond->seq, INT_MAX);
  return 0;
}

int uscl_cond_destroy(uscl_cond_t *cond)
{
  cond->seq = 0;
  cond->target = 0;
  return 0;
}