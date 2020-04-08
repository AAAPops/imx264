#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arm_neon.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "webcam.h"

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

int yuyv_to_nv12_neon(char *in_buff_ptr, size_t in_buff_sz,
                      char *out_buff_ptr, size_t out_buff_sz,
                      uint16_t width, uint16_t height)
{
    // Sanity checks first
    {
        if (width % 4 != 0) {
            err("Frame width must be a multiple of 4!");
            return -1;
        }
        if (height % 2 != 0) {
            err("Frame height must be a multiple of 2!");
            return -1;
        }

        u_int8_t macropix = 8;
        uint32_t n_macro_pix = width * height / macropix;

        uint8_t mpix422_sz = 16;
        uint32_t YCrCb_422_sz = n_macro_pix * mpix422_sz;
        if (in_buff_sz != YCrCb_422_sz) {
            err("Input buffer size must be exactly %d bytes", YCrCb_422_sz);
            return -1;
        }

        uint8_t mpix420_sz = 12;
        uint32_t YCrCb_420_sz = n_macro_pix * mpix420_sz;
        if (out_buff_sz < YCrCb_420_sz) {
            err("Output buffer size must be at least %d bytes", YCrCb_420_sz);
            return -1;
        }
    }

    // Main converter body
    int line_n, chunk_n;

    uint8_t *Y_plane_start = (uint8_t*)out_buff_ptr;
    size_t Y_offset = 0;
    uint8_t *CbCr_plane_start = (uint8_t*)(out_buff_ptr + width * height);
    size_t CbCr_offset = 0;

    size_t in_buff_offset = 0;

    int yuyv_line_len = (width / 2) * 4;
    uint8x16x2_t chunk_128x2;


    for( line_n = 0; line_n < height; line_n++ ) {
        for( chunk_n = 0; chunk_n < yuyv_line_len / 32 ; chunk_n++)
        {
            in_buff_offset = yuyv_line_len * line_n + 32 * chunk_n;
            chunk_128x2 = vld2q_u8( (uint8_t*)in_buff_ptr + in_buff_offset);

            vst1q_u8(Y_plane_start + Y_offset, chunk_128x2.val[0]);
            Y_offset += 16;

            if( line_n % 2 == 0 ) {
                vst1q_u8(CbCr_plane_start + CbCr_offset, chunk_128x2.val[1]);
                CbCr_offset += 16;
            }

        }

        if( yuyv_line_len % 32 != 0 ) {
            uint8_t bytes_left = yuyv_line_len % 32;
            in_buff_offset += 32;

            for( uint8_t iter = 0; iter < bytes_left; iter += 4 ) {

                Y_plane_start[Y_offset] = in_buff_ptr[in_buff_offset + 0];
                Y_offset++;

                Y_plane_start[Y_offset] = in_buff_ptr[in_buff_offset + 2];
                Y_offset++;

                if (line_n % 2 == 0) {
                    CbCr_plane_start[CbCr_offset] = in_buff_ptr[in_buff_offset + 1];
                    CbCr_offset++;
                    CbCr_plane_start[CbCr_offset] = in_buff_ptr[in_buff_offset + 3];
                    CbCr_offset++;
                }

                in_buff_offset += 4;
            }
        }

    }
/*
    dbg("Total lines in picture = %d", line_n);
    dbg("Y' plane start pos: 0, end pos: %d [0x%x]", Y_offset - 1, Y_offset - 1);
    dbg("CbCr plane start pos: %d, end pos: %d [0x%x]",
        width * height, width * height + CbCr_offset - 1,
        width * height + CbCr_offset - 1);
*/
    return 0;
}


static int process_image(struct Webcam_inst* i, uint32_t indx)
{
    int ret;

    ret = yuyv_to_nv12_neon(i->buffers[indx].start,
            i->buffers[indx].length,
            i->nv12_buff.start, i->nv12_buff.length,
            i->width, i->height);
    if( ret != 0 )
        return -1;

    fflush(stderr);
    fprintf(stderr, ".");
    fflush(stdout);

    return 0;
}


int wcam_process_new_frame(struct Webcam_inst* i)
{
    int ret;

    struct v4l2_buffer buf;
    MEMZERO(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if( xioctl(i->wcam_fd, VIDIOC_DQBUF, &buf) == -1 ) {
        switch (errno) {
            case EAGAIN:
                return 0;

            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */

            default: {
                err("ioctl(VIDIOC_DQBUF)");
                return -1;
            }
        }
    }

    assert(buf.index < i->buffers_n);

    // По идее, buf.bytesused должно быть равно i->buffers[buf.index].length
    // если же buf.bytesused меньше , то это говорит о том, что вебкамера не смогла
    // заполнить весь буфер целиком и картинка начнет "рваться"
    //i->buffers[buf.index].bytesused = buf.bytesused;

    ret = process_image(i, buf.index);
    if( ret == -1 )
        return -1;

    if( xioctl(i->wcam_fd, VIDIOC_QBUF, &buf) == -1 ){
        err("ioctl(VIDIOC_QBUF)");
        return -1;
    }

    return 0;
}

int wcam_dequeue_buf(struct Webcam_inst *i, unsigned int *index){
    int ret;

    struct v4l2_buffer buf;
    MEMZERO(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if( xioctl(i->wcam_fd, VIDIOC_DQBUF, &buf) == -1 ) {
        switch (errno) {
            case EAGAIN:
                return 0;

            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */

            default: {
                err("ioctl(VIDIOC_DQBUF) [%m]");
                return -1;
            }
        }
    }

    assert(buf.index < i->buffers_n);
    *index = buf.index;

    return 0;
}

int wcam_queue_buf(struct Webcam_inst *i, unsigned int index){
    int ret;

    struct v4l2_buffer buf;
    MEMZERO(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    ret = xioctl(i->wcam_fd, VIDIOC_QBUF, &buf);
    if( ret == -1 ){
        err("ioctl(VIDIOC_QBUF) [%m]");
        return -1;
    }

    return 0;

}

/*
int wcam_mainloop(struct Webcam_inst* i, int peer_fd)
{
    struct timeval tv;

    int iter;
    int ret;


    for( iter = 0; iter < i->frame_count; iter++) {
        if( i->frame_count == 0 )
            iter--;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(i->wcam_fd, &fds);

        // Timeout.
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        ret = select(i->wcam_fd + 1, &fds, NULL, NULL, &tv);
        if( ret == -1 && errno == EINTR )
            continue;
        if ( ret == -1 ) {
            err("select()");
            return -1;
        }
        if( ret == 0 ) {
            err("select() timeout to get one more frame from Webcam");
            exit(EXIT_FAILURE);
        }

        ret = process_new_frame(peer_fd, i);
        if ( ret == -1 )
            return -1;
    }

    return 0;
}
*/

int wcam_start_capturing(struct Webcam_inst* i)
{
    int iter;
    enum v4l2_buf_type type;

    for( iter = 0; iter < i->buffers_n; iter++ ) {
        struct v4l2_buffer buf;

        MEMZERO(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = iter;

        if( xioctl(i->wcam_fd, VIDIOC_QBUF, &buf) == -1 ) {
            err("ioctl(VIDIOC_QBUF)");
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( xioctl(i->wcam_fd, VIDIOC_STREAMON, &type) == -1 ) {
        err("ioctl(VIDIOC_STREAMON)");
        return -1;
    }

    dbg("Webcam device '%s' wcam_start_capturing() successfull", i->wcam_name);
    return 0;
}


static int init_nv12_buff(struct Webcam_inst* i) {
    // Размер картинки по ширине должен быть кратен 4
    // Размер картинки по высоте должен быть кратен 2

    uint32_t n_macro_pix = i->width * i->height / MACROPIX;

    uint32_t YCrCb_444 = n_macro_pix * MPIX444_SZ;
    //info("YCrCb 4:4:4 picture size = %d bytes", YCrCb_444);

    //uint32_t YCrCb_422 = n_macro_pix * MPIX422_SZ;
    //info("YCrCb 4:2:2 picture size = %d bytes", YCrCb_422);

    uint32_t YCrCb_420_sz = n_macro_pix * MPIX420_SZ;
    info("YCrCb 4:2:0 picture size = %d bytes", YCrCb_420_sz);


    i->nv12_buff.length = YCrCb_420_sz;
    i->nv12_buff.start = (char *)calloc(i->nv12_buff.length, sizeof(char));
    if( !i->nv12_buff.start ) {
        err("calloc(nv12_buff)");
        return -1;
    }

    dbg("Webcam device '%s' NV12 buffer init successfull", i->wcam_name);
    return 0;
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
        buff.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buff.memory   = V4L2_MEMORY_MMAP;
        buff.index    = iter;

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
        //info("Webcam buffers[%d].length = %d", iter, buff.length);
    }
    dbg("Webcam device '%s' buffers mmap() successfull", i->wcam_name);

    int ret = init_nv12_buff(i);
    if( ret == -1 )
        return -1;

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
        err("'%s' is no char device", i->wcam_name);
        return -1;
    }

    dbg("Webcam device '%s' opened successfull", i->wcam_name);
    return 0;
}

void wcam_stop_capturing(struct Webcam_inst* i)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( xioctl(i->wcam_fd, VIDIOC_STREAMOFF, &type) == -1 ) {
        err("'%s': ioctl(VIDIOC_STREAMOFF)", i->wcam_name);
    }

    info("Webcam stop capturing");
}

void wcam_uninit(struct Webcam_inst* i) {
    int iter;

    for (iter = 0; iter < i->buffers_n; iter++ ) {
        if( munmap(i->buffers[iter].start, i->buffers[iter].length) == -1 )
            err("'%s': munmap", i->wcam_name);
    }

    free(i->nv12_buff.start);
}

void wcam_close(struct Webcam_inst* i)
{
    if( close(i->wcam_fd) == -1 )
        err("'%s': webcam close", i->wcam_name);

    info("Webcam closed successful");
}