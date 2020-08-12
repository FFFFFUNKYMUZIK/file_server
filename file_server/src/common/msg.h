#ifndef _MSG_H_
#define _MSG_H_

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"
#include "fileinfo.h"

#define FILE_STRIPE (1048576) //(1024*1024*1B) = 1MB

#define RETRY_CNT 3
#define RETRY_SLEEP 1

enum OPTYPE{
    OP_READ = 0,
    OP_WRITE,
    OP_PUT,
    OP_GET,
    OP_LIST,

    //REPLY MSG
    OP_READ_REPLY,
    OP_WRITE_REPLY,
    OP_PUT_REPLY,
    OP_GET_REPLY,
    OP_LIST_REPLY,

    OP_MAX,
};

enum MSGTYPE{
    MSG_COMMAND = 0,
    MSG_DATA,

    MSGTYPE_MAX,
};

enum ERRCODE{
    ERR_NONE,
    ERR_NOFILE,     //READ & WRITE & GET
    ERR_FILEINUSE,  //PUT & GET & READ & WRITE
    ERR_UNKNOWN,

    ERR_MAX,
};

typedef unsigned int uint32_t;
typedef uint32_t optype_t;
typedef uint32_t errtype_t;
typedef uint32_t msgtype_t;

#define MSG_HEADER                          \
        uint32_t msglen;                    \
        msgtype_t msgtype;                  \
        optype_t op;                        \
        errtype_t err;                      \
        uint32_t buflen;

#define COMMSG_BODY                         \
        union{                              \
            uint32_t sfilefd;               \
            uint32_t filenum;               \
        } u;                                \
        file_info_t fi;                     \
        uint32_t offset;                    \
        uint32_t reqlen;

#define DATAMSG_BODY                        \
        uint32_t sfilefd;                   \
        uint32_t offset;                    \
        uint32_t reqlen;


typedef struct msg_hd_s{
    MSG_HEADER;
} msg_hd_t;

typedef struct commsg_s{
    MSG_HEADER;
    COMMSG_BODY;
} commsg_t;

typedef struct datamsg_s{
    MSG_HEADER;
    DATAMSG_BODY;
} datamsg_t;


void init_send_msg(msg_hd_t* pmsg, int msgtype);

msg_hd_t* alloc_send_msg(int msgtype);

void free_msg(msg_hd_t* pmsg_hd);

//send msg & byte stream
int send_msg(int socket_fd, msg_hd_t* pmsg, unsigned char* buf);

//recv msg
msg_hd_t* recv_msg(int socket_fd, unsigned char** buf);



#endif //_MSG_H_
