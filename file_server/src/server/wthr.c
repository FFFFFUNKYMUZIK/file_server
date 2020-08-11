#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <libaio.h>

#include "wthr.h"
#include "ipc_msg.h"
#include "msg.h"
#include "filemeta.h"

static io_context_t ioctx;

thr_t wthrs[WTHR_NUM];
thr_t ethr;

bool stop_worker_thread = false;

msg_hd_t*  read_proc(ipc_msg_t* pipc_msg){

    msg_hd_t* preply;
    msg_hd_t* pmsg = pipc_msg->pmsg;

    if (pmsg->msgtype == MSG_COMMAND){
        commsg_t* pcom = pmsg, *pcom_reply;
        pcom_reply = alloc_send_msg(MSG_COMMAND);
        pcom_reply->op = OP_READ_REPLY;

        int fidx;
        if ((fidx = get_file_index_from_list(pcom->fi.filename)) < 0){
            LERR("Cannot add file to list : %s\n", pcom->fi.filename);
            pcom_reply->err = ERR_NOFILE;
            return pcom_reply;
        }

        const file_meta_t* pfm = get_filemeta_by_idx(fidx);
        const file_info_t* pfi = &pfm->fi;

        pcom_reply->u.sfilefd = fidx;
        pcom_reply->fi.filesize = pfi->filesize;
        preply = pcom_reply;
    }
    else if (pmsg->msgtype == MSG_DATA){
        datamsg_t* pdata = pmsg, *pdata_reply;

        const file_meta_t* pfm = get_filemeta_by_idx(pdata->sfilefd);
        const file_info_t* pfi = &pfm->fi;

        int fd = open(pfi->filename, O_RDONLY);
        if (fd<0){
            LERR("file open error\n");
            pdata_reply = alloc_send_msg(MSG_DATA);
            pdata_reply->op = OP_GET_REPLY;
            pdata_reply->buflen = 0;
            pdata_reply->err = ERR_UNKNOWN;
            return pdata_reply;
        }

        int offset = pdata->offset;
        int remain = pdata->reqlen;
        int sendlen;

        while(remain>0) {

            sendlen = remain < FILE_STRIPE ? remain : FILE_STRIPE;

            pdata_reply = alloc_send_msg(MSG_DATA);
            pdata_reply->op = OP_READ_REPLY;
            pdata_reply->buflen = sendlen;
            pdata_reply->offset = offset;

            struct iocb *piocb = malloc(sizeof(struct iocb));
            struct iocb iocb_arr[1] = {piocb};

            ipc_msg_t *pipc_reply = malloc(sizeof(ipc_msg_t));
            pipc_reply->pconn = pipc_msg->pconn;
            pipc_reply->pmsg = pdata_reply;
            pipc_reply->buf = malloc(sendlen);

            io_prep_pread(piocb, fd, (void *) pipc_reply->buf, (size_t) sendlen, (long long) offset);
            piocb->data = pipc_reply;

            if (io_submit(ioctx, 1, iocb_arr) != 1) {
                LERR("io submit error!\n");
                pdata_reply->err = ERR_UNKNOWN;
                free(piocb);
                return pdata_reply;
            }

            offset += sendlen;
            remain -= sendlen;
        }

        preply = NULL;
    }
    return preply;
}

msg_hd_t*  write_proc(ipc_msg_t* pipc_msg){
    msg_hd_t* preply;
    msg_hd_t* pmsg = pipc_msg->pmsg;

    if (pmsg->msgtype == MSG_COMMAND){
        commsg_t* pcom = pmsg, *pcom_reply;
        pcom_reply = alloc_send_msg(MSG_COMMAND);
        pcom_reply->op = OP_WRITE_REPLY;

        int fidx;
        if ((fidx = get_file_index_from_list(pcom->fi.filename)) < 0){
            if ((fidx = add_file_to_list(&pcom->fi))<0){
                LERR("Cannot add file to list : %s\n", pcom->fi.filename);
                pcom_reply->err = ERR_UNKNOWN;
                return pcom_reply;
            }
        }

        const file_meta_t* pfm = get_filemeta_by_idx(fidx);
        const file_info_t* pfi = &pfm->fi;

        pcom_reply->u.sfilefd = fidx;
        pcom_reply->fi.filesize = pfi->filesize;

        preply = pcom_reply;
    }
    else if (pmsg->msgtype == MSG_DATA){
        datamsg_t* pdata = pmsg, *pdata_reply;

        pdata_reply = alloc_send_msg(MSG_DATA);
        pdata_reply->op = OP_WRITE_REPLY;
        pdata_reply->buflen = 0;
        const file_meta_t* pfm = get_filemeta_by_idx(pdata->sfilefd);
        const file_info_t* pfi = &pfm->fi;

        int fd = open(pfi->filename, O_WRONLY, 0644);
        if (fd<0){
            LERR("file open error\n");
            pdata_reply->err = ERR_UNKNOWN;
            return pdata_reply;
        }

        struct iocb * piocb = malloc(sizeof(struct iocb));
        struct iocb iocb_arr[1] = {piocb};

        ipc_msg_t* pipc_reply = malloc(sizeof(ipc_msg_t));
        pipc_reply->pconn = pipc_msg->pconn;
        pipc_reply->pmsg = pdata_reply;

        io_prep_pwrite(piocb, fd, (void *)pipc_msg->buf, (size_t)pdata->buflen, (long long)pdata->offset);
        piocb->data = pipc_reply;

        if (io_submit(ioctx, 1, iocb_arr)!=1){
            LERR("io submit error!\n");
            pdata_reply->err = ERR_UNKNOWN;
            free(piocb);
            return pdata_reply;
        }

        preply = NULL;
    }

    return preply;
}

msg_hd_t* put_proc(ipc_msg_t* pipc_msg){
    msg_hd_t* preply;
    msg_hd_t* pmsg = pipc_msg->pmsg;

    if (pmsg->msgtype == MSG_COMMAND){
        commsg_t* pcom = pmsg, *pcom_reply;
        pcom_reply = alloc_send_msg(MSG_COMMAND);
        pcom_reply->op = OP_PUT_REPLY;

        int fidx;
        if ((fidx = get_file_index_from_list(pcom->fi.filename)) < 0){
            if ((fidx = add_file_to_list(&pcom->fi))<0){
                LERR("Cannot add file to list : %s\n", pcom->fi.filename);
                pcom_reply->err = ERR_UNKNOWN;
                return pcom_reply;
            }
        }
        pcom_reply->u.sfilefd = fidx;
        preply = pcom_reply;
    }
    else if (pmsg->msgtype == MSG_DATA){
        datamsg_t* pdata = pmsg, *pdata_reply;

        pdata_reply = alloc_send_msg(MSG_DATA);
        pdata_reply->op = OP_PUT_REPLY;
        pdata_reply->buflen = 0;
        const file_meta_t* pfm = get_filemeta_by_idx(pdata->sfilefd);
        const file_info_t* pfi = &pfm->fi;

        int fd = open(pfi->filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd<0){
            LERR("file open error\n");
            pdata_reply->err = ERR_UNKNOWN;
            return pdata_reply;
        }

        struct iocb * piocb = malloc(sizeof(struct iocb));
        struct iocb iocb_arr[1] = {piocb};

        ipc_msg_t* pipc_reply = malloc(sizeof(ipc_msg_t));
        pipc_reply->pconn = pipc_msg->pconn;
        pipc_reply->pmsg = pdata_reply;

        io_prep_pwrite(piocb, fd, (void *)pipc_msg->buf, (size_t)pdata->buflen, (long long)pdata->offset);
        piocb->data = pipc_reply;

        if (io_submit(ioctx, 1, iocb_arr)!=1){
            LERR("io submit error!\n");
            pdata_reply->err = ERR_UNKNOWN;
            free(piocb);
            return pdata_reply;
        }

        preply = NULL;
    }

    return preply;
}

msg_hd_t* get_proc(ipc_msg_t* pipc_msg){

    msg_hd_t* preply;
    msg_hd_t* pmsg = pipc_msg->pmsg;

    if (pmsg->msgtype == MSG_COMMAND){
        commsg_t* pcom = pmsg, *pcom_reply;
        pcom_reply = alloc_send_msg(MSG_COMMAND);
        pcom_reply->op = OP_GET_REPLY;

        int fidx;
        if ((fidx = get_file_index_from_list(pcom->fi.filename)) < 0){
            LERR("Cannot add file to list : %s\n", pcom->fi.filename);
            pcom_reply->err = ERR_NOFILE;
            return pcom_reply;
        }

        const file_meta_t* pfm = get_filemeta_by_idx(fidx);
        const file_info_t* pfi = &pfm->fi;

        pcom_reply->u.sfilefd = fidx;
        pcom_reply->fi.filesize = pfi->filesize;
        preply = pcom_reply;
    }
    else if (pmsg->msgtype == MSG_DATA){
        datamsg_t* pdata = pmsg, *pdata_reply;

        const file_meta_t* pfm = get_filemeta_by_idx(pdata->sfilefd);
        const file_info_t* pfi = &pfm->fi;

        int fd = open(pfi->filename, O_RDONLY);
        if (fd<0){
            LERR("file open error\n");
            pdata_reply = alloc_send_msg(MSG_DATA);
            pdata_reply->op = OP_GET_REPLY;
            pdata_reply->buflen = 0;
            pdata_reply->err = ERR_UNKNOWN;
            return pdata_reply;
        }

        int filesize = pfi->filesize;
        int offset = 0;
        int remain = filesize;
        int sendlen;

        while(remain>0) {

            sendlen = remain < FILE_STRIPE ? remain : FILE_STRIPE;

            pdata_reply = alloc_send_msg(MSG_DATA);
            pdata_reply->op = OP_GET_REPLY;
            pdata_reply->buflen = sendlen;
            pdata_reply->offset = offset;

            struct iocb *piocb = malloc(sizeof(struct iocb));
            struct iocb iocb_arr[1] = {piocb};

            ipc_msg_t *pipc_reply = malloc(sizeof(ipc_msg_t));
            pipc_reply->pconn = pipc_msg->pconn;
            pipc_reply->pmsg = pdata_reply;
            pipc_reply->buf = malloc(sendlen);

            io_prep_pread(piocb, fd, (void *) pipc_reply->buf, (size_t) sendlen, (long long) offset);
            piocb->data = pipc_reply;

            if (io_submit(ioctx, 1, iocb_arr) != 1) {
                LERR("io submit error!\n");
                pdata_reply->err = ERR_UNKNOWN;
                free(piocb);
                return pdata_reply;
            }

            offset += sendlen;
            remain -= sendlen;
        }

        preply = NULL;
    }

    return preply;
}

msg_hd_t* list_proc(ipc_msg_t* pipc_msg, unsigned char** buf){

    msg_hd_t* preply;
    msg_hd_t* pmsg = pipc_msg->pmsg;

    if (pmsg->msgtype == MSG_COMMAND){
        commsg_t* pcom = pmsg, *pcom_reply;
        pcom_reply = alloc_send_msg(MSG_COMMAND);
        pcom_reply->op = OP_LIST_REPLY;

        int cnt;

        if ((cnt=get_file_cnt()) == 0){
            LINFO("No file on server \n");
            pcom_reply->err = ERR_NOFILE;
            return pcom_reply;
        }

        pcom_reply->u.filenum = cnt;
        preply = pcom_reply;
    }
    else if (pmsg->msgtype == MSG_DATA){
        datamsg_t* pdata = pmsg, *pdata_reply;

        int cnt = pdata->sfilefd;
        int metasize = sizeof(file_info_t);
        int filesize = cnt * metasize;
        int fidx = 0;

        pdata_reply = alloc_send_msg(MSG_DATA);
        pdata_reply->op = OP_LIST_REPLY;
        pdata_reply->buflen = filesize;

        *buf = malloc(filesize);

        int offset = 0;
        while(fidx<cnt) {
            const file_meta_t *pfm = get_filemeta_by_idx(fidx++);
            const file_info_t *pfi = &pfm->fi;
            memcpy(*buf + offset, pfi, metasize);
            offset+=metasize;
        }

        preply = pdata_reply;
    }

    return preply;
}

msg_hd_t* msg_proc(ipc_msg_t* pipc_msg, unsigned char** buf){

    msg_hd_t* pmsg_reply = NULL;

    switch(pipc_msg->pmsg->op){
        case OP_READ :
            pmsg_reply = read_proc(pipc_msg);
            break;
        case OP_WRITE :
            pmsg_reply = write_proc(pipc_msg);
            break;
        case OP_PUT :
            pmsg_reply = put_proc(pipc_msg);
            break;
        case OP_GET :
            pmsg_reply = get_proc(pipc_msg);
            break;
        case OP_LIST :
            pmsg_reply = list_proc(pipc_msg, buf);
            break;
        default :
            break;
    }

    return pmsg_reply;
}

void* wthr_main(void* arg) {

    thr_arg_t* ppipe =  arg;

    struct pollfd ipc_fd;
    ipc_msg_t ipc_msg;
    ipc_msg_t ipc_msg_reply;

    ipc_fd.fd = ppipe->rpipe;
    ipc_fd.events = POLLIN;

    while(!stop_worker_thread){

        poll(&ipc_fd, 1, POLL_WAIT_TIME);

        if (ipc_fd.revents & POLLIN){
            read(ppipe->rpipe, &ipc_msg, sizeof(ipc_msg_t));
            ipc_msg_reply.pconn = ipc_msg.pconn;
            ipc_msg_reply.pmsg = msg_proc(&ipc_msg, &ipc_msg_reply.buf);

            free(ipc_msg.pmsg);
            if (ipc_msg_reply.pmsg != NULL) {
                write(ppipe->wpipe, &ipc_msg_reply, sizeof(ipc_msg_t));
            }
        }
    }
    free(arg);
}


void* ethr_main(void *arg){

    thr_arg_t* ppipe =  arg;

    ipc_msg_t * pipc_msg_reply;

    if (io_setup(MAX_AIO_EVENT, &ioctx) != 0){
        LERR("io_setup error!\n");
        stop_main_thread = true;
    }

    struct io_event events[MAX_AIO_EVENT];

    struct timespec timeout;
    timeout.tv_sec = POLL_WAIT_TIME/1000;
    timeout.tv_nsec = (POLL_WAIT_TIME%1000)*1000;
    int event_num = 0;
    int i;

    while(!stop_worker_thread){
        if ((event_num = io_getevents(ioctx, 1, MAX_AIO_EVENT, events, &timeout)) > 0){
            for (i=0;i<event_num;i++) {
                pipc_msg_reply = events[i].data;
                write(ppipe->wpipe, pipc_msg_reply, sizeof(ipc_msg_t));

                free(pipc_msg_reply);
            }
        }
    }

    io_destroy(ioctx);
    free(arg);
}