//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_MSG_H
#define FILE_SERVER_MSG_H

enum {
    OP_READ = 0,
    OP_WRITE,
    OP_PUT,
    OP_GET,
    OP_LIST,

    OP_READ_REPLY,
    OP_WRITE_REPLY,
    OP_PUT_REPLY,
    OP_GET_REPLY,
    OP_LIST_REPLY,

    OP_MAX
} OPTYPE;

#define MSG_HEADER       \
        int msglen;      \
        int optype;      \
        int msgtype;     \
        int buflen;

typedef struct msg_s{
    MSG_HEADER;

} msg_t;

void send_msg(int socket_fd, msg_t* pmsg, unsigned char* buf);
msg_t* recv_msg(int socket_fd, unsigned char** buf);

#endif //FILE_SERVER_MSG_H
