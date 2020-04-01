#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "common.h"
#include "webcam.h"

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}


static int init_mmap(struct Webcam_inst* i) {
    struct v4l2_requestbuffers reqbuf;

    MEMZERO(reqbuf);

    reqbuf.count = REQ_BUFF;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if( xioctl(i->wcam_fd, VIDIOC_REQBUFS, &reqbuf) != 0) {
        if (EINVAL == errno) {
            err("'%s' does not support memory mapping", i->wcam_name);
            return -1;
        } else {
            err("ioctl(VIDIOC_REQBUFS)");
            return -1;
        }
    }

    if( reqbuf.count < 2 ) {
        err("Insufficient buffer memory on '%s'", i->wcam_name);
        return -1;
    }

    /*
    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(stderr, "Out of memory \n");
        exit(EXIT_FAILURE);
    }
    */

    i->buffers_n = reqbuf.count;
    int iter;

    struct v4l2_buffer buff;
    MEMZERO(buff);

    for( iter = 0; iter < i->buffers_n; iter++) {
        buff.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buff.memory      = V4L2_MEMORY_MMAP;
        buff.index       = iter;

        if( xioctl(i->wcam_fd, VIDIOC_QUERYBUF, &buff) == -1 ) {
            err("ioctl(VIDIOC_QUERYBUF)");
            return -1;
        }

        i->buffers[iter].length = buff.length;
        i->buffers[iter].start =
                mmap(NULL /* start anywhere */,
                     buff.length,
                     PROT_READ | PROT_WRITE /* required */,
                     MAP_SHARED /* recommended */,
                     i->wcam_fd, buff.m.offset);

        if( i->buffers[iter].start == MAP_FAILED ) {
            err("mmap()");
            return -1;
        }
        info("Webcam buffers[%d].length = %d", iter, buff.length);
    }

    dbg("Webcam device '%s' buffers mmap() successfull", i->wcam_name);
    return 0;
}


int wcam_init(struct Webcam_inst* i)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_streamparm streamparm;
    unsigned int min;
    int ret;

    if( xioctl(i->wcam_fd, VIDIOC_QUERYCAP, &cap) != 0 ) {
        err("'%s' is not V4L2 device", i->wcam_name);
        return -1;
    }

    if( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {
        err("'%s' is no video capture device", i->wcam_name);
        return -1;
    }

    if( !(cap.capabilities & V4L2_CAP_STREAMING) ) {
        err("'%s' does not support streaming i/o", i->wcam_name);
        return -1;
    }


    /* Select video input, video standard and tune here. */
    /*
    MEMZERO(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if( xioctl(i->wcam_fd, VIDIOC_CROPCAP, &cropcap) == 0 ) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; // reset to default

        if( xioctl(i->wcam_fd, VIDIOC_S_CROP, &crop) == -1 ) {
            switch (errno) {
                case EINVAL:
                    // Cropping not supported.
                    break;

                default:
                    // Errors ignored.
                    break;
            }
        }
    } else {
        // Errors ignored.
    }
    */

    //if( get_info == 1 )
    //    get_device_info();

    // Set frame format
    MEMZERO(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = i->width;
    fmt.fmt.pix.height      = i->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if( xioctl(i->wcam_fd, VIDIOC_S_FMT, &fmt) != 0 ) {
        err("'%s' can not set frame format %dx%d",
                i->wcam_name, i->width, i->height);
        return -1;
    }


    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;


    // Set framerate
    MEMZERO(streamparm);

    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( xioctl(i->wcam_fd, VIDIOC_G_PARM, &streamparm) != 0 ) {
        err("'%s' can not get stream params: %m \n", i->wcam_name);
        return -1;
    }

    streamparm.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = i->frame_rate;
    if( xioctl(i->wcam_fd, VIDIOC_S_PARM, &streamparm) !=0 ) {
        err("'%s' can not set frame rate: %m \n", i->wcam_name);
        return -1;
    }

    ret = init_mmap(i);
    if( ret != 0 )
        return -1;

    dbg("Webcam device '%s' initialized successfull", i->wcam_name);
    return 0;
}


int wcam_open(struct Webcam_inst* i) {
    struct stat st;

    i->wcam_fd = open(i->wcam_name, O_RDWR | O_NONBLOCK, 0);
    if( i->wcam_fd < 0 ) {
        err("Can not open Webcam device'%s'", i->wcam_name);
        return -1;
    }

    if( fstat(i->wcam_fd, &st) < 0 ) {
        err("Can not identify'%s'", i->wcam_name);
        return -1;
    }

    if (!S_ISCHR(st.st_mode)) {
        err("'%s' is no char device \n", i->wcam_name);
        return -1;
    }

    dbg("Webcam device '%s' opened successfull", i->wcam_name);
    return 0;
}