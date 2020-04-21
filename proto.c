#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "server.h"
#include "webcam.h"
#include "proto.h"


int get_peer_msg(struct Srv_inst* i, struct Proto_inst* p)
{
    int ret;

    ret = srv_get_data_1(i, p->hdr, PROTO_HEADER_SZ);
    if (ret)
        return -1;

    p->cmd = *(char*)(p->hdr + 0);
    p->status = *(char*)(p->hdr + 1);

    if( p->cmd == PROTO_CMD_DATA ) {
        p->data_len= ntohl(*(uint32_t*)(p->hdr + 2));

        if( p->data_len > 0 ) {
            ret = srv_get_data_1(i, p->data, p->data_len);
            if (ret)
                return -1;
        }
    } else {
        p->msg_len = ntohl(*(uint32_t *) (p->hdr + 2));

        if (p->msg_len > 0) {
            ret = srv_get_data_1(i, p->msg, p->msg_len);
            if (ret)
                return -1;
        }
    }

    return 0;
}

/*
static int get_h264_data(struct Srv_inst* i, struct Proto_inst* p) {
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
*/

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


void print_peer_msg(char *label, struct Proto_inst* p){
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
        info("%s Cmd = %s, Status = %s, DataLen = %d, data = 'binary data'",
                label, cmd, status, p->data_len);
    } else
        info("%s Cmd = %s, Status = %s, MsgLen = %d, msg = '%s'",
             label, cmd, status, p->msg_len, p->msg);
};




int proto_handshake(struct Srv_inst* si, struct Proto_inst* pi,
                    struct Webcam_inst* wi)
{
    int ret;

    // Handler 'HELLO' msg
    memset(pi, 0, sizeof(struct Proto_inst));
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_HELLO ) {
        print_peer_msg("Peer <---", pi);

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_HELLO;
        pi->status = PROTO_STS_OK;
        strcpy(pi->msg, "Hi, client -)");
        pi->msg_len = strlen(pi->msg);
        print_peer_msg("     --->", pi);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        print_peer_msg("!!!!!", pi);
        return -1;
    }

    // Handler 'GET_PARAM' msg
    memset(pi, 0, sizeof(struct Proto_inst));
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_GET_PARAM ) {
        print_peer_msg("Peer <---", pi);

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_GET_PARAM;
        pi->status = PROTO_STS_OK;
        strcpy(pi->msg, "-w 800 -h 600 -r 20 -o other");
        pi->msg_len = strlen(pi->msg);
        print_peer_msg("     --->", pi);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        print_peer_msg("!!!!!", pi);
        return -1;
    }


    // Handler 'SET_PARAM' msg
    memset(pi, 0, sizeof(struct Proto_inst));
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_SET_PARAM ) {
        print_peer_msg("Peer <---", pi);

        if( pi->msg_len > 0 ) {

            int offset = sizeof(uint32_t);
            wi->width = ntohl( *(uint32_t*)(pi->msg + offset) );

            offset += sizeof(uint32_t);
            wi->height = ntohl( *(uint32_t*)(pi->msg + offset) );

            offset += sizeof(uint32_t);
            wi->frame_rate = ntohl( *(uint32_t*)(pi->msg + offset) );

            info("Peer <--- New Webcam settings: -w=%d,  -h=%d,  -f=%d",
                 wi->width, wi->height, wi->frame_rate);
        }

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_SET_PARAM;
        pi->status = PROTO_STS_OK;
        print_peer_msg("     --->", pi);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        print_peer_msg("!!!!!", pi);
        return -1;
    }

    // Handler 'START' msg
    memset(pi, 0, sizeof(struct Proto_inst));
    ret = get_peer_msg(si, pi);
    if (ret)
        return -1;

    if( pi->cmd == PROTO_CMD_START ) {
        print_peer_msg("Peer <---", pi);

        // Do something here to start Webcam capture!!!

        memset(pi, 0, sizeof(struct Proto_inst));
        pi->cmd = PROTO_CMD_START;
        pi->status = PROTO_STS_OK;
        print_peer_msg("     --->", pi);

        ret = send_peer_msg(si, pi);
        if (ret)
            return -1;

    } else {
        print_peer_msg("!!!!!", pi);
        return -1;
    }


    return 0;
}