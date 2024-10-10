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
extern "C"
{
#endif

/*
 * Cache line size in bytes.
 */
#define CACHE_LINE_SIZE 128

/*
 * Maximum number of threads allowed per lock.
 */
#define MAX_NUMBER_THREADS 600

/*
 * Maximum number of locks allowed per application.
 */
#define MAX_NUMBER_LOCKS INT_MAX

#ifdef __cplusplus
}
#endif

#endif
