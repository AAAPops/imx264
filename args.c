#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "args.h"

const char short_options[] = "d:?iS:F:w:h:f:c:";

const struct option
        long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, '?' },
        { "info",   no_argument,       NULL, 'i' },
        { "server", required_argument, NULL, 'S' },
        { "file",   required_argument, NULL, 'F' },
        { "width",  required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'h' },
        { "frate",  required_argument, NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { 0, 0, 0, 0 }
};


static void set_defaults(struct Webcam_inst *wcam_i,
        struct Srv_inst *srv_i, struct Coda_inst *coda_i ){
    strcpy(wcam_i->wcam_name, "/dev/video2");
    wcam_i->width = 800;
    wcam_i->height = 600;
    wcam_i->frame_rate = 10;
    wcam_i->frame_count = 100;

    strcpy(srv_i->ip, "10.1.91.15");
    srv_i->port = 5100;

    strcpy(coda_i->coda_name, "/dev/video0");
    coda_i->bitrate = 5000000;
    //coda_i->num_bframes = 10;
}


void usage(char **argv,
           struct Webcam_inst* wcam_i, struct Srv_inst* srv_i) {
    fprintf(stderr, "Version %s \n", VERSION);
    fprintf(stderr, "Usage: %s -d %s  -w %d -h %d -f %d -c %d -S %s:%d [-F] \n\n",
            argv[0],
            wcam_i->wcam_name, wcam_i->width, wcam_i->height,
            wcam_i->frame_rate, wcam_i->frame_count,
            srv_i->ip, srv_i->port);

    fprintf(stderr,"Options: \n");
    fprintf(stderr, "\t-d | --device name   Webcam device name \n");
    fprintf(stderr, "\t   | --help          Print this message \n");
    fprintf(stderr, "\t-i | --info          Get webcam info \n");
    fprintf(stderr, "\t-S | --server        Send UDP stream to host \n");
    fprintf(stderr, "\t-F | --file          Output stream to file \n");
    fprintf(stderr, "\t-w | --width         Frame width resolution [320..1920] \n");
    fprintf(stderr, "\t-h | --height        Frame height resolution [240..1080]\n");
    fprintf(stderr, "\t-f | --frate         Framerate [5..30] \n");
    fprintf(stderr, "\t-c | --count         Number of frames to grab \n");
}


int pars_args(int argc, char **argv, struct Webcam_inst* wcam_i,
        struct Srv_inst* srv_i, struct Coda_inst* coda_i) {

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

            case 'S':
                err("'--server' option: you can not manually change the settings");
                return -1;
                break;

            case 'w':
                wcam_i->width = strtol(optarg, NULL, 10);
                if( wcam_i->width < 320 || wcam_i->width > 1920 ) {
                    err("A problem with parameter '--width'");
                    return -1;
                }
                break;

            case 'h':
                wcam_i->height = strtol(optarg, NULL, 10);
              if( wcam_i->height < 240 || wcam_i->height > 1080 ) {
                  err("A problem with parameter '--height'");
                  return -1;
              }
              break;

            case 'F':
                err("'--file' option is not implemented yet");
                return -1;
                break;

            case 'f':
                wcam_i->frame_rate = strtol(optarg, NULL, 10);
                if( wcam_i->frame_rate < 5 || wcam_i->frame_rate > 30 ) {
                    err("A problem with parameter '--frate'");
                    return -1;
                }
                break;

            case 'c':
                wcam_i->frame_count = strtol(optarg, NULL, 10);
                if( wcam_i->frame_count < 1 || wcam_i->frame_count > 10000 ) {
                    err("A problem with parameter '--count'");
                    return -1;
                }
                break;

            default:
                usage(argv, wcam_i, srv_i);
                exit(0);
        }
    }

    dbg("Will use: %s -d %s  -w %d -h %d -f %d -c %d -S %s:%d \n",
            argv[0],
            wcam_i->wcam_name, wcam_i->width, wcam_i->height,
            wcam_i->frame_rate, wcam_i->frame_count,
            srv_i->ip, srv_i->port);

    return  0;
}
