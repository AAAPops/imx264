/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Argument parser
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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/videodev2.h>

#include "common.h"

void print_usage(char *name)
{
	printf("ver.%s \n", VERSION);
	printf("Usage: %s\n", name);
	printf("\t-c <codec> - The codec to be used for encoding\n");
	printf("\t\t     Available codecs: mpeg4, h264, h265, h263, xvid, mpeg2, mpeg1, vp8, vp9\n");
	printf("\t-d <device> - video encoder device (e.g. /dev/video0)\n");
	printf("\t-w video width\n");
	printf("\t-h video height\n");
	printf("\t-f save encoded frames on disk\n");
	printf("\t-g disable test pattern generator\n");
	printf("\t-n number of frames to encode (default 100) \n");
	printf("\t-v verbose mode\n");
	printf("\t-B bitrate\n");
	printf("\t-F framerate\n");
	printf("\t-b number of B frames\n");
	printf("\n");
}

void init_to_defaults(struct instance *i)
{
	memset(i, 0, sizeof(*i));
}

int get_codec(char *str)
{
	if (strncasecmp("mpeg4", str, 5) == 0) {
		return V4L2_PIX_FMT_MPEG4;
	} else if (strncasecmp("h264", str, 4) == 0) {
		return V4L2_PIX_FMT_H264;
	} else if (strncasecmp("h263", str, 4) == 0) {
		return V4L2_PIX_FMT_H263;
	} else if (strncasecmp("xvid", str, 4) == 0) {
		return V4L2_PIX_FMT_XVID;
	} else if (strncasecmp("mpeg2", str, 5) == 0) {
		return V4L2_PIX_FMT_MPEG2;
	} else if (strncasecmp("mpeg1", str, 5) == 0) {
		return V4L2_PIX_FMT_MPEG1;
	} else if (strncasecmp("vp8", str, 3) == 0) {
		return V4L2_PIX_FMT_VP8;
	} else if (strncasecmp("vp9", str, 3) == 0) {
		return V4L2_PIX_FMT_VP9;
	} else if (strncasecmp("h265", str, 4) == 0) {
		return V4L2_PIX_FMT_HEVC;
	}
	return 0;
}

int parse_args(struct instance *i, int argc, char **argv)
{
	int c;

	init_to_defaults(i);

	i->video.name = "/dev/video0";
    i->width = 640;
    i->height = 480;
	i->num_frames_to_save = 100;
    i->framerate = 10;

    i->codec = V4L2_PIX_FMT_H264;
    i->codec_name = "h264";

	while ((c = getopt(argc, argv, "w:h:c:d:f:n:gvB:F:b:")) != -1) {
		switch (c) {
		case 'c':
			i->codec = get_codec(optarg);
			i->codec_name = optarg;
			break;
		case 'd':
			i->video.name = optarg;
			break;
		case 'w':
			i->width = strtol(optarg, NULL, 10);
			break;
		case 'h':
			i->height = strtol(optarg, NULL, 10);
			break;
		case 'f':
			i->save_encoded = 1;
			i->save_path = optarg;
			break;
		case 'n':
			i->num_frames_to_save = strtol(optarg, NULL, 10);
			break;
		case 'g':
			i->disable_gentest = true;
			break;
		case 'v':
			i->verbose = true;
			break;
		case 'B':
			i->bitrate = strtol(optarg, NULL, 10);
			break;
		case 'F':
			i->framerate = strtol(optarg, NULL, 10);
			break;
		case 'b':
			i->num_bframes = strtol(optarg, NULL, 10);
			break;
		case 'p':
			i->num_pframes = strtol(optarg, NULL, 10);
			break;
		default:
			err("Bad argument");
			return -1;
		}
	}

	return 0;
}

