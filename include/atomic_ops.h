/*
 * File: atomic_ops.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Cross-platform interface to common atomic operations
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




#ifndef _ATOMIC_OPS_H_INCLUDED_
#define _ATOMIC_OPS_H_INCLUDED_

#include <inttypes.h>

#define COMPILER_BARRIER asm volatile("" ::: "memory")
/*
 *  x86 code
 */

#  include <xmmintrin.h>

//Swap pointers
static inline void* swap_pointer(volatile void* ptr, void *x) {
#  ifdef __i386__
    __asm__ __volatile__("xchgl %0,%1"
            :"=r" ((unsigned) x)
            :"m" (*(volatile unsigned *)ptr), "0" (x)
            :"memory");

    return x;
#  elif defined(__x86_64__)
    __asm__ __volatile__("xchgq %0,%1"
            :"=r" ((unsigned long long) x)
            :"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
            :"memory");

    return x;
#  endif
}

//Swap uint64_t
static inline uint64_t swap_uint64(volatile uint64_t* target,  uint64_t x) {
    __asm__ __volatile__("xchgq %0,%1"
            :"=r" ((uint64_t) x)
            :"m" (*(volatile uint64_t *)target), "0" ((uint64_t) x)
            :"memory");

    return x;
}

//Swap uint32_t
static inline uint32_t swap_uint32(volatile uint32_t* target,  uint32_t x) {
    __asm__ __volatile__("xchgl %0,%1"
            :"=r" ((uint32_t) x)
            :"m" (*(volatile uint32_t *)target), "0" ((uint32_t) x)
            :"memory");

    return x;
}

//Swap uint16_t
static inline uint16_t swap_uint16(volatile uint16_t* target,  uint16_t x) {
    __asm__ __volatile__("xchgw %0,%1"
            :"=r" ((uint16_t) x)
            :"m" (*(volatile uint16_t *)target), "0" ((uint16_t) x)
            :"memory");

    return x;
}

//Swap uint8_t
static inline uint8_t swap_uint8(volatile uint8_t* target,  uint8_t x) {
    __asm__ __volatile__("xchgb %0,%1"
            :"=r" ((uint8_t) x)
            :"m" (*(volatile uint8_t *)target), "0" ((uint8_t) x)
            :"memory");

    return x;
}

//test-and-set uint8_t
static inline uint8_t tas_uint8(volatile uint8_t *addr) {
    uint8_t oldval;
    __asm__ __volatile__("xchgb %0,%1"
            : "=q"(oldval), "=m"(*addr)
            : "0"((unsigned char) 0xff), "m"(*addr) : "memory");
    return (uint8_t) oldval;
}

//atomic operations interface
//Compare-and-swap
#define CAS_PTR(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U8(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U16(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U32(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
//Swap
#define SWAP_PTR(a,b) swap_pointer(a,b)
#define SWAP_U8(a,b) swap_uint8(a,b)
#define SWAP_U16(a,b) swap_uint16(a,b)
#define SWAP_U32(a,b) swap_uint32(a,b)
#define SWAP_U64(a,b) swap_uint64(a,b)
//Fetch-and-increment
#define FAI_U8(a) __sync_fetch_and_add(a,1)
#define FAI_U16(a) __sync_fetch_and_add(a,1)
#define FAI_U32(a) __sync_fetch_and_add(a,1)
#define FAI_U64(a) __sync_fetch_and_add(a,1)
//Fetch-and-decrement
#define FAD_U8(a) __sync_fetch_and_sub(a,1)
#define FAD_U16(a) __sync_fetch_and_sub(a,1)
#define FAD_U32(a) __sync_fetch_and_sub(a,1)
#define FAD_U64(a) __sync_fetch_and_sub(a,1)
//Increment-and-fetch
#define IAF_U8(a) __sync_add_and_fetch(a,1)
#define IAF_U16(a) __sync_add_and_fetch(a,1)
#define IAF_U32(a) __sync_add_and_fetch(a,1)
#define IAF_U64(a) __sync_add_and_fetch(a,1)
//Decrement-and-fetch
#define DAF_U8(a) __sync_sub_and_fetch(a,1)
#define DAF_U16(a) __sync_sub_and_fetch(a,1)
#define DAF_U32(a) __sync_sub_and_fetch(a,1)
#define DAF_U64(a) __sync_sub_and_fetch(a,1)
//Test-and-set
#define TAS_U8(a) tas_uint8(a)
//Memory barrier
#define MEM_BARRIER __sync_synchronize()
//Relax CPU
//#define PAUSE _mm_pause()


#endif



