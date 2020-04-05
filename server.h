#ifndef INCLUDE_SERVER_H
#define INCLUDE_SERVER_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

struct Srv_inst {
    char    ip[128];
    int     port;
    int     srv_fd;
    int     peer_fd;
};



int srv_tcp_start(struct Srv_inst* srv_i);

void srv_stop(struct Srv_inst* i);

#endif