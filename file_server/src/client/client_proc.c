#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "../common/msg.h"
#include "../common/log.h"

#include "client_proc.h"
#include "fileio.h"

void read_proc(int socket_fd){

}


void write_proc(int socket_fd){

}



void put_proc(int socket_fd){

    //get filename from stdin
    char format[10];
    int filesize;
    char filename[FILENAME_LEN_MAX];
    char fileowner[OWNERNAME_LEN_MAX];
    char* buf = malloc(sizeof(char)*FILE_STRIPE);

    sprintf(format, "%%%ds", FILENAME_LEN_MAX);
    scanf(format, filename);
    fflush(stdin);

    //open file
    int filefd = open_rfile(filename, &filesize, fileowner);

    if (filefd<0){
        LERR("open file for read : %s fail!\n", filename);
        return;
    }

    LINFO("open file for read : %s, size : %d Byte, owner : %s\n", filename, filesize, fileowner);

    //declare/alloc msgs

    commsg_t *pmsg_com, *pmsg_com_reply = NULL;
    datamsg_t *pmsg_data, *pmsg_data_reply = NULL;

    pmsg_com = alloc_send_msg(MSG_COMMAND);
    pmsg_data = alloc_send_msg(MSG_DATA);

    int retry_cnt = RETRY_CNT;

    //setup put command msg
    pmsg_com->op = OP_PUT;
    pmsg_com->fi.filesize = filesize;
    strncpy(pmsg_com->fi.filename, filename, FILENAME_LEN_MAX);
    strncpy(pmsg_com->fi.fileowner, fileowner, OWNERNAME_LEN_MAX);

    do{
        //send put command msg
       if(send_msg(socket_fd, pmsg_com, NULL) == -1){
            LERR("send put msg fail!\n");
            goto exit;
        }

        //receive put command reply msg
        if((pmsg_com_reply = recv_msg(socket_fd, NULL)) == NULL){
            LERR("recv put reply msg fail!\n");
            goto exit;
        }

        if (pmsg_com_reply->err == ERR_NONE){
            break;
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LINFO("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is in access, please try again later!\n");
        goto exit;
    }

    printf("put handshake\n");

    int remote_fd = pmsg_com_reply->u.sfilefd;
    int datamsg_cnt = (filesize + FILE_STRIPE - 1) / FILE_STRIPE;
    int offset = 0;
    int remain = filesize;
    int sendlen = 0;

    //send msg & file data stream repeatedly
    do{
        sendlen = remain < FILE_STRIPE ? remain : FILE_STRIPE;

        pmsg_data->op = OP_PUT;
        pmsg_data->buflen = sendlen;

        pmsg_data->sfilefd = remote_fd;
        pmsg_data->offset = offset;

        if (read_file(filefd, buf, sendlen, offset) < 0){
            LERR("read file fail!\n");
            goto exit;
        }

        //send put data msg
        if(send_msg(socket_fd, pmsg_data, buf) == -1){
            LERR("send write data msg fail!\n");
            goto exit;
        }

        //receive put data reply msg
        if ((pmsg_data_reply = recv_msg(socket_fd, NULL)) == NULL){
            LERR("recv write reply msg fail!\n");
            goto exit;
        }

        if (pmsg_data_reply->err != ERR_NONE){
            LERR("receive error reply!\n");
            goto exit;
        }

        printf("\rfile sending... : %s(%dB) %d%%) \n", filename, filesize, offset/(offset+remain));
        fflush(stdout);

        offset += sendlen;
        remain -= sendlen;
        free(pmsg_data_reply);
    } while(--datamsg_cnt>0);

    LINFO("file send done : %s (%dB) \n", filename, filesize);

exit:

    close_file(filefd);

    free(buf);
    free_msg(pmsg_com);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
}

void get_proc(int socket_fd){

    //get filename from stdin
    char format[10];
    int filesize;
    char filename[FILENAME_LEN_MAX];
    char fileowner[OWNERNAME_LEN_MAX];
    char *buf;

    sprintf(format, "%%%ds", FILENAME_LEN_MAX);
    scanf(format, filename);
    fflush(stdin);

    //open file
    int filefd = open_wfile(filename);

    if (filefd<0){
        LERR("open file for write : %s fail!\n", filename);
        return;
    }

    LINFO("open file for write : %s\n", filename);

    //declare/alloc msgs
    commsg_t *pmsg_com, *pmsg_com_reply = NULL;
    datamsg_t *pmsg_data, *pmsg_data_reply = NULL;

    pmsg_com = alloc_send_msg(MSG_COMMAND);
    pmsg_data = alloc_send_msg(MSG_DATA);

    int retry_cnt = RETRY_CNT;

    //setup get command msg
    pmsg_com->op = OP_GET;
    strncpy(pmsg_com->fi.filename, filename, FILENAME_LEN_MAX);

    do{
        //send get command msg
        if(send_msg(socket_fd, pmsg_com, NULL) == -1){
            LERR("send get msg fail!\n");
            goto exit;
        }

        //receive get command reply msg
        if((pmsg_com_reply = recv_msg(socket_fd, NULL)) == NULL){
            LERR("recv get reply msg fail!\n");
            goto exit;
        }

        if (pmsg_com_reply->err == ERR_NONE) break;

        if (pmsg_com_reply->err == ERR_NOFILE){
            LERR("Server : File not found.\n");
            goto exit;
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is in access, please try again later!\n");
        goto exit;
    }

    int remote_fd = pmsg_com_reply->u.sfilefd;
    filesize = pmsg_com_reply->fi.filesize;

    int remain = filesize;
    int recvlen = 0;

    //send data request msg once
    pmsg_data->op = OP_GET;
    pmsg_data->buflen = 0;
    int offset = 0;

    pmsg_data->sfilefd = remote_fd;
    pmsg_data->offset = 0;
    pmsg_data->reqlen = filesize;

    if(send_msg(socket_fd, pmsg_data, NULL) == -1){
        LERR("send put data msg fail!\n");
        goto exit;
    }

    //receive file data stream repeatedly
    while(remain > 0){

        //receive put data reply msg
        if ((pmsg_data_reply = recv_msg(socket_fd, &buf)) == NULL){
            LERR("recv read reply msg fail!\n");
            goto exit;
        }

        recvlen = pmsg_data_reply->buflen;
        offset = pmsg_data_reply->offset;
        free_msg(pmsg_data_reply);

        if (write_file(filefd, buf, recvlen, offset) < 0){
            LERR("write file fail!\n");
            goto exit;
        }

        free(buf);
        remain -= recvlen;
        pmsg_data_reply = NULL;
    }

exit:

    close_file(filefd);

    free(buf);
    free_msg(pmsg_com);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);

}

void list_proc(int socket_fd){}
