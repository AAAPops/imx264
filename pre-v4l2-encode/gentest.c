/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "common.h"

enum yuv_order {
	YUV_YCbCr = 1,
	YUV_YCrCb = 2,
	YUV_YC = 4,
	YUV_CY = 8,
};

struct yuv_info {
	enum yuv_order order;
	unsigned int xsub;
	unsigned int ysub;
	unsigned int chroma_stride;
};

struct color_yuv {
	unsigned char y;
	unsigned char u;
	unsigned char v;
};

#define MAKE_YUV_601_Y(r, g, b) \
	((( 66 * (r) + 129 * (g) +  25 * (b) + 128) >> 8) + 16)
#define MAKE_YUV_601_U(r, g, b) \
	(((-38 * (r) -  74 * (g) + 112 * (b) + 128) >> 8) + 128)
#define MAKE_YUV_601_V(r, g, b) \
	(((112 * (r) -  94 * (g) -  18 * (b) + 128) >> 8) + 128)

static void fill_yuv_planar_noise(unsigned char *y_mem, unsigned char *u_mem,
				  unsigned char *v_mem, unsigned int width,
				  unsigned int height, unsigned int stride)
{
	const struct yuv_info yuv = { YUV_YCbCr, 2, 2, 2 };	/* NV12 */
	unsigned int cs = yuv.chroma_stride;
	unsigned int xsub = yuv.xsub;
	unsigned int ysub = yuv.ysub;
	unsigned int x;
	unsigned int y;
	struct color_yuv color;
	unsigned int c;

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {
			c = rand() % 192;

			if (c > 128)
				color.y = 236;
			else
				color.y = 16;
			color.u = 128;
			color.v = 128;

			y_mem[x] = color.y;
			u_mem[x / xsub * cs] = color.u;
			v_mem[x / xsub * cs] = color.v;
		}

		y_mem += stride;

		if ((y + 1) % ysub == 0) {
			u_mem += stride * cs / xsub;
			v_mem += stride * cs / xsub;
		}
	}

}

static void fill_yuv_planar(unsigned char *y_mem, unsigned char *u_mem,
			    unsigned char *v_mem, unsigned int width,
			    unsigned int height, unsigned int stride)
{
	const struct yuv_info yuv = { YUV_YCbCr, 2, 2, 2 };	/* NV12 */
	unsigned int cs = yuv.chroma_stride;
	unsigned int xsub = yuv.xsub;
	unsigned int ysub = yuv.ysub;
	unsigned int x;
	unsigned int y;
	unsigned int r, g, b;
	struct color_yuv color;

	r = 0;
	g = 0;
	b = 192;

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {
			if (x % 128 == 0) {
				r = r ? 0 : 192;
				b = b ? 0 : 192;
			}

			color.y = MAKE_YUV_601_Y(r, g, b);
			color.u = MAKE_YUV_601_U(r, g, b);
			color.v = MAKE_YUV_601_V(r, g, b);

			y_mem[x] = color.y;
			u_mem[x / xsub * cs] = color.u;
			v_mem[x / xsub * cs] = color.v;
		}

		y_mem += stride;

		if ((y + 1) % ysub == 0) {
			u_mem += stride * cs / xsub;
			v_mem += stride * cs / xsub;
		}
	}
}

static void *testbuf;

int gentest_init(unsigned int width, unsigned int height, unsigned int stride,
		 unsigned int crop_w, unsigned int crop_h,
		 unsigned int size)
{
	unsigned char *y_mem, *u_mem, *v_mem;
	unsigned int w = width, h = height;

	if (crop_w && crop_h) {
		w = crop_w;
		h = crop_h;
	}

	srand(1);

	testbuf = malloc(size);
	if (!testbuf)
		return -1;

	y_mem = testbuf;
	u_mem = testbuf + stride * height;
	v_mem = u_mem + 1;

	fill_yuv_planar(y_mem, u_mem, v_mem, w, h, stride);

	return 0;
}

void gentest_deinit(void)
{
	if (testbuf)
		free(testbuf);
}

void gentest_fill(unsigned int width, unsigned int height, unsigned char *to,
		  unsigned int size)
{
	unsigned int stride = ALIGN(width, 128);
	unsigned char *y_mem = to;
	unsigned char *u_mem = to + stride * ALIGN(height, 32);
	unsigned char *v_mem = u_mem + 1;

	memcpy(to, testbuf, size);
	fill_yuv_planar_noise(y_mem, u_mem, v_mem, width/4, height/4, stride);
}
