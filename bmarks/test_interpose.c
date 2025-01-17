/*
 * File: test_interpose.c
 * Author: Victor Laforet <victor.laforet@inria.fr>
 *
 * Description:
 *      Test pthread functions interposition.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Victor Laforet
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

#include <getopt.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define XSTR(s) STR(s)
#define STR(s) #s

#define SUCCESS_OR_FAIL(func)                                     \
  do                                                              \
  {                                                               \
    printf("%s: ", STR(func));                                    \
    ret = func;                                                   \
    if (ret == counter)                                           \
      printf("Success\n");                                        \
    else                                                          \
    {                                                             \
      printf("Failed, counter (%d) != ret (%d)\n", counter, ret); \
      exit(EXIT_FAILURE);                                         \
    }                                                             \
    counter++;                                                    \
  } while (0)

int main(int argc, char **argv)
{
  int i, c;

  struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {NULL, 0, NULL, 0}};

  while (1)
  {
    i = 0;
    c = getopt_long(argc, argv, "h", long_options, &i);

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
      printf("test_interpose -- Test pthread functions interposition\n");
      printf("\n");
      printf("Usage:\n");
      printf("  interpose.sh test_init\n");
      printf("\n");
      printf("Options:\n");
      printf("  -h, --help\n");
      printf("        Print this message\n");
      exit(0);
    case '?':
      printf("Use -h or --help for help\n");
      exit(0);
    default:
      exit(1);
    }
  }

  printf("The test fails if it does not finish or exits early.\n\n");

  int counter = 42;
  int ret;

  const struct timespec ts = {.tv_nsec = 0, .tv_sec = 0};

  pthread_mutex_t mutex;
  SUCCESS_OR_FAIL(pthread_mutex_init(&mutex, NULL));
  SUCCESS_OR_FAIL(pthread_mutex_timedlock(&mutex, &ts));
  SUCCESS_OR_FAIL(pthread_mutex_lock(&mutex));
  SUCCESS_OR_FAIL(pthread_mutex_unlock(&mutex));
  SUCCESS_OR_FAIL(pthread_mutex_trylock(&mutex));
  SUCCESS_OR_FAIL(pthread_mutex_unlock(&mutex));
  SUCCESS_OR_FAIL(pthread_mutex_destroy(&mutex));

  pthread_cond_t cond;
  SUCCESS_OR_FAIL(pthread_cond_init(&cond, NULL));
  SUCCESS_OR_FAIL(pthread_cond_wait(&cond, &mutex));
  SUCCESS_OR_FAIL(pthread_cond_timedwait(&cond, &mutex, &ts));
  SUCCESS_OR_FAIL(pthread_cond_signal(&cond));
  SUCCESS_OR_FAIL(pthread_cond_broadcast(&cond));
  SUCCESS_OR_FAIL(pthread_cond_destroy(&cond));

  pthread_spinlock_t spin;
  SUCCESS_OR_FAIL(pthread_spin_init(&spin, 0));
  SUCCESS_OR_FAIL(pthread_spin_lock(&spin));
  SUCCESS_OR_FAIL(pthread_spin_trylock(&spin));
  SUCCESS_OR_FAIL(pthread_spin_unlock(&spin));
  SUCCESS_OR_FAIL(pthread_spin_destroy(&spin));

  pthread_rwlock_t rwlock;
  SUCCESS_OR_FAIL(pthread_rwlock_init(&rwlock, NULL));
  SUCCESS_OR_FAIL(pthread_rwlock_rdlock(&rwlock));
  SUCCESS_OR_FAIL(pthread_rwlock_wrlock(&rwlock));
  SUCCESS_OR_FAIL(pthread_rwlock_timedrdlock(&rwlock, &ts));
  SUCCESS_OR_FAIL(pthread_rwlock_timedwrlock(&rwlock, &ts));
  SUCCESS_OR_FAIL(pthread_rwlock_tryrdlock(&rwlock));
  SUCCESS_OR_FAIL(pthread_rwlock_trywrlock(&rwlock));
  SUCCESS_OR_FAIL(pthread_rwlock_unlock(&rwlock));
  SUCCESS_OR_FAIL(pthread_rwlock_destroy(&rwlock));

  printf("\nInterposition test successful\n");
  return EXIT_SUCCESS;
}
