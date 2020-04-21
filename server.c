#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"
#include "server.h"


int srv_srv_start(struct Srv_inst* i)
{
    struct sockaddr_in servaddr;
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

    return 0;
}

int srv_peer_accept(struct Srv_inst* i) {
    info("Server waiting for a client on %s:%d...", i->string, i->port);

    // Accept the data packet from client and verification
    i->peer_fd = accept(i->srv_fd, (struct sockaddr*)NULL, NULL);
    if( i->srv_fd < 0 ) {
        err("Client acccept failed... [%m]");
        return -1;
    }

    info("Server acccept the client...");

    return 0;

}


void srv_srv_stop(struct Srv_inst* i) {
    if( close(i->srv_fd) == -1 )
        err("'Srv: server close()");

    info("Server finished successful");
}

void srv_peer_stop(struct Srv_inst* i) {
    if( close(i->peer_fd) == -1 )
        err("'Srv: peer close()");

    info("Peer closed successful");
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

/*
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
*/

int srv_get_data_1(struct Srv_inst* i, void *buffer, size_t count) {

    struct pollfd pfds;
    int ret;
    size_t n_bytes;
    size_t offset = 0;

    pfds.fd = i->peer_fd;
    pfds.events = POLLIN;

    while (1) {
        ret = poll(&pfds, 1, 1000 * TIMEOUT_SEC);
        if( ret == -1 ) {
            err("poll: [%m]");
            return -1;

        } else if( ret == 0 ) {
            err("poll: Time out");
            return -1;
        }
/*
        printf("  fd=%d; events: %s%s%s%s\n", pfds.fd,
               (pfds.revents & POLLIN)  ? "POLLIN "  : "",
               (pfds.revents & POLLHUP) ? "POLLHUP " : "",
               (pfds.revents & POLLHUP) ? "POLLRDHUP " : "",
               (pfds.revents & POLLERR) ? "POLLERR " : "");
*/
        if (pfds.revents & POLLIN) {
            n_bytes = recv(i->peer_fd, buffer + offset, count, 0);
            if( n_bytes == -1 ) {
                err("recv: [%m]");
                return -1;
            }
            if( n_bytes == 0 ) {
                info("peer closed connection");
                return -1;
            }
        } else {  // POLLERR | POLLHUP
            info("peer closed connection");
            return -1;
        }

        if( n_bytes == count )
            break;

        offset += n_bytes;
        count -= n_bytes;
    }



    return 0;
}

