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

    uint8_t read_buff[128];
};



int srv_tcp_start(struct Srv_inst* srv_i);
int srv_send_data(struct Srv_inst* srv_i, void* buff_ptr, size_t buff_len);
int srv_get_data(struct Srv_inst* i);

void srv_stop(struct Srv_inst* i);

#endif