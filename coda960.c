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

static char *dbg_type[2] = {"NV12", "H264"};
static char *dbg_status[2] = {"ON", "OFF"};


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
    reqbuf.count = NV12_REQBUF_CNT;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(i->coda_fd, VIDIOC_REQBUFS, &reqbuf);
    if( ret == -1 ) {
        err("Init_NV12: REQBUFS failed [%m]");
        return -1;
    }

    i->buff_nv12_n = reqbuf.count;

    info("Init_NV12: Number of buffers %d (requested %d)",
         i->buff_nv12_n, NV12_REQBUF_CNT);

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


int coda_init_h264(struct Coda_inst *i)
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buf;
    int iter;
    int ret;

    MEMZERO(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.height = i->height;
    fmt.fmt.pix.width = i->width;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    ret = ioctl(i->coda_fd, VIDIOC_S_FMT, &fmt);
    if( ret == -1 ) {
        err("Init_h264: S_FMT failed (%ux%u) [%m]", i->width, i->height);
        return -1;
    }

    info("Init_h264: Set format %ux%u, sizeimage %u, bpl %u, pixelformat:%x",
         fmt.fmt.pix.width, fmt.fmt.pix.height,
         fmt.fmt.pix.sizeimage, fmt.fmt.pix.bytesperline,
         fmt.fmt.pix.pixelformat);
/*
    vid->cap_w = fmt.fmt.pix.width;
    vid->cap_h = fmt.fmt.pix.height;

    vid->cap_buf_size = fmt.fmt.pix.sizeimage;

    vid->cap_buf_cnt_min = 1;
    vid->cap_buf_queued = 0;
*/

    MEMZERO(reqbuf);
    reqbuf.count = H264_REQBUF_CNT;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(i->coda_fd, VIDIOC_REQBUFS, &reqbuf);
    if( ret == -1 ) {
        err("Init_h264: REQBUFS failed [%m]");
        return -1;
    }

    i->buff_264_n = reqbuf.count;

    info("Init_h264: Number of buffers %u (requested %u)",
         i->buff_264_n, H264_REQBUF_CNT);

    for( iter = 0; iter < i->buff_264_n; iter++) {
        MEMZERO(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = iter;

        ret = ioctl(i->coda_fd, VIDIOC_QUERYBUF, &buf);
        if( ret == -1 ) {
            err("Init_h264: QUERYBUF failed [%m]");
            return -1;
        }

        //vid->cap_buf_off[n] = buf.m.offset;
        i->buff_264[iter].length = buf.length;
        //info("------buf.length = %d", buf.length);
        i->buff_264[iter].start = mmap(NULL, buf.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                i->coda_fd, buf.m.offset);

        if( i->buff_264[iter].start == MAP_FAILED) {
            err("Init_h264: MMAP failed [%d]");
            return -1;
        }
    }

    info("Init_h264: Succesfully m-mapped %d buffer(s)", i->buff_264_n);

    return 0;
}


int coda_set_control(struct Coda_inst *i)
{
    struct v4l2_streamparm parm;
    struct v4l2_control cntrl;
    int ret;

    MEMZERO(parm);
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.numerator = 1;
    parm.parm.output.timeperframe.denominator = i->framerate;

    info("Set_ctrl: setting framerate: %u (%u/%u)", i->framerate,
         parm.parm.output.timeperframe.numerator,
         parm.parm.output.timeperframe.denominator);

    ret = ioctl(i->coda_fd, VIDIOC_S_PARM, &parm);
    if( ret == -1 ) {
        err("Set_ctrl: set framerate [%m]");
        return -1;
    }

    // Set bitrate manually (not recommended)
    if (i->bitrate <= 160000000 && i->bitrate >= 32000) {
        MEMZERO(cntrl);
        cntrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
        cntrl.value = i->bitrate;

        info("Set_ctrl: setting bitrate: %d", cntrl.value);

        ret = ioctl(i->coda_fd, VIDIOC_S_CTRL, &cntrl);
        if (ret == -1) {
            err("Set_ctrl: set bitrate [%m]");
            return -1;
        }
    }


    MEMZERO(cntrl);
    cntrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    cntrl.value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
    info("Set_ctrl: setting h264 profile: %d", cntrl.value);
    ret = ioctl(i->coda_fd, VIDIOC_S_CTRL, &cntrl);
    if( ret == -1 ) {
        err("Set_ctrl: set H264_PROFILE [%m]");
        return -1;
    }

    MEMZERO(cntrl);
    cntrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
    cntrl.value = V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
    info("Set_ctrl: setting h264 level: %u", cntrl.value);
    ret = ioctl(i->coda_fd, VIDIOC_S_CTRL, &cntrl);
    if( ret == -1 ) {
        err("Set_ctrl: set H264_LEVEL_4_0 [%m]");
        return -1;
    }


    if (i->num_bframes && i->num_bframes < 4) {
        MEMZERO(cntrl);
        cntrl.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
        cntrl.value = i->num_bframes;
        info("Set_ctrl: setting num B-frames: %d", cntrl.value);
        ret = ioctl(i->coda_fd, VIDIOC_S_CTRL, &cntrl);
        if (ret == -1) {
            err("Set_ctrl: set num B-frames [%m]");
            return -1;
        }
    }

    info("Set_ctrl: Succesfully set all Controls");
    return 0;
}


static int coda_queue_buf(struct Coda_inst *i,
        unsigned int index, unsigned int type)
{
    struct v4l2_buffer buf;
    struct timeval tv;
    int ret;

//    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//    buf.memory = V4L2_MEMORY_MMAP;
//    buf.index = iter;

    MEMZERO(buf);
    buf.type = type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    //buf.length = nplanes;
    //buf.m.bytesused = l1;
    //buf.m.offset = 0;
/*
    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        buf.length = vid->cap_buf_size;
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
*/
    ret = ioctl(i->coda_fd, VIDIOC_QBUF, &buf);
    if( ret == -1 ) {
        err("Failed to queue buffer[%d] on %s [%m]",
            buf.index,
            dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE]);
        return -1;
    }

    dbg("Queued buffer[%d] on %s queue",
        buf.index, dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE]);

    return 0;
}


int coda_queue_buf_h264(struct Coda_inst *i, unsigned int index)
{
    if( index >= i->buff_264_n ) {
        err("Tried to queue a non exisiting buffer");
        return -1;
    }

    int ret = coda_queue_buf(i, index, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if( ret == -1 )
        return -1;

    return 0;
}


int coda_queue_buf_nv12(struct Coda_inst *i, unsigned int index)
{
    if( index >= i->buff_nv12_n) {
        err("Tried to queue a non exisiting buffer");
        return -1;
    }

    int ret = coda_queue_buf(i, index, V4L2_BUF_TYPE_VIDEO_OUTPUT);
    if( ret == -1 )
        return -1;

    return 0;
}



int coda_stream_act(struct Coda_inst *i, unsigned int type,
                    unsigned int action)
{
    int ret;

    ret = ioctl(i->coda_fd, action, &type);
    if( ret == -1 ) {
        err("Failed to change streaming (type=%s, status=%s)",
            dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE],
            dbg_status[action == VIDIOC_STREAMOFF]);
        return -1;
    }

    dbg("Stream %s on %s queue", dbg_status[action == VIDIOC_STREAMOFF],
        dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE]);

    return 0;
}


static int coda_dequeue_buf(struct Coda_inst *i, struct v4l2_buffer *buf)
{
    int ret;

    ret = ioctl(i->coda_fd, VIDIOC_DQBUF, buf);
    if (ret < 0) {
        err("Failed to dequeue buffer [%m]");
        return -1;
    }

    dbg("Dequeued buffer on %s queue with index %d (flags:%x, bytesused:%d)",
        dbg_type[buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE],
        buf->index, buf->flags, buf->bytesused);

    return 0;
}


int coda_dequeue_nv12(struct Coda_inst *i, unsigned int *indx)
{
    struct v4l2_buffer buf;
    int ret;

    MEMZERO(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory = V4L2_MEMORY_MMAP;

    ret = coda_dequeue_buf(i, &buf);
    if (ret < 0)
        return -1;

    *indx = buf.index;

    return 0;
}

int coda_dequeue_h264(struct Coda_inst *i, unsigned int *indx,
                    unsigned int *finished, unsigned int *bytesused,
                    unsigned int *buf_flags)
{
    int ret;
    struct v4l2_buffer buf;

    MEMZERO(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    ret = coda_dequeue_buf(i, &buf);
    if( ret < 0 )
        return -1;

    *finished = 0;

    if( buf.flags & V4L2_BUF_FLAG_LAST || buf.bytesused == 0 )
        *finished = 1;

    *bytesused = buf.bytesused;
    *indx = buf.index;
    //*data_offset = buf.m.offset;

    if (buf_flags)
        *buf_flags = buf.flags;

    return 0;
}

