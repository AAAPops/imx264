#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"
#include "server.h"


int srv_tcp_start(struct Srv_inst* i)
{
    struct sockaddr_in servaddr, peer_addr;
    socklen_t peer_addr_size;

    // socket create and verification
    i->srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if( i->srv_fd == -1 ) {
        err("Socket creation failed...[%m]");
        return -1;
    }

    int enable = 1;
    if( setsockopt(i->srv_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 ) {
        err("setsockopt(SO_REUSEADDR) failed");
        return -1;
    }

    MEMZERO(servaddr);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(i->addr); //INADDR_LOOPBACK for 127.0.0.1
    servaddr.sin_port = htons(i->port);

        // Binding newly created socket to given IP and verification
    if ((bind(i->srv_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        err("Socket bind failed... [%m]");
        return  -1;;
    }

    // Now server is ready to listen and verification
    if( listen(i->srv_fd, 1) != 0 ) {
        err("Server listen failed... [%m]");
        return -1;
    }
    info("Server waiting for a client...");

    peer_addr_size = sizeof(struct sockaddr_in);;
    // Accept the data packet from client and verification
    i->peer_fd = accept(i->srv_fd, (struct sockaddr*)&peer_addr, &peer_addr_size);
    if( i->srv_fd < 0 ) {
        err("Client acccept failed... [%m]");
        return -1;
    }

    info("Server acccept the client...");

    return 0;
}


void srv_stop(struct Srv_inst* i) {
    if( close(i->peer_fd) == -1 )
        err("'Srv: peer close()");

    if( close(i->srv_fd) == -1 )
        err("'Srv: server close()");

    info("Server finished successful");
}

int srv_send_data(struct Srv_inst* i, void* buff_ptr, size_t buff_len) {
    // send the buffer to 'stdout'
    //if (file_ptr)
    //    fwrite(buff_ptr, buff_size, 1, file_ptr);

    // send the buffer to client
    int n_bytes = send(i->peer_fd, buff_ptr, buff_len, MSG_NOSIGNAL);
    if( n_bytes != buff_len ) {
        err("Client was not able to receive %d bytes", buff_len);
        return -1;
    }

    return 0;
}


int srv_get_data(struct Srv_inst* i) {

    int n_bytes = recv(i->peer_fd, i->read_buff, sizeof(i->read_buff), 0);
    if( n_bytes == -1 ) {
        err("Some error with client [%m]");
        return -1;
    } else if( n_bytes == 0 ) {
        info("Client closed connection");
        return 0;
    }

    return n_bytes;
}