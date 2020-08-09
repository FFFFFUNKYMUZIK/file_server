//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_SOCK_H
#define FILE_SERVER_SOCK_H

#include <arpa/inet.h>

typedef struct conn_s{
    struct sockaddr_in cliaddr;
    int socket_fd;
} conn_t;

int listen_socket();
int accept();

#endif //FILE_SERVER_SOCK_H
