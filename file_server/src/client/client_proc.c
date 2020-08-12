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

    //get filename from stdin
    char format[10];
    char filename[FILENAME_LEN_MAX];

    sprintf(format, "%%%ds", FILENAME_LEN_MAX);
    scanf(format, filename);
    fflush(stdin);

    int fileoffset;
    int buflen;

    scanf("%d", &fileoffset);
    fflush(stdin);

    if (fileoffset<0){
        LERR("invalid offset value!\n");
        return;
    }

    scanf("%d", &buflen);
    fflush(stdin);

    if (buflen<0 || buflen > MAX_READ_BUF_SIZE){
        LERR("invalid size value!\n");
        return;
    }

    char buf[MAX_READ_BUF_SIZE + 1];

    //declare/alloc msgs
    commsg_t *pmsg_com, *pmsg_com_reply = NULL;
    datamsg_t *pmsg_data, *pmsg_data_reply = NULL;

    pmsg_com = alloc_send_msg(MSG_COMMAND);
    pmsg_data = alloc_send_msg(MSG_DATA);

    int retry_cnt = RETRY_CNT;

    //setup get command msg
    pmsg_com->op = OP_READ;
    pmsg_com->offset = fileoffset;
    pmsg_com->reqlen = buflen;

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

        if (pmsg_com_reply->err == ERR_UNKNOWN){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : Unknown error... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is not available, please try again later!\n");
        goto exit;
    }

    int remote_fd = pmsg_com_reply->u.sfilefd;
    int filesize = pmsg_com_reply->fi.filesize;

    int remain = filesize < fileoffset+buflen?  filesize-fileoffset : buflen;
    if (remain <= 0){
        LERR("Invalid offset value!\n");
        goto exit;
    }
    int total = remain;
    int recvlen = 0;

    //send data request msg once
    pmsg_data->op = OP_READ;
    pmsg_data->buflen = 0;
    int bufoffset;

    pmsg_data->sfilefd = remote_fd;
    pmsg_data->offset = fileoffset;
    pmsg_data->reqlen = remain;

    if(send_msg(socket_fd, pmsg_data, NULL) == -1){
        LERR("send read data msg fail!\n");
        goto exit;
    }

    char* tmpbuf;
    //receive file data stream repeatedly
    while(remain > 0){

        //receive put data reply msg
        if ((pmsg_data_reply = recv_msg(socket_fd, &tmpbuf)) == NULL){
            LERR("recv read reply msg fail!\n");
            goto exit;
        }

        if (pmsg_data_reply->err != ERR_NONE){
            LERR("receive error reply!\n");
            free(tmpbuf);
            goto exit;
        }

        recvlen = pmsg_data_reply->buflen;
        bufoffset = pmsg_data_reply->offset - fileoffset;
        free_msg(pmsg_data_reply);
        pmsg_data_reply = NULL;

        memcpy(buf+bufoffset, tmpbuf, recvlen);
        free(tmpbuf);
        remain -= recvlen;

        int percent = 100 - remain*100/total;
        printf("\rFile Receiving : %s [", filename);
        int progress;
        for (progress = 0;progress<20;progress++) printf("%c", progress < (100-percent)/5 ? ' ' : '<');
        printf("] (%d/%d B) %d%%", total-remain, total, percent);
        fflush(stdout);
    }

    printf("\n");
    printf("Reading file : \n");
    int i;
    for (i=0;i<total;i++) putchar(buf[i]);

    printf("\n\n");
    LINFO("file read done : %s (%d B) \n", filename, filesize);

    exit:

    free_msg(pmsg_com);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);
}


void write_proc(int socket_fd){

    //get filename from stdin
    char format[10];
    char filename[FILENAME_LEN_MAX];

    sprintf(format, "%%%ds", FILENAME_LEN_MAX);
    scanf(format, filename);
    fflush(stdin);

    int fileoffset;
    int buflen = 0;
    scanf("%d", &fileoffset);
    fflush(stdin);

    if (fileoffset<0){
        LERR("invalid offset value!\n");
        return;
    }

    char buf[MAX_WRITE_BUF_SIZE + 1];
    char c;
    while ((c = getc(stdin)) != '\n'){
        if (c=='\"'){
            while ((c = getc(stdin)) != '\n' && c != '\"' && buflen < MAX_WRITE_BUF_SIZE){
                buf[buflen++] = c;
            }
            buf[buflen] = '\0';

            if (buflen==0 || c == '\n'){
                LERR("invalid buffer input!\n");
                fflush(stdin);
                return;
            }
        }
    }

    if (buflen == MAX_WRITE_BUF_SIZE){
        LERR("Too long buffer input!\n");
        return;
    }

    int offset = fileoffset;

    LINFO("write filename : %s, size : %d Byte, offset : %d\n", filename, buflen, offset);

    //declare/alloc msgs
    commsg_t *pmsg_com, *pmsg_com_reply = NULL;
    datamsg_t *pmsg_data, *pmsg_data_reply = NULL;

    pmsg_com = alloc_send_msg(MSG_COMMAND);
    pmsg_data = alloc_send_msg(MSG_DATA);

    int retry_cnt = RETRY_CNT;

    //setup put command msg
    pmsg_com->op = OP_WRITE;
    pmsg_com->offset = fileoffset;
    pmsg_com->reqlen = buflen;
    strncpy(pmsg_com->fi.filename, filename, FILENAME_LEN_MAX);

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

        if (pmsg_com_reply->err == ERR_UNKNOWN){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LINFO("Server : Unknown error... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LINFO("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is not available, please try again later!\n");
        goto exit;
    }

    int remote_fd = pmsg_com_reply->u.sfilefd;
    int filesize = pmsg_com_reply->fi.filesize;

    int remain = filesize < fileoffset+buflen?  filesize-fileoffset : buflen;
    if (remain <= 0){
        LERR("Invalid offset value!\n");
        goto exit;
    }
    int sendlen = 0;
    int bufoffset = 0;

    //send msg & file data stream repeatedly
    while(remain > 0) {

        init_send_msg(pmsg_data, MSG_DATA);
        sendlen = remain < FILE_STRIPE ? remain : FILE_STRIPE;

        pmsg_data->op = OP_WRITE;
        pmsg_data->buflen = sendlen;

        pmsg_data->sfilefd = remote_fd;
        pmsg_data->offset = fileoffset;

        //send put data msg
        if (send_msg(socket_fd, pmsg_data, buf + bufoffset) == -1) {
            LERR("send write data msg fail!\n");
            goto exit;
        }

        //receive put data reply msg
        if ((pmsg_data_reply = recv_msg(socket_fd, NULL)) == NULL) {
            LERR("recv write reply msg fail!\n");
            goto exit;
        }

        if (pmsg_data_reply->err != ERR_NONE) {
            LERR("receive error reply!\n");
            goto exit;
        }

        fileoffset += sendlen;
        bufoffset += sendlen;
        remain -= sendlen;
        free(pmsg_data_reply);

        int percent = (buflen-remain) * 100 / buflen;
        printf("\rFile writing : %s [", filename);
        int progress;
        for (progress = 0; progress < 20; progress++) printf("%c", progress < percent / 5 ? '>' : ' ');
        printf("] (%d/%d B) %d%%", buflen-remain, buflen, percent);
        fflush(stdout);
    }

    printf("\n");
    LINFO("file write done : %s, size : %d Byte, offset : %d\n", filename, buflen, offset);

    exit:

    free_msg(pmsg_com);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
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

        if (pmsg_com_reply->err == ERR_UNKNOWN){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LINFO("Server : Unknown error... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LINFO("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is not available, please try again later!\n");
        goto exit;
    }

    int remote_fd = pmsg_com_reply->u.sfilefd;
    int datamsg_cnt = (filesize + FILE_STRIPE - 1) / FILE_STRIPE;
    int offset = 0;
    int remain = filesize;
    int sendlen = 0;

    //send msg & file data stream repeatedly
    do{
        init_send_msg(pmsg_data, MSG_DATA);
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

        offset += sendlen;
        remain -= sendlen;
        free(pmsg_data_reply);

        int percent = offset*100/filesize;
        printf("\rFile sending : %s [", filename);
        int progress;
        for (progress = 0;progress<20;progress++) printf("%c", progress < percent/5 ? '>' : ' ' );
        printf("] (%d/%d B) %d%%", offset, filesize, percent);
        fflush(stdout);
    } while(--datamsg_cnt>0);

    printf("\n");
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

        if (pmsg_com_reply->err == ERR_UNKNOWN){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : Unknown error... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        if (pmsg_com_reply->err == ERR_FILEINUSE){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : File in use... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }

        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("The remote file is not available, please try again later!\n");
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

        if (pmsg_data_reply->err != ERR_NONE){
            LERR("receive error reply!\n");
            free(buf);
            goto exit;
        }

        recvlen = pmsg_data_reply->buflen;
        offset = pmsg_data_reply->offset;
        free_msg(pmsg_data_reply);
        pmsg_data_reply = NULL;

        if (write_file(filefd, buf, recvlen, offset) < 0){
            LERR("write file fail!\n");
            free(buf);
            goto exit;
        }

        free(buf);
        remain -= recvlen;

        int percent = 100 - remain*100/filesize;
        printf("\rFile Receiving : %s [", filename);
        int progress;
        for (progress = 0;progress<20;progress++) printf("%c", progress < (100-percent)/5 ? ' ' : '<');
        printf("] (%d/%d B) %d%%", filesize-remain, filesize, percent);
        fflush(stdout);
    }

    printf("\n");
    LINFO("file receive done : %s (%d B) \n", filename, filesize);

exit:

    close_file(filefd);

    free_msg(pmsg_com);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);

}

void list_proc(int socket_fd){

    int metasize;
    char *buf;

    LINFO("get file list from server...\n");

    //declare/alloc msgs
    commsg_t *pmsg_com, *pmsg_com_reply = NULL;
    datamsg_t *pmsg_data, *pmsg_data_reply = NULL;

    pmsg_com = alloc_send_msg(MSG_COMMAND);
    pmsg_data = alloc_send_msg(MSG_DATA);

    int retry_cnt = RETRY_CNT;

    //setup get command msg
    pmsg_com->op = OP_LIST;

    do{
        //send get command msg
        if(send_msg(socket_fd, pmsg_com, NULL) == -1){
            LERR("send list msg fail!\n");
            goto exit;
        }

        //receive get command reply msg
        if((pmsg_com_reply = recv_msg(socket_fd, NULL)) == NULL){
            LERR("recv list reply msg fail!\n");
            goto exit;
        }

        if (pmsg_com_reply->err == ERR_NONE) break;

        if (pmsg_com_reply->err == ERR_NOFILE){
            printf("no file found!\n");
            goto exit;
        }

        if (pmsg_com_reply->err == ERR_UNKNOWN){
            free_msg(pmsg_com_reply);
            pmsg_com_reply = NULL;
            LERR("Server : Unknown error... retry(%d/%d)\n", retry_cnt, RETRY_CNT);
        }
        sleep(RETRY_SLEEP);

    } while(--retry_cnt>0);

    if (retry_cnt == 0){
        LINFO("Unknown error occurs, please try again later!\n");
        goto exit;
    }

    int remote_file_cnt = pmsg_com_reply->u.filenum;
    metasize = sizeof(file_info_t);

    int remain = metasize*remote_file_cnt;
    int recvlen = 0;

    //send data request msg once
    pmsg_data->op = OP_LIST;
    pmsg_data->buflen = 0;

    pmsg_data->sfilefd = remote_file_cnt;

    if(send_msg(socket_fd, pmsg_data, NULL) == -1){
        LERR("send list data msg fail!\n");
        goto exit;
    }

    //receive put data reply msg
    if ((pmsg_data_reply = recv_msg(socket_fd, &buf)) == NULL){
        LERR("recv list reply msg fail!\n");
        goto exit;
    }

    if (pmsg_data_reply->err != ERR_NONE){
        LERR("receive error reply!\n");
        free(buf);
        goto exit;
    }

    recvlen = pmsg_data_reply->buflen;
    if (recvlen != remain){
        LERR("reply buffer size invalid!\n");
        free(buf);
        goto exit;
    }

    //print table contents

    char format[FILENUMBER_WIDTH + FILENAME_WIDTH + FILESIZE_WIDTH + FILEOWNER_WIDTH + 10];

    sprintf(format, "|%%%d.%ds|%%%d.%ds|%%%d.%ds|%%%d.%ds|\n", FILENUMBER_WIDTH, FILENUMBER_WIDTH,
            FILENAME_WIDTH, FILENAME_WIDTH,
            FILESIZE_WIDTH, FILESIZE_WIDTH,
            FILEOWNER_WIDTH, FILEOWNER_WIDTH);

    printf("\n");
    printf(format, "FILE.NO", "FILENAME", "FILESIZE(B)", "FILEOWNER");


    sprintf(format, "|%%%dd|%%%d.%ds|%%%dd|%%%d.%ds|\n", FILENUMBER_WIDTH,
            FILENAME_WIDTH, FILENAME_WIDTH,
            FILESIZE_WIDTH,
            FILEOWNER_WIDTH, FILEOWNER_WIDTH);

    int fileidx;
    file_info_t* pfi = (file_info_t*)buf;;

    for (fileidx=1;fileidx<=remote_file_cnt;pfi++){
        if (strlen(pfi->filename) > FILENAME_WIDTH){
            pfi->filename[FILENAME_WIDTH-1] = '.';
            pfi->filename[FILENAME_WIDTH-2] = '.';
            pfi->filename[FILENAME_WIDTH-3] = '.';
        }
        if (strlen(pfi->fileowner) > FILEOWNER_WIDTH){
            pfi->fileowner[FILEOWNER_WIDTH-1] = '.';
            pfi->fileowner[FILEOWNER_WIDTH-2] = '.';
            pfi->fileowner[FILEOWNER_WIDTH-3] = '.';
        }
        printf(format, fileidx++, pfi->filename, pfi->filesize, pfi->fileowner);
    }

    free_msg(pmsg_data_reply);
    pmsg_data_reply = NULL;
    free(buf);

    printf("\n");
    LINFO("list receive done : %d files \n", fileidx-1);

    exit:

    free_msg(pmsg_com);
    if (pmsg_com_reply) free_msg(pmsg_com_reply);
    free_msg(pmsg_data);
    if (pmsg_data_reply) free_msg(pmsg_data_reply);
}
