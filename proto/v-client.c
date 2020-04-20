
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common.h"
#include "../webcam.h"
#include "../server.h"
#include "../proto.h"

#define SA struct sockaddr
#define IP_ADDR "10.1.91.123"
#define PORT 5100

#define H264_BUFF_SZ 5*1000000 // 5MB


int make_connection(struct Srv_inst* i) {
    struct sockaddr_in servaddr;
    MEMZERO(servaddr);

    // socket create and varification
    i->peer_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (i->peer_fd == -1) {
        err("Socket creation failed...");
        return -1;
    }
    info("Socket successfully created..");

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(IP_ADDR);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    int ret = connect(i->peer_fd, (SA*)&servaddr, sizeof(servaddr));
    if (ret) {
        err("Connection with the server failed...");
        return -1;
    }
    info("Connected to the server..");

    return 0;
}


int main()
{
    struct Webcam_inst wcam_inst;
    MEMZERO(wcam_inst);
    struct Srv_inst clnt_inst;
    MEMZERO(clnt_inst);
    struct Proto_inst proto_inst;
    MEMZERO(proto_inst);

    struct pollfd pfds;
    int ret;

    char h264_buf[H264_BUFF_SZ];

    ret = make_connection(&clnt_inst);
    if (ret) {
        err("Connection with the server failed...");
        goto err;
    }

    //Send HELLO and greating message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_HELLO;
        strcpy(proto_inst.msg, "Hi, server!");
        proto_inst.msg_len = strlen(proto_inst.msg);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg(&proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_HELLO ||
                    proto_inst.status != PROTO_STS_OK) {
            err("Get strange answer from server. 'HELLO' expected!");
            goto err;
        }
    }

    //Send GET_PARAM message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_GET_PARAM;
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg(&proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_GET_PARAM ||
            proto_inst.status != PROTO_STS_OK) {
            err("Get strange answer from server. 'GET_PARAM' expected!");
            goto err;
        }
        if( proto_inst.msg_len == 0 ) {
            err("'GET_PARAM' answer has to have current Webcam settings");
            goto err;
        }
    }

    //Send SET_PARAM message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_SET_PARAM;
        strcpy(proto_inst.msg, "-w 1024 -h 768 -r 10");
        proto_inst.msg_len = strlen(proto_inst.msg);
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg(&proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_SET_PARAM ||
            proto_inst.status != PROTO_STS_OK) {
            err("Get strange answer from server. 'SET_PARAM' expected!");
            goto err;
        }
    }


    sleep(2);
    //Send START message
    {
        MEMZERO(proto_inst);
        proto_inst.cmd = PROTO_CMD_START;
        ret = send_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;

        ret = get_peer_msg(&clnt_inst, &proto_inst);
        if (ret)
            goto err;
        print_peer_msg(&proto_inst);
        // Analyze answer from server:
        if( proto_inst.cmd != PROTO_CMD_START ||
            proto_inst.status != PROTO_STS_OK) {
            err("Get strange answer from server. 'SET_PARAM' expected!");
            goto err;
        }
    }


    info("......Get DATA loop here.....");

    pfds.fd = clnt_inst.peer_fd;
    pfds.events = POLLIN;

    while (1) {
        ret = poll(&pfds, 1, 1000 * TIMEOUT_SEC);
        if (ret == -1) {
            err("poll: [%m]");
            return -1;

        } else if (ret == 0) {
            err("poll: Time out");
            return -1;
        }

        if (pfds.revents & POLLIN) {
            // Get DATA
            MEMZERO(proto_inst);
            proto_inst.data = h264_buf;
            ret = get_h264_data(&clnt_inst, &proto_inst);
            if (ret)
                goto err;

            // Analyze answer from server:
            if (proto_inst.cmd != PROTO_CMD_DATA) {
                err("Get strange answer from server. 'DATA' expected!");
                goto err;
            }
            print_peer_msg(&proto_inst);

        } else {  // POLLERR | POLLHUP
            info("peer closed connection");
            return -1;
        }
    }


err:
    close(clnt_inst.peer_fd);
}
