#ifndef _IPC_MSG_H_
#define _IPC_MSG_H_

#include "../common/sock.h"
#include "../common/msg.h"

typedef struct ipc_msg_s{
    conn_t * pconn;
    msg_hd_t* pmsg;
    unsigned char* buf;
} ipc_msg_t;


#endif //_IPC_MSG_H_
