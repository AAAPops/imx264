/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Main file of the application
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

#include <stdio.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include "args.h"
#include "common.h"
#include "fileops.h"
#include "video.h"
#include "gentest.h"

/* This is the size of the buffer for the compressed stream.
 * It limits the maximum compressed frame size. */
#define STREAM_BUFFER_SIZE	(1024 * 1024)

/* The number of compressed stream buffers */
#define STREAM_BUFFER_CNT	2

/* The number of extra buffers for the decoded output.
 * This is the number of buffers that the application can keep
 * used and still enable video device to decode with the hardware. */
#define RESULT_EXTRA_BUFFER_CNT 2

static const int event_types[] = {
	V4L2_EVENT_EOS,
	V4L2_EVENT_SOURCE_CHANGE,
};

/*
     * Experimental, meant for debugging, testing and internal use.
     * Only implemented if CONFIG_VIDEO_ADV_DEBUG is defined.
     * You must be root to use these ioctls. Never use these in applications!
*/
static int subscribe_for_events(int fd)
{
	int size_event = sizeof(event_types) / sizeof(event_types[0]);
	struct v4l2_event_subscription sub;
	int i, ret;

	for (i = 0; i < size_event; i++) {
		memset(&sub, 0, sizeof(sub));
		sub.type = event_types[i];
		ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
		if (ret < 0)
			err("cannot subscribe for event type %d (%s)",
				sub.type, strerror(errno));
	}

	return 0;
}

static int handle_v4l_events(struct video *vid)
{
	struct v4l2_event event;
	int ret;

	memset(&event, 0, sizeof(event));
	ret = ioctl(vid->fd, VIDIOC_DQEVENT, &event);
	if (ret < 0) {
		err("vidioc_dqevent failed (%s) %d", strerror(errno), -errno);
		return -errno;
	}

	switch (event.type) {
	case V4L2_EVENT_EOS:
		info("EOS reached");
		break;
	case V4L2_EVENT_SOURCE_CHANGE:
		info("Source changed");
		break;
	default:
		dbg("unknown event type occurred %x", event.type);
		break;
	}

	return 0;
}

static void cleanup(struct instance *i)
{
	if (i->video.fd)
		video_close(i);
	if (i->in.fd)
		input_close(i);
	if (i->out.fd)
		close(i->out.fd);
}


static int save_encoded(struct instance *i, const void *buf, unsigned int size,
			bool is_ivf)
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	char filename[64];
	int ret;
	struct output *out = &i->out;
	ssize_t written;
	const char *ext = i->codec_name;

	if (!i->save_encoded)
		return 0;

	ret = sprintf(filename, "%s/encoded.%s", i->save_path, ext);
	if (ret < 0) {
		err("sprintf fail (%s)", strerror(errno));
		return -1;
	}

	if (out->fd)
		goto write;

	out->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, mode);
	if (out->fd < 0) {
		err("cannot open file (%s)", strerror(errno));
		return -1;
	}

	dbg("created file %s", filename);

write:
	written = pwrite(out->fd, buf, size, out->offs);
	if (written < 0) {
		err("cannot write to file (%s)", strerror(errno));
		return -1;
	}

	out->offs += written;

	dbg("written %zd bytes at offset %zu", written, out->offs);

	return 0;
}

static int input_read(struct instance *inst, unsigned int index,
		      unsigned int *used, unsigned int *fs)
{
	struct video *vid = &inst->video;
	unsigned char *to = vid->out_buf_addr[index];

	*used = vid->out_buf_size;
	*fs = vid->out_buf_size;

	if (inst->disable_gentest)
		return 0;

	gentest_fill(inst->width, inst->height, to, vid->out_buf_size);

	return 0;
}

/* This threads is responsible for reading input file or stream and
 * feeding video enccoder with consecutive frames to encode */
static void *input_thread_func(void *args)
{
	struct instance *i = (struct instance *)args;
	struct video *vid = &i->video;
	unsigned int used, fs, n;
	int ret;

	while (!i->error && !i->finish) {
		n = 0;
		pthread_mutex_lock(&i->lock);
		while (n < vid->out_buf_cnt && vid->out_buf_flag[n])
			n++;
		pthread_mutex_unlock(&i->lock);

		if (n < vid->out_buf_cnt) {

			ret = input_read(i, n, &used, &fs);
			if (ret)
				err("cannot read from input file");

			if (vid->total_encoded >= i->num_frames_to_save) {
				i->finish = 1;
				fs = 0;
			}

			ret = video_queue_buf_out(i, n, fs);
			if (ret)
				continue;

			pthread_mutex_lock(&i->lock);
			vid->out_buf_flag[n] = 1;
			pthread_mutex_unlock(&i->lock);

			dbg("queued output buffer %d", n);
		} else {
			pthread_mutex_lock(&i->lock);
			pthread_cond_wait(&i->cond, &i->lock);
			pthread_mutex_unlock(&i->lock);
		}
	}

	return NULL;
}


static void *main_thread_func(void *args)
{
	struct instance *i = (struct instance *)args;
	struct video *vid = &i->video;
	struct pollfd pfd;
	short revents;
	unsigned int n, finished;
	int ret;

	pfd.fd = vid->fd;
	pfd.events = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM |
		     POLLRDBAND | POLLPRI;

	fprintf(stdout, "encoded frame ");

	while (1) {
		ret = poll(&pfd, 1, 10000);
		if (!ret) {
			err("poll timeout");
			break;
		} else if (ret < 0) {
			err("poll error");
			break;
		}

		revents = pfd.revents;

		if (revents & POLLPRI)
			handle_v4l_events(vid);

		if (revents & (POLLIN | POLLRDNORM)) {
			unsigned int bytesused, dataoffset;

			/* capture buffer is ready */

			dbg("dequeuing capture buffer");

			ret = video_dequeue_capture(i, &n, &finished,
						    &bytesused, &dataoffset,
						    NULL);
			if (ret < 0)
				goto next_event;

			pthread_mutex_lock(&i->lock);
			vid->cap_buf_flag[n] = 0;
			pthread_mutex_unlock(&i->lock);

			fprintf(stdout, "%03ld\b\b\b", vid->total_encoded);
			fflush(stdout);

			if (finished)
				break;

			vid->total_encoded++;

			uint8_t *data = (uint8_t*)vid->cap_buf_addr[n];
			data += dataoffset;

			save_encoded(i, (const void *)data, bytesused,
				     i->codec == V4L2_PIX_FMT_VP8);

			ret = video_queue_buf_cap(i, n);
			if (!ret) {
				pthread_mutex_lock(&i->lock);
				vid->cap_buf_flag[n] = 1;
				pthread_mutex_unlock(&i->lock);
			}
		}

next_event:
		if (revents & (POLLOUT | POLLWRNORM)) {

			if (i->finish)
				continue;

			dbg("dequeuing output buffer");

			ret = video_dequeue_output(i, &n);
			if (ret < 0) {
				err("dequeue output buffer fail");
			} else {
				pthread_mutex_lock(&i->lock);
				vid->out_buf_flag[n] = 0;
				pthread_mutex_unlock(&i->lock);
				pthread_cond_signal(&i->cond);
			}

			dbg("dequeued output buffer %d", n);
		}
	}

	return NULL;
}


int main(int argc, char **argv)
{
	struct instance coda960;
	struct video *vid = &coda960.video;
	pthread_t input_thread, main_thread;
	int ret, n;

	if ( argc == 1) {
        print_usage(argv[0]);
        return 1;
	}

	ret = parse_args(&coda960, argc, argv);
	if (ret) {
		print_usage(argv[0]);
		return 1;
	}

	info("encoder device: %s, codec: %s", coda960.video.name, coda960.codec_name);
    info("encoding resolution: %dx%d, framerate: %d, frames_to_save: %d ",
            coda960.width, coda960.height, coda960.framerate,
            coda960.num_frames_to_save);

	pthread_mutex_init(&coda960.lock, 0);

	pthread_condattr_init(&coda960.attr);
	pthread_cond_init(&coda960.cond, &coda960.attr);

	vid->total_encoded = 0;

	ret = video_open(&coda960, coda960.video.name);
	if (ret)
		goto err;

    /* Never use these in applications!

	ret = subscribe_for_events(vid->fd);
	if (ret)
		goto err;
    */

	ret = video_setup_capture(&coda960, 2, coda960.width, coda960.height);
	if (ret)
		goto err;

	ret = video_setup_output(&coda960, coda960.codec, STREAM_BUFFER_SIZE, 1);
	if (ret)
		goto err;

	info("gentest init: %ux%u, bpl:%u, crop_w:%u, crop_h:%u",
		vid->out_w, vid->out_h, vid->out_bytesperline,
         coda960.width, coda960.height);


	ret = gentest_init(vid->out_w, vid->out_h, vid->out_bytesperline,
			   coda960.width, coda960.height,
			   vid->out_buf_size);
	if (ret)
		goto err;

	ret = video_set_control(&coda960);
	if (ret)
		goto err;

	for (n = 0; n < vid->cap_buf_cnt; n++) {
		ret = video_queue_buf_cap(&coda960, n);
		if (ret)
			goto err;

		vid->cap_buf_flag[n] = 1;
	}

	ret = video_stream(&coda960, V4L2_BUF_TYPE_VIDEO_CAPTURE, VIDIOC_STREAMON);
	if (ret)
		goto err;

	ret = video_stream(&coda960, V4L2_BUF_TYPE_VIDEO_OUTPUT, VIDIOC_STREAMON);
	if (ret)
		goto err;

	dbg("Launching threads");

	if (pthread_create(&input_thread, NULL, input_thread_func, &coda960))
		goto err;

	if (pthread_create(&main_thread, NULL, main_thread_func, &coda960))
		goto err;

	pthread_join(input_thread, 0);
	pthread_join(main_thread, 0);

	dbg("Threads have finished");

	video_stop(&coda960);

	info("Total frames encoded %ld", vid->total_encoded);

	pthread_mutex_destroy(&coda960.lock);
	cleanup(&coda960);
	gentest_deinit();

	return 0;
err:
	pthread_mutex_destroy(&coda960.lock);
	pthread_cond_destroy(&coda960.cond);
	pthread_condattr_destroy(&coda960.attr);
	cleanup(&coda960);
	gentest_deinit();
	return 1;
}
