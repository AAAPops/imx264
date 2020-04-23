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

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <semaphore.h>

#define VERSION     "0.1"

/* When ADD_DETAILS is defined every debug and error message contains
 * information about the file, function and line of code where it has
 * been called */
#define DEBUG
#define ADD_DETAILS

/* When DEBUG is defined debug messages are printed on the screen.
 * Otherwise only error messages are displayed. */
//#define DEBUG

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

#define V4L2_PIX_FMT_HEVC	v4l2_fourcc('H', 'E', 'V', 'C') /* HEVC */
#define V4L2_PIX_FMT_VP9	v4l2_fourcc('V', 'P', '9', '0') /* VP9 */

#define memzero(x)	memset(&(x), 0, sizeof (x));

#define ALIGN(val, align)	((val + (align - 1)) & ~(align - 1))

/* Maximum number of output buffers */
#define MAX_OUT_BUF		16

/* Maximum number of capture buffers (32 is the limit imposed by MFC */
#define MAX_CAP_BUF		32

/* Number of output planes */
#define OUT_PLANES		1

/* Number of capture planes */
#define CAP_PLANES		1

/* Maximum number of planes used in the application */
//#define MAX_PLANES		CAP_PLANES

/* Maximum number of frame buffers - used for double buffering and
 * vsyns synchronisation */
//#define FB_MAX_BUFS		2

/* The buffer is free to use by video decoder */
//#define BUF_FREE		0

/* Input file related parameters */
struct input {
	char *name;
	int fd;
	char *p;
	int size;
	int offs;
};

/* Output file related parameters */
struct output {
	char *name;
	int fd;
	char *p;
	unsigned int size;
	off_t offs;
};

/* video decoder related parameters */
struct video {
	char *name;
	int fd;

	/* Output queue related */
	unsigned int out_w;
	unsigned int out_h;
	unsigned int out_bytesperline;
	unsigned int out_buf_cnt;
	unsigned int out_buf_size;
	uint32_t out_buf_off[MAX_OUT_BUF];
	uint8_t *out_buf_addr[MAX_OUT_BUF];
	int out_buf_flag[MAX_OUT_BUF];

	/* Capture queue related */
	unsigned int cap_w;
	unsigned int cap_h;
	//unsigned int cap_crop_w;
	//unsigned int cap_crop_h;
	//unsigned int cap_crop_left;
	//unsigned int cap_crop_top;
	unsigned int cap_buf_cnt;
	unsigned int cap_buf_cnt_min;
	unsigned int cap_buf_size;
	uint32_t cap_buf_off[MAX_CAP_BUF];
	uint8_t *cap_buf_addr[MAX_CAP_BUF];
	unsigned int cap_buf_flag[MAX_CAP_BUF];
	unsigned int cap_buf_queued;

	unsigned long total_encoded;
};

struct instance {
	unsigned int width;
	unsigned int height;
	unsigned int save_encoded;
	char *save_path;
	unsigned long num_frames_to_save;
	bool disable_gentest;
	bool verbose;

	/* Input file related parameters */
	struct input	in;

	struct output	out;

	/* video decoder related parameters */
	struct video	video;

	unsigned int bitrate;
	unsigned int framerate;
	unsigned int num_bframes;
	unsigned int num_pframes;

	/* Codec to be used for encoding */
	unsigned long codec;
	char *codec_name;

	pthread_mutex_t lock;
	pthread_condattr_t attr;
	pthread_cond_t cond;

	/* Control */
	int error;   /* The error flag */
	unsigned int finish; /* Flag set when decoding has been completed
				and all threads finish */
};

#endif /* INCLUDE_COMMON_H */

