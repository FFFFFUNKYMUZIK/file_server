//
// Created by cjh on 20. 8. 9..
//

#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <libaio.h>


#include "wthr.h"
#include "ipc_msg.h"
#include "msg.h"

static io_context_t ioctx;

thr_t wthrs[WTHR_NUM];
bool stop_worker_thread = false;

void read_proc(){




}


void write_proc(){



}

void put_proc(){



}

void get_proc(){


}

void list_proc(){



}

msg_t* msg_proc(msg_t* pmsg){

    switch(pmsg->optype){
        case OP_READ :
            read_proc(pmsg);
            break;
        case OP_WRITE :
            write_proc(pmsg);
            break;
        case OP_PUT :
            put_proc(pmsg);

            break;
        case OP_GET :
            get_proc(pmsg);


            break;
        case OP_LIST :
            list_proc(pmsg);
            return
            break;
    }

return NULL;
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
            ipc_msg_reply.pmsg = msg_proc(ipc_msg.pmsg);

            free(ipc_msg.pmsg);
            write(ppipe->wpipe, &ipc_msg_reply, sizeof(ipc_msg_t));
        }
    }
    free(arg);
}


void* ethr_main(void *arg){

    thr_arg_t* ppipe =  arg;

    ipc_msg_t ipc_msg;

    if (io_setup(MAX_AIO_EVENT, &ioctx) != MAX_AIO_EVENT){
        stop_worker_thread = true;
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
                events[0].data
            }
        }
    }

    io_destroy(ioctx);
    free(arg);
}