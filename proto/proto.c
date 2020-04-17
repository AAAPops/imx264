#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../common.h"
#include "../server.h"
#include "../webcam.h"
#include "proto.h"


int get_peer_msg(struct Srv_inst* i, struct Proto_inst* p) {
    int ret;

    memset(p, 0, sizeof(struct Proto_inst));
    ret = srv_get_data_1(i, p->hdr, PROTO_HEADER_SZ);
    if (ret)
        return -1;

    p->cmd = *(char*)(p->hdr + 0);
    p->status = *(char*)(p->hdr + 1);
    p->msg_len= ntohl(*(uint32_t*)(p->hdr + 2));

    if( p->msg_len > 0 ) {
        ret = srv_get_data_1(i, p->msg, p->msg_len);
        if (ret)
            return -1;
    }

    return 0;
}


int get_h264_data(struct Srv_inst* i, struct Proto_inst* p) {
    int ret;

    ret = srv_get_data_1(i, p->hdr, PROTO_HEADER_SZ);
    if (ret)
        return -1;

    p->cmd = *(char*)(p->hdr + 0);
    p->status = *(char*)(p->hdr + 1);
    p->data_len= ntohl(*(uint32_t*)(p->hdr + 2));

    if( p->cmd != PROTO_CMD_DATA ) {
        err("'PROTO_CMD_DATA expected, but get:'");
        print_peer_msg(p);
        return -1;
    }

    if( p->data_len > 0 ) {
        ret = srv_get_data_1(i, p->data, p->data_len);
        if (ret)
            return -1;
    }

    return 0;
}


int send_peer_msg(struct Srv_inst* i, struct Proto_inst* p) {
    int ret;

    p->hdr[0] = p->cmd;
    p->hdr[1] = p->status;

    // Send message if BINARY data present
    if( p->cmd == PROTO_CMD_DATA ) {
        *(uint32_t *)(p->hdr + 2) = htonl(p->data_len);

        ret = srv_send_data(i, p->hdr, PROTO_HEADER_SZ);
        if (ret)
            return -1;

        if( p->data_len > 0 ) {
            ret = srv_send_data(i, p->data, p->data_len);
            if (ret)
                return -1;
        }

    } else {    // Send message if TEXT data present
        *(uint32_t *)(p->hdr + 2) = htonl(p->msg_len);

        ret = srv_send_data(i, p->hdr, PROTO_HEADER_SZ);
        if (ret)
            return -1;

        if( p->msg_len > 0 ) {
            ret = srv_send_data(i, p->msg, p->msg_len);
            if (ret)
                return -1;
        }
    }

    return 0;
}


void print_peer_msg(struct Proto_inst* p){
    char cmd[32];
    char status[32];


    if( p->cmd == PROTO_CMD_DATA )
        strcpy(cmd, "DATA");
    else if( p->cmd == PROTO_CMD_HELLO )
        strcpy(cmd, "HELLO");
    else if( p->cmd == PROTO_CMD_SET_PARAM )
        strcpy(cmd, "SET_PARAM");
    else if( p->cmd == PROTO_CMD_GET_PARAM )
        strcpy(cmd, "GET_PARAM");
    else if( p->cmd == PROTO_CMD_START )
        strcpy(cmd, "START");
    else if( p->cmd == PROTO_CMD_STOP )
        strcpy(cmd, "STOP");
    else
        strcpy(cmd, "Unknown Command");


    if( p->status == PROTO_STS_OK )
        strcpy(status, "OK");
    else if( p->status == PROTO_STS_NOK )
        strcpy(status, "NOK");
    else if( p->status == PROTO_STS_ERROR )
        strcpy(status, "ERROR");
    else if( p->status == PROTO_STS_IDLE )
        strcpy(status, "IDLE");
    else if( p->status == PROTO_STS_NONE )
        strcpy(status, "none");
    else
        strcpy(status, "Unknown Status");


    if( p->cmd == PROTO_CMD_DATA ) {
        info("MSG<--- Cmd = %s, Status = %s, DataLen = %d, msg = 'binary data'",
                cmd, status, p->data_len);
    } else
        info("MSG<--- Cmd = %s, Status = %s, MsgLen = %d, msg = '%s'",
             cmd, status, p->msg_len, p->msg);
};


int proto_handshake(struct Srv_inst* si, struct Proto_inst* pi,
                    struct Webcam_inst* wi)
{
    int ret;

    // Handler 'HELLO' msg
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_HELLO ) {
        print_peer_msg(pi);

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_HELLO;
        pi->status = PROTO_STS_OK;
        strcpy(pi->msg, "Hi, client -)");
        pi->msg_len = strlen(pi->msg);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        err("!!!!!!!!!!!!!!!!");
        print_peer_msg(pi);
        return -1;
    }

    // Handler 'GET_PARAM' msg
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_GET_PARAM ) {
        print_peer_msg(pi);

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_GET_PARAM;
        pi->status = PROTO_STS_OK;
        strcpy(pi->msg, "-w 800 -h 600 -r 20 -o other");
        pi->msg_len = strlen(pi->msg);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        err("!!!!!!!!!!!!!!!!");
        print_peer_msg(pi);
        return -1;
    }


    // Handler 'SET_PARAM' msg
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_SET_PARAM ) {
        print_peer_msg(pi);

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_SET_PARAM;
        pi->status = PROTO_STS_OK;
        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        err("!!!!!!!!!!!!!!!!");
        print_peer_msg(pi);
        return -1;
    }

    // Handler 'START' msg
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_START ) {
        print_peer_msg(pi);

        // Do something here to start Webcam capture!!!

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_START;
        pi->status = PROTO_STS_OK;
        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        err("!!!!!!!!!!!!!!!!");
        print_peer_msg(pi);
        return -1;
    }

    // Send h264 DATA
    char bin_data[] = "h264 frame here!!!";
    memset(pi, 0, sizeof(struct Proto_inst));
    pi->cmd = PROTO_CMD_DATA;
    pi->status = PROTO_STS_OK;

    pi->data = bin_data;
    pi->data_len = strlen(bin_data);
    ret = send_peer_msg(si, pi);
    if (ret)
        return -1;

    sleep(5);
    char bin_data_2[] = "Попытка #2!!! h264 frame here!!!";
    memset(pi, 0, sizeof(struct Proto_inst));
    pi->cmd = PROTO_CMD_DATA;
    pi->status = PROTO_STS_OK;

    pi->data = bin_data_2;
    pi->data_len = strlen(bin_data_2);
    ret = send_peer_msg(si, pi);
    if (ret)
        return -1;


    return 0;
}