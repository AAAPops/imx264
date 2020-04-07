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

#include "coda960.h"


/* check is it a decoder video device */
static int is_device_encoder(int fd, const char *name)
{
    struct v4l2_capability cap;
    struct v4l2_fmtdesc fmtdesc;
    int ret;

    MEMZERO(cap);
    if( ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1 ) {
        err("'%s': Failed to verify capabilities [%m]", name);
        return -1;
    }

    dbg("'%s': driver='%s' bus_info='%s' card='%s'",
            name, cap.driver, cap.bus_info, cap.card);
/*
    dbg("caps (%s): driver=\"%s\" bus_info=\"%s\" card=\"%s\" "
        "version=%u.%u.%u", name, cap.driver, cap.bus_info, cap.card,
        (cap.version >> 16) & 0xff,
        (cap.version >> 8) & 0xff,
        cap.version & 0xff);
*/
    if( !(cap.capabilities & V4L2_CAP_STREAMING ) ||
        !(cap.capabilities & V4L2_CAP_VIDEO_M2M)) {
        err("'%s': Insufficient capabilities for video device", name);
        return -1;
    }

    MEMZERO(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    for( fmtdesc.index = 0;; fmtdesc.index++ ) {
        ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
        if( ret == -1 ) {
            err("'%s': does not support NV12 pixel format", name);
            return -1;
        }

        if( fmtdesc.pixelformat == V4L2_PIX_FMT_NV12 )
            break;
    }
    //info("Support input pixel format: '%s'", fmtdesc.description);

    MEMZERO(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for( fmtdesc.index = 0;; fmtdesc.index++ ) {
        ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
        if( ret == -1 ) {
            err("'%s': does not support h264 pixel format", name);
            return -1;
        }

        if( fmtdesc.pixelformat == V4L2_PIX_FMT_H264 )
            break;
    }
    //info("Support output pixel format: '%s'", fmtdesc.description);

    return 0;
}


int coda_open(struct Coda_inst *i)
{
    int ret;

    //dbg("'%s': try to open video encoder device: ", i->coda_name);

    i->coda_fd = open(i->coda_name, O_RDWR, 0);
    if( i->coda_fd < 0 ) {
        err("'%s': failed to open video device", i->coda_name);
        return -1;
    }

    ret = is_device_encoder(i->coda_fd, i->coda_name);
    if (ret < 0)
        return -1;


    info("'%s': is a real nv12-to-h264 encoder", i->coda_name);
    return 0;
}

void coda_close(struct Coda_inst *i)
{
    close(i->coda_fd);

    info("Coda closed successful");
}


int coda_init_nv12(struct Coda_inst *i)
        //unsigned long codec, unsigned int size, unsigned int count)
{
    //struct video *vid = &i->video;
    struct v4l2_format fmt, try_fmt, get_fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buf;
    int ret;
    int iter;

    MEMZERO(try_fmt);
    try_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    try_fmt.fmt.pix.width = i->width;
    try_fmt.fmt.pix.height = i->height;
    try_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

    ret = ioctl(i->coda_fd, VIDIOC_TRY_FMT, &try_fmt);
    if( ret == -1 ) {
        err("Init_NV12: TRY_FMT failed (%ux%u) [%m]",
            try_fmt.fmt.pix.width, try_fmt.fmt.pix.height);
        return -1;
    }

    info("Init_NV12: Try format %ux%u, sizeimage %u, bpl %u",
         try_fmt.fmt.pix.width, try_fmt.fmt.pix.height,
         try_fmt.fmt.pix.sizeimage, try_fmt.fmt.pix.bytesperline);

    MEMZERO(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = i->width;
    fmt.fmt.pix.height = i->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

    ret = ioctl(i->coda_fd, VIDIOC_S_FMT, &fmt);
    if( ret == -1 ) {
        err("Init_NV12: S_FMT failed %ux%u [%m]",
                fmt.fmt.pix.width, fmt.fmt.pix.height);
        return -1;
    }

    info("Init_NV12: Set format %ux%u, sizeimage %u, bpl %u",
         fmt.fmt.pix.width, fmt.fmt.pix.height,
         fmt.fmt.pix.sizeimage, fmt.fmt.pix.bytesperline);


    MEMZERO(get_fmt);
    get_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    get_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;

    ret = ioctl(i->coda_fd, VIDIOC_G_FMT, &get_fmt);
    if (ret) {
        err("Init_NV12: get_fmt failed [%m]");
        return -1;
    }

    info("Init_NV12: Get format %ux%u, sizeimage %u, bpl %u",
         get_fmt.fmt.pix.width, get_fmt.fmt.pix.height,
         get_fmt.fmt.pix.sizeimage,
         get_fmt.fmt.pix.bytesperline);

    //i->nv_12_w = get_fmt.fmt.pix.width;
    //i->nv_12_w  = get_fmt.fmt.pix.height;
    //i->nv_12_w  = get_fmt.fmt.pix.bytesperline;

    MEMZERO(reqbuf);
    reqbuf.count = NV12_BUF_COUNT;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(i->coda_fd, VIDIOC_REQBUFS, &reqbuf);
    if( ret == -1 ) {
        err("Init_NV12: REQBUFS failed [%m]");
        return -1;
    }

    i->buff_nv12_n = reqbuf.count;

    info("Init_NV12: Number of buffers %u (requested %u)",
         i->buff_nv12_n, NV12_BUF_COUNT);

    for( iter = 0; iter < i->buff_nv12_n; iter++) {
        MEMZERO(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = iter;

        ret = ioctl(i->coda_fd, VIDIOC_QUERYBUF, &buf);
        if (ret) {
            err("Init_NV12: QUERYBUF failed [%m]");
            return -1;
        }

        //vid->out_buf_off[n] = buf.m.offset;
        i->buff_nv12[iter].length = buf.length;
        //info("---------> buf.length = %d", buf.length);

        i->buff_nv12[iter].start = mmap(NULL, buf.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                i->coda_fd,  buf.m.offset);

        if( i->buff_nv12[iter].start == MAP_FAILED ) {
            err("Init_NV12: MMAP failed [%m]");
            return -1;
        }

    }

    info("Init_NV12: Succesfully m-mapped %d buffer(s)", i->buff_nv12_n);

    return 0;
}