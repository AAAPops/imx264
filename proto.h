#ifndef INCLUDE_PROTO_H
#define INCLUDE_PROTO_H

#include <stdio.h>
#include <stdint.h>

#define PROTO_HEADER_SZ 6
#define PROTO_MSG_SZ 1024


// Client-Server protocol commands
#define PROTO_CMD_DATA       0
#define PROTO_CMD_HELLO      1
#define PROTO_CMD_SET_PARAM  2
#define PROTO_CMD_GET_PARAM  3
#define PROTO_CMD_START      4
#define PROTO_CMD_STOP       5

// Protocol command's status
#define PROTO_STS_NONE   0
#define PROTO_STS_OK     1
#define PROTO_STS_NOK    255
#define PROTO_STS_ERROR  254
#define PROTO_STS_IDLE   10



struct Proto_inst {
    char        hdr[PROTO_HEADER_SZ];
    uint8_t     cmd;
    uint8_t     status;

    char        msg[PROTO_MSG_SZ];
    uint32_t    msg_len;

    void       *data;
    uint32_t    data_len;
};


int get_peer_msg(struct Srv_inst* i, struct Proto_inst* p);
int send_peer_msg(struct Srv_inst* i, struct Proto_inst* p);
//int get_h264_data(struct Srv_inst* i, struct Proto_inst* p);

int proto_handshake(struct Srv_inst* s, struct Proto_inst* p,
                    struct Webcam_inst* w);

void print_peer_msg(char *label, struct Proto_inst* p);


#endif