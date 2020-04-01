#ifndef INCLUDE_ARGS_H
#define INCLUDE_args_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

#include "server.h"
#include "webcam.h"


int pars_args(int argc, char **argv,
        struct Webcam_inst* wcam_i, struct Srv_inst* srv_i);


#endif