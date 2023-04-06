/*
 * File: platform_defs.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Platform specific definitions and parameters
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




#ifndef _PLATFORM_DEFS_H_INCLUDED_
#define _PLATFORM_DEFS_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif
    /*
     * For each machine that is used, one needs to define
     *  NUMBER_OF_SOCKETS: the number of sockets the machine has
     *  CORES_PER_SOCKET: the number of cores per socket
     *  CACHE_LINE_SIZE
     *  NOP_DURATION: the duration in cycles of a noop instruction (generally 1 cycle on most small machines)
     *  the_cores - a mapping from the core ids as configured in the OS to physical cores (the OS might not alwas be configured corrrectly)
     *  get_cluster - a function that given a core id returns the socket number ot belongs to
     */


#ifdef DEFAULT
#  define NUMBER_OF_SOCKETS 1
#  define CORES_PER_SOCKET CORE_NUM
#  define CACHE_LINE_SIZE 64
# define NOP_DURATION 2
  static uint8_t  __attribute__ ((unused)) the_cores[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 
        8, 9, 10, 11, 12, 13, 14, 15, 
        16, 17, 18, 19, 20, 21, 22, 23, 
        24, 25, 26, 27, 28, 29, 30, 31, 
        32, 33, 34, 35, 36, 37, 38, 39, 
        40, 41, 42, 43, 44, 45, 46, 47  
    };
#endif

#define PREFETCHW(x)		

    static inline int get_cluster(int thread_id) {
#ifdef __solaris__
        if (thread_id>64){
            perror("Thread id too high");
            return 0;
        }
        return thread_id/CORES_PER_SOCKET;  
#else
        return thread_id/CORES_PER_SOCKET;
#endif
    }

#ifdef __cplusplus
}

#endif


#endif
