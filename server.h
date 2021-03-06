#ifndef INCLUDE_SERVER_H
#define INCLUDE_SERVER_H

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

struct Srv_inst {
    char       string[128];
    uint32_t   addr;
    int        port;
    int        srv_fd;
    int        peer_fd;

    int        run_mode;

    uint8_t    read_buff[128];
};



int srv_srv_start(struct Srv_inst* srv_i);
int srv_peer_accept(struct Srv_inst* i);

int srv_send_data(struct Srv_inst* srv_i, void* buff_ptr, size_t buff_len);
int srv_get_data(struct Srv_inst* i);

void srv_srv_stop(struct Srv_inst* i);
void srv_peer_stop(struct Srv_inst* i);

int srv_get_data_1(struct Srv_inst* i, void *buffer, size_t count);

#endif