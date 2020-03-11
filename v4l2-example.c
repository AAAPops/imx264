/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see https://linuxtv.org/docs.php for more information
 */

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

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct buffer {
    void   *start;
    size_t  length;
};

static char            *dev_name = "/dev/video0";
static enum io_method   io = IO_METHOD_MMAP;
static int              wcam_fd = -1;
static int              get_info = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;

static int              x_res = 640;
static int              y_res = 480;
static int              frame_rate = 10;
static int              frame_count = 100;

FILE                    *file_ptr;
char                    *file_name;

int                     srv_port;
int                     sock_fd = -1;
int                     conn_fd = -1;

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

static void process_image(const void *buff_ptr, int buff_size) {
    // send the buffer to 'stdout'
    if (file_ptr)
        fwrite(buff_ptr, buff_size, 1, file_ptr);

    // send the buffer to client
    if ( conn_fd > 0 ) {
        int n_bytes = write(conn_fd, buff_ptr, buff_size);
        if ( n_bytes != buff_size ) {
            fprintf(stderr, "n_bytes %d == buff_size %d \n", n_bytes, buff_size);
            exit(EXIT_FAILURE);
        }
    }

    fflush(stderr);
    fprintf(stderr, ".");
    fflush(stdout);
}

static int read_frame(void)
{
    struct v4l2_buffer buf;
    unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            if (-1 == read(wcam_fd, buffers[0].start, buffers[0].length)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */

                    default:
                        errno_exit("read");
                }
            }

            process_image(buffers[0].start, buffers[0].length);
            break;

        case IO_METHOD_MMAP:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl(wcam_fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */

                    default:
                        errno_exit("VIDIOC_DQBUF");
                }
            }

            assert(buf.index < n_buffers);

            process_image(buffers[buf.index].start, buf.bytesused);

            if (-1 == xioctl(wcam_fd, VIDIOC_QBUF, &buf))
                errno_exit("VIDIOC_QBUF");
            break;

        case IO_METHOD_USERPTR:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;

                if (-1 == xioctl(wcam_fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                for (i = 0; i < n_buffers; ++i)
                        if (buf.m.userptr == (unsigned long)buffers[i].start
                            && buf.length == buffers[i].length)
                                break;

                assert(i < n_buffers);

                process_image((void *)buf.m.userptr, buf.bytesused);

                if (-1 == xioctl(wcam_fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;
        }

        return 1;
}

static void mainloop(void)
{
    unsigned int count;

    count = frame_count;

    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(wcam_fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(wcam_fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                errno_exit("select");
            }

            if (0 == r) {
                fprintf(stderr, "select timeout \n");
                exit(EXIT_FAILURE);
            }

            if (read_frame())
                break;
            /* EAGAIN - continue select loop. */
        }
    }
}

static void stop_capturing(void)
{
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl(wcam_fd, VIDIOC_STREAMOFF, &type))
                errno_exit("VIDIOC_STREAMOFF");
            break;
    }
}

static void start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (-1 == xioctl(wcam_fd, VIDIOC_QBUF, &buf))
                    errno_exit("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl(wcam_fd, VIDIOC_STREAMON, &type))
                errno_exit("VIDIOC_STREAMON");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)buffers[i].start;
                buf.length = buffers[i].length;

                if (-1 == xioctl(wcam_fd, VIDIOC_QBUF, &buf))
                    errno_exit("VIDIOC_QBUF");
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl(wcam_fd, VIDIOC_STREAMON, &type))
                errno_exit("VIDIOC_STREAMON");
            break;
    }
}

static void uninit_device(void)
{
    unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            free(buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap(buffers[i].start, buffers[i].length))
                    errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free(buffers[i].start);
            break;
    }

    free(buffers);
}

static void init_read(unsigned int buffer_size)
{
    buffers = calloc(1, sizeof(*buffers));

    if (!buffers) {
        fprintf(stderr, "Out of memory \n");
        exit(EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc(buffer_size);

    if (!buffers[0].start) {
        fprintf(stderr, "Out of memory \n");
        exit(EXIT_FAILURE);
    }
}

static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(wcam_fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping \n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s \n", dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(stderr, "Out of memory \n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl(wcam_fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
                mmap(NULL /* start anywhere */,
                        buf.length,
                        PROT_READ | PROT_WRITE /* required */,
                        MAP_SHARED /* recommended */,
                        wcam_fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit("mmap");
    }
}

static void init_userp(unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(wcam_fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/on", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc(4, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc(buffer_size);

                if (!buffers[n_buffers].start) {
                        fprintf(stderr, "Out of memory\\n");
                        exit(EXIT_FAILURE);
                }
        }
}


static void get_device_info(void)
{
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum framesize;

    int index, index2;

    CLEAR(fmtdesc);
    CLEAR(framesize);

    fprintf(stderr, "'%s' support formats: \n", dev_name);

    for( index = 0; ;index++ ) {
        fmtdesc.index = index;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if( ioctl(wcam_fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0 )
            if (errno == EINVAL)
                exit(0);
            else
                errno_exit("VIDIOC_ENUM_FMT");

        fprintf(stderr, " %s \n", fmtdesc.description);
        for( index2 = 0; ;index2++ ) {
            framesize.index = index2;
            framesize.pixel_format = fmtdesc.pixelformat;

            if( ioctl(wcam_fd, VIDIOC_ENUM_FRAMESIZES, &framesize) < 0 )
                if (errno == EINVAL)
                    break;
                 else
                     errno_exit("VIDIOC_ENUM_FRAMESIZES");

            if( framesize.type == V4L2_FRMSIZE_TYPE_DISCRETE ) {
                fprintf(stderr, "\t\t\t %dx%d \n", framesize.discrete.width, framesize.discrete.height);
                continue;
            }

            if( framesize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS ||
                framesize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                fprintf(stderr, "\t\t\t Unsupported format \n");
                continue;
            }
        }
    }
}



static void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    struct v4l2_streamparm streamparm;
    unsigned int min;

    if( ioctl(wcam_fd, VIDIOC_QUERYCAP, &cap) <0 ) {
        fprintf(stderr, "'%s' is no V4L2 device: %m \n",dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device: %m \n",dev_name);
        exit(EXIT_FAILURE);
    }

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


    /* Select video input, video standard and tune here. */
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(wcam_fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(wcam_fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;

                default:
                    /* Errors ignored. */
                    break;
            }
        }
        } else {
                /* Errors ignored. */
        }

    if ( get_info == 1 ) {
        get_device_info();
    }

    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = x_res;
    fmt.fmt.pix.height      = y_res;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if ( ioctl(wcam_fd, VIDIOC_S_FMT, &fmt) < 0 )
        errno_exit("VIDIOC_S_FMT");


    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;


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

    switch (io) {
        case IO_METHOD_READ:
            init_read(fmt.fmt.pix.sizeimage);
            break;

        case IO_METHOD_MMAP:
            init_mmap();
            break;

        case IO_METHOD_USERPTR:
            init_userp(fmt.fmt.pix.sizeimage);
            break;
    }
}

static void close_device(void)
{
    if (-1 == close(wcam_fd))
        errno_exit("close");

    wcam_fd = -1;
}

static void open_device(void)
{
    struct stat st;

    wcam_fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if ( wcam_fd < 0 ) {
        fprintf(stderr, "Cannot open '%s': %m \n", dev_name);
        exit(EXIT_FAILURE);
    }

    if ( fstat(wcam_fd, &st) < 0 ) {
        fprintf(stderr, "Cannot identify '%s': %m \n", dev_name);
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "'%s' is no char device \n", dev_name);
        exit(EXIT_FAILURE);
    }
}

static void start_server(void)
{
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        fprintf(stderr, "Socket creation failed...[%m] \n");
        exit(EXIT_FAILURE);
    } else
        fprintf(stderr, "Socket successfully created...\n");

    int enable = 1;
    if ( setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 )
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed \n");

    bzero(&servaddr, sizeof(servaddr));
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_LOOPBACK for 127.0.0.1
    servaddr.sin_port = htons(srv_port);

    // Binding newly created socket to given IP and verification
    if ((bind(sock_fd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        fprintf(stderr, "Socket bind failed... [%m] \n");
        exit(EXIT_FAILURE);;
    } else
        fprintf(stderr, "Socket successfully binded... \n");

    // Now server is ready to listen and verification
    if ((listen(sock_fd, 1)) != 0) {
        fprintf(stderr, "Listen failed... [%m] \n");
        exit(EXIT_FAILURE);
    } else
        fprintf(stderr, "Server listening on port #%d \n", srv_port);

    int len = sizeof(cli);
    // Accept the data packet from client and verification
    conn_fd = accept(sock_fd, (SA*)&cli, &len);
    if (conn_fd < 0) {
        fprintf(stderr, "Server acccept failed... [%m]\n");
        exit(EXIT_FAILURE);
    } else
        fprintf(stderr, "Server acccept the client...\n");
}


static void usage(FILE *fp, int argc, char **argv)
{
    fprintf(fp,
            "Usage: %s [options] \n\n"
            "Version 1.3\n"
            "Options:\n"
            "-d | --device name   Video device name [%s] \n"
            "-h | --help          Print this message \n"
            "-i | --info          Get device info \n"
            "-s | --server        Start server on 127.0.0.1:[port] \n"
            "-f | --file          Outputs stream to file \n"
            "-x | --x-res         Webcam X resolution [default: 640] \n"
            "-y | --y-res         Webcam Y resolution [default: 480] \n"
            "-r | --frate         Webcam Frame rate [default: 10] \n"
            "-c | --count         Number of frames to grab [%i] \n"
            "",
            argv[0], dev_name, frame_count);
}

static const char short_options[] = "d:his:f:x:y:r:c:";

static const struct option
long_options[] = {
    { "device", required_argument, NULL, 'd' },
    { "help",   no_argument,       NULL, 'h' },
    { "info",   no_argument,       NULL, 'i' },
    { "server", required_argument, NULL, 's' },
    { "file",   required_argument, NULL, 'f' },
    { "x-res",  required_argument, NULL, 'x' },
    { "y-res",  required_argument, NULL, 'y' },
    { "frate",  required_argument, NULL, 'r' },
    { "count",  required_argument, NULL, 'c' },
    { 0, 0, 0, 0 }
};

int main(int argc, char **argv) {
    //dev_name = "/dev/video0";

    for (;;) {
        int idx;
        int c;

        c = getopt_long(argc, argv, short_options, long_options, &idx);

        if (-1 == c)
            break;

        switch (c) {
            case 0: /* getopt_long() flag */
                break;

            case 'd':
                dev_name = optarg;
                break;

            case 'h':
                usage(stdout, argc, argv);
                exit(EXIT_SUCCESS);

            case 'i':
                get_info = 1;
                break;

            case 's':
                errno = 0;
                srv_port = strtol(optarg, NULL, 0);
                if (errno)
                    errno_exit(optarg);
                start_server();
                break;

            case 'x':
                errno = 0;
                x_res = strtol(optarg, NULL, 0);
                if (errno)
                    errno_exit(optarg);
                break;

            case 'y':
                errno = 0;
                y_res = strtol(optarg, NULL, 0);
                if (errno)
                    errno_exit(optarg);
                break;

            case 'f':
                file_name = optarg;
                file_ptr = fopen(file_name, "wb");
                if (!file_ptr)
                    errno_exit(optarg);
                break;

            case 'r':
                errno = 0;
                frame_rate = strtol(optarg, NULL, 0);
                if (errno)
                    errno_exit(optarg);
                break;

            case 'c':
                errno = 0;
                frame_count = strtol(optarg, NULL, 0);
                if (errno)
                    errno_exit(optarg);
                break;

            default:
                usage(stderr, argc, argv);
                exit(EXIT_FAILURE);
        }
    }

    open_device();
    init_device();
    start_capturing();
    mainloop();
    stop_capturing();
    uninit_device();
    close_device();

    // Close Server's socket
    close(sock_fd);

    fprintf(stderr, "\n");
    return 0;
}

