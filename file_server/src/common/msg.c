#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "msg.h"
#include "sock.h"
#include "log.h"

void init_send_msg(msg_hd_t* pmsg, int msgtype){
    if (msgtype == MSG_COMMAND){
        memset(pmsg, 0x00, sizeof(commsg_t));
        pmsg->msglen = sizeof(commsg_t);
        pmsg->msgtype = MSG_COMMAND;
        pmsg->err = ERR_NONE;
        pmsg->buflen = 0;
    }
    else if (msgtype == MSG_DATA){
        memset(pmsg, 0x00, sizeof(datamsg_t));
        pmsg->msglen = sizeof(datamsg_t);
        pmsg->msgtype = MSG_DATA;
        pmsg->err = ERR_NONE;
    }
}

msg_hd_t* alloc_send_msg(int msgtype){
    msg_hd_t* pmsg;
    if (msgtype == MSG_COMMAND){
        pmsg = (msg_hd_t*)malloc(sizeof(commsg_t));
        init_send_msg(pmsg, msgtype);
    }
    else if (msgtype == MSG_DATA){
        pmsg = (msg_hd_t*)malloc(sizeof(datamsg_t));
        init_send_msg(pmsg, msgtype);
    }

    return pmsg;
}


void free_msg(msg_hd_t* pmsg_hd){
    if (!pmsg_hd){
        free(pmsg_hd);
    }
}

// h(n)ton(h)l & h(n)ton(h)s : host endian to Network endian(network standard : big endian)

static inline void serialize_msg_header(msg_hd_t* pmsg_hd){
    pmsg_hd->msglen  = htonl(pmsg_hd->msglen);
    pmsg_hd->msgtype = htonl(pmsg_hd->msgtype);
    pmsg_hd->op      = htonl(pmsg_hd->op);
    pmsg_hd->err     = htonl(pmsg_hd->err);
    pmsg_hd->buflen  = htonl(pmsg_hd->buflen);
}

static inline void deserialize_msg_header(msg_hd_t* pmsg_hd){
    pmsg_hd->msglen  = ntohl(pmsg_hd->msglen);
    pmsg_hd->msgtype = ntohl(pmsg_hd->msgtype);
    pmsg_hd->op      = ntohl(pmsg_hd->op);
    pmsg_hd->err     = ntohl(pmsg_hd->err);
    pmsg_hd->buflen  = ntohl(pmsg_hd->buflen);
}

static inline void serialize_msg_body(msg_hd_t* pmsg_hd, int msgtype){
    if (msgtype == MSG_COMMAND){
        commsg_t* pmsg = pmsg_hd;
        pmsg->u.sfilefd   = htonl(pmsg->u.sfilefd);
        pmsg->fi.filesize = htonl(pmsg->fi.filesize);
    }
    else if (msgtype == MSG_DATA){
        datamsg_t* pmsg = pmsg_hd;

        pmsg->sfilefd = htonl(pmsg->sfilefd);
        pmsg->offset  = htonl(pmsg->offset);
        pmsg->reqlen = htonl(pmsg->reqlen);
    }
}

static inline void deserialize_msg_body(msg_hd_t* pmsg_hd, int msgtype){
    if (msgtype == MSG_COMMAND){
        commsg_t* pmsg = pmsg_hd;
        pmsg->u.sfilefd   = ntohl(pmsg->u.sfilefd);
        pmsg->fi.filesize = ntohl(pmsg->fi.filesize);
    }
    else if (msgtype == MSG_DATA){
        datamsg_t* pmsg = pmsg_hd;

        pmsg->sfilefd = ntohl(pmsg->sfilefd);
        pmsg->offset  = ntohl(pmsg->offset);
        pmsg->reqlen = htonl(pmsg->reqlen);
    }
}

static inline void serialize_msg(msg_hd_t* pmsg_hd){
    int msgtype =  pmsg_hd->msgtype;

    serialize_msg_header(pmsg_hd);
    serialize_msg_body(pmsg_hd, msgtype);
}

static inline void deserialize_msg(msg_hd_t* pmsg_hd){
    deserialize_msg_header(pmsg_hd);
    deserialize_msg_body(pmsg_hd, pmsg_hd->msgtype);
}


//send msg & byte stream

int send_msg(int socket_fd, msg_hd_t* pmsg, unsigned char* buf){

    int msgtype;
    int msgsize, bufsize;
    int sendlen;

    msgtype = pmsg->msgtype;
    msgsize = pmsg->msglen;
    bufsize = pmsg->buflen;

    if (msgtype >= MSGTYPE_MAX || pmsg == NULL){
        LERR("msg type invalid! : %d \n", msgtype);
        return -1;
    }
    serialize_msg(pmsg);

    sendlen = sock_send(socket_fd, (char*)pmsg, msgsize);

    if (sendlen != msgsize){
        LERR("send msg fail!\n");
        return -1;
    }

    if (bufsize>0 && buf!=NULL) {
        sendlen = sock_send(socket_fd, (char *) buf, bufsize);

        if (sendlen != bufsize) {
            LERR("send buf fail!\n");
            return -1;
        }
    }

    return 0;
}

//recv msg

msg_hd_t* recv_msg(int socket_fd, unsigned char** buf){

    int msgtype;
    int msghdsize, msgsize, bufsize, recvlen;

    msghdsize = sizeof(msg_hd_t);
    msg_hd_t* pmsg_hd = malloc(sizeof(msg_hd_t));

    if (!pmsg_hd) {
        LERR("recv buf malloc fail!\n");
        return NULL;
    }

    recvlen = sock_recv(socket_fd, pmsg_hd, msghdsize);

    if (recvlen != msghdsize){
        if (recvlen != 0) {
            LERR("recv msg header fail!\n");
        }
        free(pmsg_hd);
        return NULL;
    }

    deserialize_msg_header(pmsg_hd);

    msgsize = pmsg_hd->msglen;
    bufsize = pmsg_hd->buflen;

    msg_hd_t* pmsg = malloc((msgsize)*sizeof(char));
    memcpy(pmsg, pmsg_hd, msghdsize);
    free(pmsg_hd);

    recvlen = sock_recv(socket_fd, ((char*)pmsg) + msghdsize, msgsize - msghdsize);
    if (recvlen != msgsize - msghdsize){
        LERR("recv msg fail!\n");
        free(pmsg);
        return NULL;
    }

    if (bufsize > 0) {
        *buf = malloc((bufsize) * sizeof(char));

        recvlen = sock_recv(socket_fd, *buf, bufsize);
        if (recvlen != bufsize) {
            LERR("recv buf fail!\n");
            free(pmsg);
            free(*buf);
            return NULL;
        }
    }
    else if (buf!=NULL){
        *buf = NULL;
    }

    msgtype = pmsg->msgtype;

    if (msgtype >= MSGTYPE_MAX){
        LERR("msg type invalid! : %d \n", msgtype);
        if(pmsg->buflen>0) free(*buf);
        free(pmsg);
        return NULL;
    }

    deserialize_msg_body(pmsg, msgtype);

    return pmsg;
}
