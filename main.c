#include <stdio.h>
#include <string.h>

#include "common.h"
#include "webcam.h"
#include "args.h"
#include "server.h"



void set_defaults(struct Webcam_inst *wcam_i,struct Srv_inst *srv_i){
    strcpy(wcam_i->wcam_name, "/dev/video2");
    wcam_i->width = 800;
    wcam_i->height = 600;
    wcam_i->frame_rate = 10;
    wcam_i->frame_count = 100;

    strcpy(srv_i->ip, "10.1.91.15");
    srv_i->port = 5100;
}


int main(int argc, char **argv) {
    struct Webcam_inst wcam_inst;
    MEMZERO(wcam_inst);
    struct Srv_inst srv_inst;
    MEMZERO(srv_inst);

    int ret;

    set_defaults(&wcam_inst, &srv_inst);

    ret = pars_args(argc, argv, &wcam_inst, &srv_inst);
    if( ret != 0 ) goto err;

    ret = wcam_open(&wcam_inst);
    if( ret != 0 ) goto err;

    ret = wcam_init(&wcam_inst);
    if( ret != 0 ) goto err;


    return 0;
err:
    return -1;
}