//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_IPC_MSG_H
#define FILE_SERVER_IPC_MSG_H

#include "sock.h"
#include "msg.h"

typedef struct ipc_msg_s{
    conn_t * pconn;
    msg_t* pmsg;
    unsigned char* buf;
} ipc_msg_t;

#endif //FILE_SERVER_IPC_MSG_H
