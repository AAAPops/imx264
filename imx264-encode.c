#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <netinet/in.h>
#include <sys/socket.h>

#define SA struct sockaddr
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define IO_METHOD_MMAP  0
#define BUFF_MAX        32

struct _buffer {
    void   *start;
    size_t  length;
};

char            *encoder_name = "/dev/video2";
int             encdr_fd = -1;
struct _buffer  srs_buffers[BUFF_MAX];
struct _buffer  out_buffers[BUFF_MAX];
uint8_t         n_srs_buffs;
uint8_t         n_out_buffs;

size_t          x_res = 640;
size_t          y_res = 480;
uint8_t         frame_rate = 10;
//size_t          frame_count = 100;

FILE            *fl_in_ptr, *fl_out_ptr;
char            *fl_in_name =  "/root/stream_640x480x10.yuyv";
char            *fl_out_name = "/root/stream.h264";

//int           srv_port;
//int           sock_fd = -1;
//int           conn_fd = -1;


static void errno_exit(const char *s)
{
    fprintf(stderr, "%s: error %d, %m \n", s, errno);
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static void open_encoder(void)
{
    fprintf(stderr, "info: %s() \n", __func__);
    struct stat st;

    encdr_fd = open(encoder_name, O_RDWR | O_NONBLOCK, 0);
    if ( encdr_fd < 0 ) {
        fprintf(stderr, "Cannot open '%s': %m \n", encoder_name);
        exit(EXIT_FAILURE);
    }

    if ( fstat(encdr_fd, &st) < 0 ) {
        fprintf(stderr, "Cannot identify '%s': %m \n", encoder_name);
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "'%s' is no char device \n", encoder_name);
        exit(EXIT_FAILURE);
    }
}

static void close_encoder(void)
{
    fprintf(stderr, "info: %s() \n", __func__);
    if ( close(encdr_fd) == -1 )
        errno_exit("close");
}

static void init_encoder(void)
{
    fprintf(stderr, "info: %s() \n", __func__);

    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_streamparm streamparm;
    unsigned int min;

    if( xioctl(encdr_fd, VIDIOC_QUERYCAP, &cap) <0 ) {
        fprintf(stderr, "'%s' is no V4L2 device: %m \n",encoder_name);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "cap.capabilities = 0x%x \n", cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        fprintf(stderr, "%s is no video output device: %m \n",encoder_name);
        //exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device: %m \n",encoder_name);
        exit(EXIT_FAILURE);
    }
/*
    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                fprintf(stderr, "%s does not support read i/o \n", dev_name);
                exit(EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf(stderr, "%s does not support streaming i/o \n", dev_name);
                exit(EXIT_FAILURE);
            }
            break;
    }
*/

    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = x_res;
    fmt.fmt.pix.height      = y_res;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if ( ioctl(encdr_fd, VIDIOC_S_FMT, &fmt) < 0 )
        errno_exit("VIDIOC_S_FMT");

/*
    // Set framerate
    CLEAR(streamparm);
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( xioctl(wcam_fd, VIDIOC_G_PARM, &streamparm) != 0 ) {
        fprintf(stderr, "'%s' can not get stream params: %m \n",dev_name);
        exit(EXIT_FAILURE);
    }

    streamparm.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = frame_rate;
    if( xioctl(wcam_fd, VIDIOC_S_PARM, &streamparm) !=0 ) {
        fprintf(stderr, "'%s' can not set frame rate: %m \n",dev_name);
        exit(EXIT_FAILURE);
    }
*/

}


int main(int argc, char **argv) {


    open_encoder();
    init_encoder();


    close_encoder();
}