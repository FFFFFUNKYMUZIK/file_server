#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "sock.h"
#include "log.h"

int open_listen_socket(char* port, int wait){
    int sfd;
    struct sockaddr_in server_addr;

    //create TCP server socket
    if ((sfd = socket(AF_INET, SOCK_STREAM,0)) == -1){
        LERR("Server : cannot open socket stream\n");
        exit(1);
    }

    int option = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option) );

    memset(&server_addr, 0, sizeof(server_addr));

    //set server address
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr= htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(port));

    //bind socket
    if (bind(sfd, (struct sockaddr*) & server_addr, sizeof(server_addr)) == -1){
        LERR("Server : cannot bind socket\n");
        exit(1);
    }

    //listen socket
    if (listen(sfd, wait) == -1){
        LERR("Server : cannot listen connection request\n");
        exit(1);
    }

    return sfd;
}



int connect_server(char* haddr, char* port){

    int cfd;
    struct sockaddr_in server_addr;

    if ((cfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        LERR("Cannot create socket\n");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(haddr);
    server_addr.sin_port = htons(atoi(port));
    int ret;

    if ((ret = connect(cfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) == -1){
        LERR("Cannot connect server\n");
        exit(1);
    }

    return cfd;
}


int sock_send(int fd, char* buf, unsigned int size){

    int send_len = 0;
    int offset = 0;
    int remain = size;

    while(remain > 0 && ((send_len = write(fd, buf + offset, remain)) > 0)){
        remain-=send_len;
        offset+=send_len;
    }

    return offset;
}


int sock_recv(int fd, char* buf, unsigned int size){

    int recv_len = 0;
    int offset = 0;
    int remain = size;

    while(remain > 0 && ((recv_len = read(fd, buf + offset, remain)) > 0)){
        remain-=recv_len;
        offset+=recv_len;
    }

    return offset;
}
