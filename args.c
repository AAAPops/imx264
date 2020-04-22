#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "common.h"
#include "log.h"
#include "args.h"

const char short_options[] = "d:?iP:F:w:h:f:c:D:b";

const struct option
        long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, '?' },
        { "info",   no_argument,       NULL, 'i' },
        { "port",   required_argument, NULL, 'P' },
        { "file",   required_argument, NULL, 'F' },
        { "width",  required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "frate",  required_argument, NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { "debug",  required_argument, NULL, 'D' },
        { "background",  required_argument, NULL, 'b' },
        { 0, 0, 0, 0 }
};


static void set_defaults(struct Webcam_inst *wcam_i,
        struct Srv_inst *srv_i, struct Coda_inst *coda_i ){
    strcpy(wcam_i->wcam_name, "/dev/video2");
    wcam_i->width = 800;
    wcam_i->height = 600;
    wcam_i->frame_rate = 10;
    wcam_i->frame_count = 100;

    strcpy(srv_i->string, "loopback");
    srv_i->port = 5100;
    srv_i->run_mode = FOREGROUND;

    strcpy(coda_i->coda_name, "/dev/video0");
    coda_i->bitrate = 0;
    //coda_i->num_bframes = 10;
}


void usage(char **argv,
           struct Webcam_inst* wcam_i, struct Srv_inst* srv_i) {
    fprintf(stderr, "Version %s \n", VERSION);
    fprintf(stderr, "Usage: %s -d %s  -w %d -h %d -f %d -c %d -P %s:%d [-D] \n\n",
            argv[0],
            wcam_i->wcam_name, wcam_i->width, wcam_i->height,
            wcam_i->frame_rate, wcam_i->frame_count,
            srv_i->string, srv_i->port);

    fprintf(stderr,"Options: \n");
    fprintf(stderr, "\t-d | --device name   Webcam device name \n");
    fprintf(stderr, "\t   | --help          Print this message \n");
    fprintf(stderr, "\t-i | --info          Get webcam info \n");
    fprintf(stderr, "\t-P | --port          Listen on [127.0.0.1]:port [1024..65535]\n");
    fprintf(stderr, "\t-F | --file          Output stream to file \n");
    fprintf(stderr, "\t-w | --width         Frame width resolution [320..1920] \n");
    fprintf(stderr, "\t-h | --height        Frame height resolution [240..1080]\n");
    fprintf(stderr, "\t-f | --frate         Framerate [5..30] \n");
    fprintf(stderr, "\t-c | --count         Number of frames to grab [0 - run forever] \n");
    fprintf(stderr, "\t-b | --background    Run in background mode \n");
    fprintf(stderr, "\t-D | --debug         Debug level [0..6] \n");
}


int pars_args(int argc, char **argv, struct Webcam_inst* wcam_i,
        struct Srv_inst* srv_i, struct Coda_inst* coda_i)
{
    int loglevel = 0;
    log_set_level(LOG_INFO);

    set_defaults(wcam_i, srv_i, coda_i);

    if( argc == 1 ) {
        usage(argv, wcam_i, srv_i);
        exit(0);
    }

    for (;;) {
        int idx;
        int c;
        c = getopt_long(argc, argv, short_options, long_options, &idx);

        if( c == -1 )
            break;

        switch (c) {
            case 0: // getopt_long() flag
                break;

            case 'd':
                strcpy(wcam_i->wcam_name, optarg);
                break;

            case '?':
                usage(argv, wcam_i, srv_i);
                exit(0);

            case 'i':
                wcam_i->get_info = 1;
                break;

            case 'P': {
                const char ch = ':';
                char *colon_ptr;

                colon_ptr = memchr(optarg, ch, strlen(optarg));
                if( colon_ptr ) {
                    srv_i->addr = INADDR_LOOPBACK;
                    strcpy(srv_i->string, "127.0.0.1");
                    srv_i->port = strtol(colon_ptr + 1, NULL, 10);
                } else {
                    srv_i->addr = INADDR_ANY;
                    strcpy(srv_i->string, "*");
                    srv_i->port = strtol(optarg, NULL, 10);
                }

                if( srv_i->port < 1024 || srv_i->port > 65535 ) {
                    log_fatal("A problem with parameter '--port'");
                    return -1;
                }

                break;
            }

            case 'w':
                wcam_i->width = strtol(optarg, NULL, 10);
                if( wcam_i->width < 320 || wcam_i->width > 1920 ) {
                    log_fatal("A problem with parameter '--width'");
                    return -1;
                }
                break;

            case 'h':
                wcam_i->height = strtol(optarg, NULL, 10);
              if( wcam_i->height < 240 || wcam_i->height > 1080 ) {
                  log_fatal("A problem with parameter '--height'");
                  return -1;
              }
              break;

            case 'F':
                log_fatal("'--file' option is not implemented yet");
                return -1;
                break;

            case 'f':
                wcam_i->frame_rate = strtol(optarg, NULL, 10);
                if( wcam_i->frame_rate < 5 || wcam_i->frame_rate > 30 ) {
                    log_fatal("A problem with parameter '--frate'");
                    return -1;
                }
                break;

            case 'c':
                wcam_i->frame_count = strtol(optarg, NULL, 10);
                if( wcam_i->frame_count < 0 || wcam_i->frame_count > 100000 ) {
                    log_fatal("A problem with parameter '--count'");
                    return -1;
                }
                break;

            case 'D':
                loglevel = strtol(optarg, NULL, 10);
                if( loglevel < LOG_TRACE || loglevel > LOG_FATAL ) {
                    log_fatal("A problem with parameter '-D'");
                    return -1;
                }
                log_set_level(loglevel);
                break;

            case 'b':
                srv_i->run_mode = BACKGROUND;
                log_set_quiet(BACKGROUND);
                break;

            default:
                usage(argv, wcam_i, srv_i);
                exit(0);
        }
    }

    log_info("Will use: %s -d %s  -w %d -h %d -f %d -c %d -P %s:%d -D%d \n",
            argv[0],
            wcam_i->wcam_name, wcam_i->width, wcam_i->height,
            wcam_i->frame_rate, wcam_i->frame_count,
            srv_i->string, srv_i->port, loglevel);

    return  0;
}
