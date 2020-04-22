/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Common stuff header file
 *
 * Copyright 2012 Samsung Electronics Co., Ltd.
 * Copyright (c) 2015 Linaro Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define VERSION      "0.3.a"
#define TIMEOUT_SEC  5

#define FOREGROUND 0
#define BACKGROUND 1


/* When ADD_DETAILS is defined every debug and error message contains
 * information about the file, function and line of code where it has
 * been called */
//#define ADD_DETAILS

/* When DEBUG is defined debug messages are printed on the screen.
 * Otherwise only error messages are displayed. */
//#define DEBUG
#ifdef TMP_DISABLE

#ifdef ADD_DETAILS
#define err(msg, ...) \
	fprintf(stderr, "Error (%s:%s:%d): " msg "\n", __FILE__, \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define err(msg, ...) \
	fprintf(stderr, "Error: " msg "\n", __FILE__, ##__VA_ARGS__)
#endif /* ADD_DETAILS */

#define info(msg, ...) \
	fprintf(stderr, "Info : " msg "\n", ##__VA_ARGS__)

#ifdef DEBUG
#ifdef ADD_DETAILS
#define dbg(msg, ...) \
	fprintf(stdout, "(%s:%s:%d): " msg "\n", __FILE__, \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define dbg(msg, ...) \
	fprintf(stdout, msg "\n", ##__VA_ARGS__)
#endif /* ADD_DETAILS */
#else /* DEBUG */
#define dbg(...) {}
#endif /* DEBUG */

#endif // New Webcam

#define MEMZERO(x)	memset(&(x), 0, sizeof (x));



struct Buffer {
    void   *start;
    size_t  length;
    size_t  bytesused;
};

#endif /* INCLUDE_COMMON_H */

