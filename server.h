#ifndef INCLUDE_SERVER_H
#define INCLUDE_SERVER_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

struct Srv_inst {
    char    ip[128];
    int     port;
    int     fd;
};

#endif