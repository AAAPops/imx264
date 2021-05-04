
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include "../common.h"
#include "../webcam.h"
#include "../server.h"
#include "../proto.h"
#include "../log.h"

#define SA struct sockaddr
//#define IP_ADDR "10.1.91.123"
//#define PORT 5100

#define SV_SOCK_PATH    "/tmp/gst-sock"
#define BACKLOG    5

#define FRAME_BUFFER_SZ 5000000 // 5MB
#define QUEUE_MINIMAL_THRESHHOLD 1
#define QUEUE_MAX_ELEMENTS 100

#define H264_BUFF_SZ 1000000 // 1MB

struct Args_inst {
    int     width;
    int     height;
    int     framerate;

    char    binstr[1024];
    int     binstr_len;
};

struct H264_Buffer {
    uint8_t  buff[FRAME_BUFFER_SZ];       // Указатель на начало h264 буфера
    uint32_t buff_total_sz;               //
    uint32_t buff_curr_sz;                // Текущий размер буфера в байтах
    uint32_t h264_idx;                    // Текущее число h264 кадров в буфере
    uint32_t h264_frame_sz[QUEUE_MAX_ELEMENTS]; // Массив, содержащий размер каждого h264 кадра
    uint32_t min_treshhold;               // Минимальная длина буфера в начале работы
};



int make_srv_connect(struct Srv_inst* si) {
    struct sockaddr_in servaddr;
    MEMZERO(servaddr);

    // socket create and verification
    si->peer_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( si->peer_fd == -1 ) {
        log_fatal("Socket creation failed...");
        return -1;
    }
    log_info("Socket successfully created..");

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(si->string);
    servaddr.sin_port = htons(si->port);

    // connect the client socket to server socket
    int ret = connect(si->peer_fd, (SA*)&servaddr, sizeof(servaddr));
    if (ret) {
        log_fatal("Connection with the server failed...");
        return -1;
    }
    log_info("Connected to the server..");

    return 0;
}

void usage(char **argv) {
    fprintf(stderr, "Version %s \n", VERSION);
    fprintf(stderr, "Usage: %s -w Width -h Height -f Framerate [-D debug level] -S ip:port\n",
            argv[0]);

    fprintf(stderr,"Options: \n");
    fprintf(stderr, "\t-w     Frame width resolution [320..1920] \n");
    fprintf(stderr, "\t-h     Frame height resolution [240..1080]\n");
    fprintf(stderr, "\t-f     Framerate [5..30] \n");
    fprintf(stderr, "\t-S     Server [ip:port] to connect to \n");
    fprintf(stderr, "\t-D     Debug level [0..6] \n");
}

int pars_args(int argc, char **argv,
        struct Args_inst *ai, struct Srv_inst *ci){
    int rez;
    int loglevel = 0;
    log_set_level(loglevel);

    ai->width = 0;
    ai->height = 0;
    ai->framerate = 0;

    if( argc == 1 ) {
        usage(argv);
        exit(0);
    }

//	opterr=0;
    while ( (rez = getopt(argc,argv,"w:h:f:D:S:")) != -1){
        switch (rez){
            case 'w':
                ai->width = strtol(optarg, NULL, 10);
                if( ai->width < 320 || ai->width > 1920 ) {
                    log_fatal("A problem with parameter '-w'");
                    return -1;
                }
                break;
            case 'h':
                ai->height = strtol(optarg, NULL, 10);
                if( ai->height < 240 || ai->height > 1080 ) {
                    log_fatal("A problem with parameter '-h'");
                    return -1;
                }
                break;
            case 'f':
                ai->framerate = strtol(optarg, NULL, 10);
                if( ai->framerate < 5 || ai->framerate > 30 ) {
                    log_fatal("A problem with parameter '-f'");
                    return -1;
                }
                break;
            case 'D':
                loglevel = strtol(optarg, NULL, 10);
                if( loglevel < LOG_TRACE || loglevel > LOG_FATAL ) {
                    log_fatal("A problem with parameter '-h'");
                    return -1;
                }
                log_set_level(loglevel);
                break;

            case 'S': {
                const char ch = ':';
                char *colon_ptr;

                colon_ptr = memchr(optarg, ch, strlen(optarg));
                if( colon_ptr ) {
                    *colon_ptr = '\0';

                    strcpy(ci->string, optarg);
                    ci->port = strtol(colon_ptr + 1, NULL, 10);
                } else {
                    log_fatal("A problem with parameter '-S'");
                    return -1;
                }

                break;
            }

            case '?':
            default:
                log_error("Error in arguments found!");
                break;
        }
    }

    log_debug("Normalized Args: -w = %d,  -h = %d,  -f = %d",
         ai->width, ai->height, ai->framerate);
    log_debug("ip:port = %s:%d", ci->string, ci->port);
    if( ai->width == 0 || ai->height == 0 || ai->framerate == 0) {
        usage(argv);
        exit(-1);
    }

    return 0;
}

int serialize_args(struct Args_inst *ai) {
    ai->binstr_len = 0;

    *(uint32_t *) (ai->binstr + ai->binstr_len) = 0;
    ai->binstr_len += sizeof(uint32_t);

    *(uint32_t *) (ai->binstr + ai->binstr_len) = htonl(ai->width);
    ai->binstr_len += sizeof(uint32_t);

    *(uint32_t *) (ai->binstr + ai->binstr_len) = htonl(ai->height);
    ai->binstr_len += sizeof(uint32_t);

    *(uint32_t *) (ai->binstr + ai->binstr_len) = htonl(ai->framerate);
    ai->binstr_len += sizeof(uint32_t);

    log_debug("Packed binary string len = %d", ai->binstr_len);

    return 0;
}

int h264_buff_init(struct H264_Buffer *fb0) {
    int ret = 0;

    MEMZERO(fb0->buff);
    fb0->buff_total_sz = sizeof(fb0->buff);
    fb0->buff_curr_sz = 0;
    fb0->h264_idx = 0;
    MEMZERO(fb0->h264_frame_sz);
    fb0->min_treshhold = 0;

    return ret;
}

int h264_buff_add_new_frame(struct H264_Buffer *fb0,
                            void *raw_frame, uint32_t raw_frame_sz) {
    int ret = 0;

    // Тонна проверок на корректность добавления еще одного h264 кадра

    fb0->h264_idx++;
    fb0->h264_frame_sz[fb0->h264_idx] = raw_frame_sz;

    memcpy(fb0->buff + fb0->buff_curr_sz, raw_frame, raw_frame_sz);

    fb0->buff_curr_sz += raw_frame_sz;


    log_debug("Frames in queue (add): %d, Memory in use: %d", fb0->h264_idx, fb0->buff_curr_sz);

    return ret;
}


int h264_buff_del_first_frame(struct H264_Buffer *fb0) {
    int ret = 0;

    uint32_t first_frame_sz = fb0->h264_frame_sz[1];
    fb0->buff_curr_sz -= first_frame_sz;

    memcpy(fb0->buff, fb0->buff + first_frame_sz, fb0->buff_curr_sz);

    int i;
    for(i = 1; i < fb0->h264_idx; i++ )
        fb0->h264_frame_sz[i] = fb0->h264_frame_sz[i+1];

    fb0->h264_idx--;

    //log_debug("Frames in queue (del): %d, Memory in use: %d", fb0->h264_idx, fb0->buff_curr_sz);

    return ret;
}


int main(int argc, char **argv)
{
    //struct Webcam_inst wcam_inst;
    //MEMZERO(wcam_inst);
    struct Srv_inst clnt_inst;
    MEMZERO(clnt_inst);
    struct Proto_inst proto_inst;
    MEMZERO(proto_inst);
    struct Args_inst args_inst;
    MEMZERO(args_inst);

    struct pollfd pfds;
    int ret;

    char h264_buf[H264_BUFF_SZ];


    struct H264_Buffer fb;
    ret = h264_buff_init(&fb);
    if (ret)
      goto err;


    ret = pars_args(argc, argv, &args_inst, &clnt_inst);
    if (ret)
        goto err;


    ret = make_srv_connect(&clnt_inst);
    if (ret)
        goto err;

    //Send HELLO and greating message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_HELLO;
        strcpy(proto_inst.msg, "Hi, server!");
        proto_inst.msg_len = strlen(proto_inst.msg);
        print_peer_msg("Srv <---", &proto_inst);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg("    --->", &proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_HELLO ||
                    proto_inst.status != PROTO_STS_OK) {
            log_fatal("Get strange answer from server. 'HELLO' expected!");
            goto err;
        }
    }

    //Send GET_PARAM message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_GET_PARAM;
        print_peer_msg("Srv <---", &proto_inst);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg("    --->", &proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_GET_PARAM ||
            proto_inst.status != PROTO_STS_OK) {
            log_fatal("Get strange answer from server. 'GET_PARAM' expected!");
            goto err;
        }
        if( proto_inst.msg_len == 0 ) {
            log_fatal("'GET_PARAM' answer has to have current Webcam settings");
            goto err;
        }
    }

    //Send SET_PARAM message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_SET_PARAM;

        serialize_args(&args_inst);
        proto_inst.msg_len = args_inst.binstr_len;
        memcpy(proto_inst.msg, args_inst.binstr, proto_inst.msg_len);

        print_peer_msg("Srv <---", &proto_inst);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg("    --->", &proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_SET_PARAM ||
            proto_inst.status != PROTO_STS_OK) {
            log_fatal("Get strange answer from server. 'SET_PARAM' expected!");
            goto err;
        }
    }


    sleep(1);
    //Send START message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_START;
        print_peer_msg("Srv <---", &proto_inst);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg("    --->", &proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_START ||
            proto_inst.status != PROTO_STS_OK) {
            log_fatal("Get strange answer from server. 'SET_PARAM' expected!");
            goto err;
        }
    }


    log_info("......Get DATA loop here.....");

    pfds.fd = clnt_inst.peer_fd;
    pfds.events = POLLIN;

    while (1) {
        ret = poll(&pfds, 1, 1000 * TIMEOUT_SEC);
        if (ret == -1) {
            log_fatal("poll: [%m]");
            return -1;

        } else if (ret == 0) {
            log_fatal("poll: Time out");
            return -1;
        }

        if (pfds.revents & POLLIN) {
            // Get DATA
            MEMZERO(proto_inst);
            proto_inst.data = h264_buf;
            ret = get_peer_msg(&clnt_inst, &proto_inst);
            if (ret)
                goto err;

            // Analyze answer from server:
            if (proto_inst.cmd == PROTO_CMD_DATA) {
                print_peer_msg("Srv --->", &proto_inst);

                h264_buff_add_new_frame(&fb, proto_inst.data, proto_inst.data_len);
                fb.min_treshhold++;

                if (fb.min_treshhold >= QUEUE_MINIMAL_THRESHHOLD ) {
                    fb.min_treshhold = QUEUE_MINIMAL_THRESHHOLD;

                    write(STDOUT_FILENO, fb.buff, fb.h264_frame_sz[1]);
                    log_debug("Write to stdout: %d", fb.h264_frame_sz[1]);

                    h264_buff_del_first_frame(&fb);
                }
            } else {
                log_warn("Get strange answer from server. 'DATA' expected!");
                goto err;
            }

        } else {  // POLLERR | POLLHUP
            log_info("peer closed connection");
            return -1;
        }
    }


err:
    close(clnt_inst.peer_fd);
}


/*
int
gst_connection(char *sock_path) {
    struct sockaddr_un addr;
    int sfd, cfd;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        err("socket(AF_UNIX}: [%m]");
    }

    // Construct server socket address, bind socket to it,
    //and make this a listening socket
    if (remove(sock_path) == -1 && errno != ENOENT) {
        err("remove(%s)", sock_path);
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        err("bind(%s), sock_path");
        return -1;
    }

    info("Listen connection on '%s'", sock_path);
    if (listen(sfd, BACKLOG) == -1) {
        err("listen(%s), sock_path");
        return -1;
    }

    // Handle client connections iteratively
    // Accept a connection. The connection is returned on a new
    // socket, 'cfd'; the listening socket ('sfd') remains open
    // and can be used to accept further connections.
    cfd = accept(sfd, NULL, NULL);
    if (cfd == -1) {
        err("accept(%s), sock_path");
        return -1;
    }
    info("Accept incoming connection on '%s'", sock_path);

        // Transfer data from connected socket to stdout until EOF
        //write(STDOUT_FILENO, buf, numRead) != numRead)
        //fatal("partial/failed write");
    return cfd;
}
*/
