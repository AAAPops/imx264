
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common.h"

#define IP_ADDR "10.1.91.123"
#define PORT 5100
#define SA struct sockaddr

// Client-Server protocol commands
#define PROTO_CMD_DATA       0
#define PROTO_CMD_HELLO      1
#define PROTO_CMD_SET_PARAM  2
#define PROTO_CMD_GET_PARAM  3
#define PROTO_CMD_START      4
#define PROTO_CMD_STOP       5

// Statuses of protocol commands
#define PROTO_STS_OK     0
#define PROTO_STS_NOK    255
#define PROTO_STS_ERROR  1
#define PROTO_STS_IDLE   10
#define PROTO_STS_START  11
#define PROTO_STS_STOP   12

/*
void func(int sockfd)
{
    char buff[MAX];
    int n;
    for (;;) {
        bzero(buff, sizeof(buff));
        printf("Enter the string : ");
        n = 0;
        while ((buff[n++] = getchar()) != '\n')
            ;
        write(sockfd, buff, sizeof(buff));
        bzero(buff, sizeof(buff));
        read(sockfd, buff, sizeof(buff));
        printf("From Server : %s", buff);
        if ((strncmp(buff, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            break;
        }
    }
}
*/

int main()
{
    int ret;
    int sock_fd;
    struct sockaddr_in servaddr;

    // socket create and varification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    info("sock_fd = %d", sock_fd);
    if (sock_fd == -1) {
        err("Socket creation failed...");
        return -1;
    }
    info("Socket successfully created..");

    MEMZERO(servaddr);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(IP_ADDR);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    ret = connect(sock_fd, (SA*)&servaddr, sizeof(servaddr));
    if (ret) {
        err("Connection with the server failed...");
        return -1;
    }
    info("Connected to the server..");

    sleep(1);
    close(sock_fd);
}
