/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
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

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"

#define V4L2_BUF_FLAG_LAST			0x00100000

static char *dbg_type[2] = {"OUTPUT", "CAPTURE"};
static char *dbg_status[2] = {"ON", "OFF"};

/* check is it a decoder video device */
static int is_video_encoder(int fd, const char *name)
{
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdesc;
	int found;

	memzero(cap);
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		err("Failed to verify capabilities: %m");
		return -1;
	}

	dbg("caps (%s): driver=\"%s\" bus_info=\"%s\" card=\"%s\" "
	    "version=%u.%u.%u", name, cap.driver, cap.bus_info, cap.card,
	    (cap.version >> 16) & 0xff,
	    (cap.version >> 8) & 0xff,
	     cap.version & 0xff);

	if (!(cap.capabilities & V4L2_CAP_STREAMING) ||
	    !(cap.capabilities & V4L2_CAP_VIDEO_M2M)) {
		err("Insufficient capabilities for video device %s", name);
		return -1;
	}

	memzero(fmtdesc);
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmtdesc.index = 0;

    found = 0;
	while( ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1 ) {
		dbg("  %s", fmtdesc.description);

		switch (fmtdesc.pixelformat) {
		    case V4L2_PIX_FMT_NV12:
		    case V4L2_PIX_FMT_NV21:
		        found = 1;
		        break;
		    default:
		        dbg("%s is not an encoder video device", name);
		        return -1;
		}

		if( found == 1)
		    break;

		fmtdesc.index++;
	}

	found = 0;
	memzero(fmtdesc);
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	while( ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1 ) {
		dbg("  %s", fmtdesc.description);

		switch (fmtdesc.pixelformat) {
		case V4L2_PIX_FMT_MPEG:
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_H263:
		case V4L2_PIX_FMT_MPEG1:
		case V4L2_PIX_FMT_MPEG2:
		case V4L2_PIX_FMT_MPEG4:
		case V4L2_PIX_FMT_XVID:
		case V4L2_PIX_FMT_VC1_ANNEX_G:
		case V4L2_PIX_FMT_VC1_ANNEX_L:
		case V4L2_PIX_FMT_VP8:
			found = 1;
			break;
		default:
			err("%s is not an encoder video device", name);
			return -1;
		}

		if( found ==1 )
			break;

		fmtdesc.index++;
	}

	return 0;
}

int video_open(struct instance *i, const char *video_dev_name)
{
	//char video_name[64];
	//unsigned idx = 0, v = 0;
	int ret, fd = -1;

	dbg("try to open video encoder device: %s", video_dev_name);

	fd = open(video_dev_name, O_RDWR, 0);
	if (fd < 0) {
	    err("Failed to open video device: %s", video_dev_name);
        return -1;
	}

	ret = is_video_encoder(fd, video_dev_name);
	if (ret < 0) {
	    close(fd);
        return -1;
	}

	info("found encoder video device %s", video_dev_name);
	dbg("found encoder video device %s", video_dev_name);

	i->video.fd = fd;

    return 0;
}

void video_close(struct instance *i)
{
	close(i->video.fd);
}

int video_set_control(struct instance *i)
{
	struct v4l2_streamparm parm;
	struct v4l2_control cntrl;
	int ret;

	parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	parm.parm.output.timeperframe.numerator = 1;
	if (i->framerate <= 120 && i->framerate >= 1)
		parm.parm.output.timeperframe.denominator = i->framerate;
	else
		parm.parm.output.timeperframe.denominator = 30;

	info("setting framerate: %u (%u/%u)", i->framerate,
		parm.parm.output.timeperframe.numerator,
		parm.parm.output.timeperframe.denominator);

	ret = ioctl(i->video.fd, VIDIOC_S_PARM, &parm);
	if (ret)
		err("set framerate (%s)", strerror(errno));

	cntrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
	if (i->bitrate <= 160000000 && i->bitrate >= 32000)
		cntrl.value = i->bitrate;
	else
		cntrl.value = 10 * 1024 * 1024;

	info("setting bitrate: %u", cntrl.value);

	ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
	if (ret)
		err("set control - bitrate (%s)", strerror(errno));

	if (i->codec == V4L2_PIX_FMT_H264) {
		memset(&cntrl, 0, sizeof(cntrl));
		cntrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
		cntrl.value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
		info("setting h264 profile: %u", cntrl.value);
		ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
		if (ret)
			err("set control - profile (%s)", strerror(errno));

		memset(&cntrl, 0, sizeof(cntrl));
		cntrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
		cntrl.value = V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
		info("setting h264 level: %u", cntrl.value);
		ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
		if (ret)
			err("set control - level (%s)", strerror(errno));

		if (i->num_bframes && i->num_bframes < 4) {
			memset(&cntrl, 0, sizeof(cntrl));
			cntrl.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
			cntrl.value = i->num_bframes;
			info("setting num b frames: %u", cntrl.value);
			ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
			if (ret)
				err("set control - num b frames (%s)",
					strerror(errno));
		}
	}

	if (i->codec == V4L2_PIX_FMT_HEVC) {
		if (i->num_bframes && i->num_bframes < 4) {
			memset(&cntrl, 0, sizeof(cntrl));
			cntrl.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
			cntrl.value = i->num_bframes;
			info("setting num b frames: %u", cntrl.value);
			ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
			if (ret)
				err("set control - num b frames (%s)",
					strerror(errno));
		}
	}

	if (i->codec == V4L2_PIX_FMT_VP8) {
		memset(&cntrl, 0, sizeof(cntrl));
		cntrl.id = V4L2_CID_MPEG_VIDEO_VPX_PROFILE;
		cntrl.value = 2;
		info("setting VP8 profile: %u", cntrl.value);
		ret = ioctl(i->video.fd, VIDIOC_S_CTRL, &cntrl);
		if (ret)
			err("set control - profile (%s)", strerror(errno));
	}

#if 0
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[32];
	int c = 0;

	memset (&ctrls, 0, sizeof(ctrls));
	ctrls.controls = ctrl;
	ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;

	ctrl[c].id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
	ctrl[c].value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
	c++;

	ctrl[c].id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
	ctrl[c].value = V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
	c++;

	ctrl[c].id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
	ctrl[c].value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	c++;

	ctrl[c].id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
	ctrl[c].value = 1;
	c++;

	/*
	 * intra_period = pframes + bframes + 1
	 * bframes/pframes must be integer
	 */

	ctrl[c].id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
	ctrl[c].value = 0;
	c++;

//	ctrl[c].id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES;
//	ctrl[c].value = 2 * 15 - 1;
//	c++;

	ctrls.count = c;

	ret = ioctl(i->video.fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	if (ret)
		err("set ext controls (%s)", strerror(errno));
#endif
	return 0;
}

int video_export_buf(struct instance *i, unsigned int index)
{
	struct video *vid = &i->video;
	struct v4l2_exportbuffer expbuf;
	int num_planes = CAP_PLANES;
	int n;

	for (n = 0; n < num_planes; n++) {
		memset(&expbuf, 0, sizeof(expbuf));

		expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		expbuf.index = index;
		expbuf.flags = O_CLOEXEC | O_RDWR;
		expbuf.plane = n;

		if (ioctl(vid->fd, VIDIOC_EXPBUF, &expbuf) < 0) {
			err("Failed to export CAPTURE buffer index%d (%s)",
			    index, strerror(errno));
			return -1;
		}

		info("Exported CAPTURE buffer index%d (plane%d) with fd %d",
		     index, n, expbuf.fd);
	}

	return 0;
}

static int video_queue_buf(struct instance *i, unsigned int index,
			   unsigned int l1, unsigned int l2,
			   unsigned int type, unsigned int nplanes)
{
	struct video *vid = &i->video;
	struct v4l2_buffer buf;
	//struct v4l2_plane planes[2];
	struct timeval tv;
	int ret;

	memzero(buf);
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = index;
	buf.length = nplanes;

	//buf.m.bytesused = l1;
	//buf.m.planes[1].bytesused = l2;

	buf.m.offset = 0;


	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		buf.length = vid->cap_buf_size[0];
	} else {
		buf.length = vid->out_buf_size;
		if (l1 == 0) {
			buf.bytesused = 1;
			buf.flags |= V4L2_BUF_FLAG_LAST;
		}

		ret = gettimeofday(&tv, NULL);
		if (ret)
			err("getting timeofday (%s)", strerror(errno));

		buf.timestamp = tv;
	}

	ret = ioctl(vid->fd, VIDIOC_QBUF, &buf);
	if (ret) {
		err("Failed to queue buffer (index=%d) on %s (%s)",
		    buf.index,
		    dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE],
		    strerror(errno));
		return -1;
	}

	dbg("  Queued buffer on %s queue with index %d",
	    dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE], buf.index);

	return 0;
}

int video_queue_buf_out(struct instance *i, unsigned int n, unsigned int length)
{
	struct video *vid = &i->video;

	if (n >= vid->out_buf_cnt) {
		err("Tried to queue a non exisiting buffer");
		return -1;
	}

	return video_queue_buf(i, n, length, 0,
			       V4L2_BUF_TYPE_VIDEO_OUTPUT,
			       OUT_PLANES);
}

int video_queue_buf_cap(struct instance *i, unsigned int index)
{
	struct video *vid = &i->video;

	if (index >= vid->cap_buf_cnt) {
		err("Tried to queue a non exisiting buffer");
		return -1;
	}

	return video_queue_buf(i, index, vid->cap_buf_size[0],
			       vid->cap_buf_size[1],
			       V4L2_BUF_TYPE_VIDEO_CAPTURE,
			       CAP_PLANES);
}

static int video_dequeue_buf(struct instance *i, struct v4l2_buffer *buf)
{
	struct video *vid = &i->video;
	int ret;

	ret = ioctl(vid->fd, VIDIOC_DQBUF, buf);
	if (ret < 0) {
		err("Failed to dequeue buffer (%s)", strerror(errno));
		return -errno;
	}

	dbg("Dequeued buffer on %s queue with index %d (flags:%x, bytesused:%d)",
	    dbg_type[buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE],
	    buf->index, buf->flags, buf->m.planes[0].bytesused);

	return 0;
}

int video_dequeue_output(struct instance *i, unsigned int *n)
{
	struct v4l2_buffer buf;
	//struct v4l2_plane planes[OUT_PLANES];
	int ret;

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
	//buf.m.planes = planes;
	buf.length = OUT_PLANES;

	ret = video_dequeue_buf(i, &buf);
	if (ret < 0)
		return ret;

	*n = buf.index;

	return 0;
}

int video_dequeue_capture(struct instance *i, unsigned int *n,
			  unsigned int *finished, unsigned int *bytesused,
			  unsigned int *data_offset, unsigned int *buf_flags)
{
	struct v4l2_buffer buf;
	//struct v4l2_plane planes[CAP_PLANES];

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	//buf.m.planes = planes;
	buf.length = CAP_PLANES;

	if (video_dequeue_buf(i, &buf))
		return -1;

	*finished = 0;

	if (buf.flags & V4L2_BUF_FLAG_LAST || buf.bytesused == 0)
		*finished = 1;

	*bytesused = buf.bytesused;
	*n = buf.index;
	*data_offset = buf.m.offset;

	if (buf_flags)
		*buf_flags = buf.flags;

	return 0;
}

int video_stream(struct instance *i, enum v4l2_buf_type type,
		 unsigned int status)
{
	struct video *vid = &i->video;
	int ret;

	ret = ioctl(vid->fd, status, &type);
	if (ret) {
		err("Failed to change streaming (type=%s, status=%s)",
		    dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE],
		    dbg_status[status == VIDIOC_STREAMOFF]);
		return -1;
	}

	dbg("Stream %s on %s queue", dbg_status[status==VIDIOC_STREAMOFF],
	    dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE]);

	return 0;
}

int video_stop(struct instance *i)
{
	int ret;

	ret = video_stream(i, V4L2_BUF_TYPE_VIDEO_CAPTURE,
			   VIDIOC_STREAMOFF);
	if (ret < 0)
		err("STREAMOFF CAPTURE queue failed (%s)", strerror(errno));

	ret = video_stream(i, V4L2_BUF_TYPE_VIDEO_OUTPUT,
			   VIDIOC_STREAMOFF);
	if (ret < 0)
		err("STREAMOFF OUTPUT queue failed (%s)", strerror(errno));

	return 0;
}

int video_setup_capture(struct instance *i, unsigned int count, unsigned int w,
			unsigned int h)
{
	struct video *vid = &i->video;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	//struct v4l2_plane planes[CAP_PLANES];
	unsigned int n;
	int ret;

	memzero(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.pixelformat = i->codec;
	ret = ioctl(vid->fd, VIDIOC_S_FMT, &fmt);
	if (ret) {
		err("CAPTURE: S_FMT failed (%ux%u) (%s)", w, h, strerror(errno));
		return -1;
	}

	info("CAPTURE: Set format %ux%u, sizeimage %u, bpl %u, pixelformat:%x",
	    fmt.fmt.pix.width, fmt.fmt.pix.height,
	    fmt.fmt.pix.sizeimage,
	    fmt.fmt.pix.bytesperline,
	    fmt.fmt.pix.pixelformat);

	vid->cap_w = fmt.fmt.pix.width;
	vid->cap_h = fmt.fmt.pix.height;

	vid->cap_buf_size[0] = fmt.fmt.pix.sizeimage;

	vid->cap_buf_cnt_min = 1;
	vid->cap_buf_queued = 0;

	memzero(reqbuf);
	reqbuf.count = count;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		err("CAPTURE: REQBUFS failed (%s)", strerror(errno));
		return -1;
	}

	vid->cap_buf_cnt = reqbuf.count;

	info("CAPTURE: Number of buffers %u (requested %u)",
		vid->cap_buf_cnt, count);

	for (n = 0; n < vid->cap_buf_cnt; n++) {
		memzero(buf);
		//memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;
		//buf.m.planes = planes;
		buf.length = CAP_PLANES;

		ret = ioctl(vid->fd, VIDIOC_QUERYBUF, &buf);
		if (ret) {
			err("CAPTURE: QUERYBUF failed (%s)", strerror(errno));
			return -1;
		}

		vid->cap_buf_off[n][0] = buf.m.offset;

		vid->cap_buf_addr[n][0] = mmap(NULL, buf.length,
					       PROT_READ | PROT_WRITE,
					       MAP_SHARED,
					       vid->fd,
					       buf.m.offset);

		if (vid->cap_buf_addr[n][0] == MAP_FAILED) {
			err("CAPTURE: MMAP failed (%s)", strerror(errno));
			return -1;
		}

		vid->cap_buf_size[0] = buf.length;
	}

	info("CAPTURE: Succesfully mmapped %u buffers", n);

	return 0;
}

int video_setup_output(struct instance *i, unsigned long codec,
		       unsigned int size, unsigned int count)
{
	struct video *vid = &i->video;
	struct v4l2_format fmt, try_fmt, g_fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	//struct v4l2_plane planes[OUT_PLANES];
	int ret;
	int n;

	memzero(try_fmt);
	try_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	try_fmt.fmt.pix.width = i->width;
	try_fmt.fmt.pix.height = i->height;
	try_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

	ret = ioctl(vid->fd, VIDIOC_TRY_FMT, &try_fmt);
	if (ret) {
		err("OUTPUT: TRY_FMT failed (%ux%u) (%s)",
			try_fmt.fmt.pix.width,
			try_fmt.fmt.pix.height, strerror(errno));
		return -1;
	}

	info("OUTPUT: Try format %ux%u, sizeimage %u, bpl %u",
	     try_fmt.fmt.pix.width, try_fmt.fmt.pix.height,
	     try_fmt.fmt.pix.sizeimage,
	     try_fmt.fmt.pix.bytesperline);

	memzero(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = i->width;
	fmt.fmt.pix.height = i->height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

	ret = ioctl(vid->fd, VIDIOC_S_FMT, &fmt);
	if (ret) {
		err("OUTPUT: S_FMT failed %ux%u (%s)", fmt.fmt.pix.width,
			fmt.fmt.pix.height, strerror(errno));
		return -1;
	}

	info("OUTPUT: Set format %ux%u, sizeimage %u, (requested=%u), bpl %u",
	     fmt.fmt.pix.width, fmt.fmt.pix.height,
	     fmt.fmt.pix.sizeimage, size,
	     fmt.fmt.pix.bytesperline);

	vid->out_buf_size = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memzero(g_fmt);
	g_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	g_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

	ret = ioctl(vid->fd, VIDIOC_G_FMT, &g_fmt);
	if (ret) {
		err("OUTPUT: G_FMT failed (%s)", strerror(errno));
		return -1;
	}

	info("OUTPUT: Get format %ux%u, sizeimage %u, bpl %u",
	     g_fmt.fmt.pix.width, g_fmt.fmt.pix.height,
	     g_fmt.fmt.pix.sizeimage,
	     g_fmt.fmt.pix.bytesperline);

	vid->out_w = g_fmt.fmt.pix.width;
	vid->out_h = g_fmt.fmt.pix.height;
	vid->out_bytesperline = g_fmt.fmt.pix.bytesperline;

	memzero(reqbuf);
	reqbuf.count = count;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		err("OUTPUT: REQBUFS failed (%s)", strerror(errno));
		return -1;
	}

	vid->out_buf_cnt = reqbuf.count;

	info("OUTPUT: Number of buffers %u (requested %u)",
	     vid->out_buf_cnt, count);

	for (n = 0; n < vid->out_buf_cnt; n++) {
		memzero(buf);
		//memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;
		//buf.m.planes = planes;
		buf.length = OUT_PLANES;

		ret = ioctl(vid->fd, VIDIOC_QUERYBUF, &buf);
		if (ret) {
			err("OUTPUT: QUERYBUF failed (%s)", strerror(errno));
			return -1;
		}

		vid->out_buf_off[n] = buf.m.offset;
		vid->out_buf_size = buf.length;

		vid->out_buf_addr[n] = mmap(NULL, buf.length,
					    PROT_READ | PROT_WRITE, MAP_SHARED,
					    vid->fd,
					    buf.m.offset);

		if (vid->out_buf_addr[n] == MAP_FAILED) {
			err("OUTPUT: MMAP failed (%s)", strerror(errno));
			return -1;
		}

		vid->out_buf_flag[n] = 0;
	}

	info("OUTPUT: Succesfully mmapped %u buffers", n);

	return 0;
}
