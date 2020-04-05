#ifndef INCLUDE_ARGS_H
#define INCLUDE_args_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

#include "server.h"
#include "webcam.h"
#include "coda960.h"


int pars_args(int argc, char **argv,
        struct Webcam_inst* wcam_i,
        struct Srv_inst* srv_i,
        struct Coda_inst* coda_i);


#endif