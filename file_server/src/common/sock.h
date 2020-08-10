#ifndef _SOCK_H_
#define _SOCK_H_

#include <arpa/inet.h>

typedef struct conn_s{
    struct sockaddr_in cliaddr;
    int socket_fd;
} conn_t;

int open_listen_socket(char* port, int wait);
int connect_server(char* hddr, char* port);
int sock_send(int fd, char* buf, unsigned int size);
int sock_recv(int fd, char* buf, unsigned int size);

#endif //_SOCK_H_
