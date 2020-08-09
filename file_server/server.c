#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include "config.h"
#include "sock.h"
#include "wthr.h"
#include "ipc_msg.h"
#include "msg.h"
#include "msg_queue.h"

conn_t connections[MAX_CLIENT];
bool stop_main_thread = false;

static inline void send_msg_to_client(ipc_msg_t* pipc_msg){
    send_msg(pipc_msg->pconn->socket_fd, pipc_msg->pmsg, pipc_msg->buf);
    free(pipc_msg->pmsg);
    if (pipc_msg->pmsg->buflen > 0) free(pipc_msg->buf);
}

void assign_job(ipc_msg_t* pipc_msg){

    for (int i=0;i<WTHR_NUM;i++){
        if (!wthrs[i].isrun){
            write(wthrs[i].wpipe, pipc_msg, sizeof(ipc_msg_t));
        }
    }

    ipc_msg_t* pipc_msg_alloc = malloc(sizeof(ipc_msg_t));
    memcpy(pipc_msg_alloc, pipc_msg, sizeof(ipc_msg_t));

    enqueue(pipc_msg_alloc);
}

int get_new_job(ipc_msg_t* pipc_msg){

    ipc_msg_t* pipc_msg_alloc = dequeue();

    if (pipc_msg_alloc == NULL){

        return -1;
    }

    memcpy(pipc_msg, pipc_msg_alloc, sizeof(ipc_msg_t));
    free(pipc_msg_alloc);
    return 0;
}


int create_thrs(int tnum, thr_t thrarr[]){

    int i;
    int wpipe[2], rpipe[2];
    thr_arg_t* arg;

    if (pipe(wpipe) < 0 || pipe(rpipe) <0 ){
        return -1;
    }

    //wpipe[0] : wthr(read)
    //wpipe[1] : cthr(write)
    //rpipe[0] : cthr(read)
    //rpipe[1] : wthr(write)

    for (i=0;i<WTHR_NUM;i++){
        arg = malloc(sizeof(thr_arg_t));
        arg->rpipe = wpipe[0];
        arg->wpipe = rpipe[1];
        wthrs[i].rpipe = rpipe[0];
        wthrs[i].wpipe = wpipe[1];

        if (pthread_create(&wthrs[i].tid, NULL, wthr_main, arg) > 0){
            goto out;
        }
        wthrs[i].isrun = false;
    }

    return 0;

    out:

        free(arg);
        return -1;
}

int main() {

    //msg
    ipc_msg_t ipc_msg;


    if (create_thrs(WTHR_NUM, &wthrs[0])<0){
        return -1;
    }

    if (create_thrs(1, &ethr)<0){
        return -1;
    }

    queue_init();

    //accept_client;
    int sfd = listen_socket();

    // setup fd array
    // server socket, wthr, ethr, client
    struct pollfd fds[1 + WTHR_NUM + 1 + MAX_CLIENT];
    int tot_fd = WTHR_NUM+MAX_CLIENT+2;

    int i=0;
    //register server listen socket fd to poll fd array
    fds[i].fd = sfd;
    fds[i].events = POLLIN;

    //register wthr listen pipe to poll fd array
    for (;i<WTHR_NUM;i++){
        fds[i].fd = wthrs[i].rpipe;
        fds[i].events = POLLIN;
    }
    //register ethr listen pipe to poll fd array
    fds[i].fd = ethr.rpipe;
    fds[i].events = POLLIN;

    int nread = 0;
    int maxidx = i;
    int clientidx = i+1;

    socklen_t clilen = sizeof(struct sockaddr_in);

    for (;i<tot_fd;i++){
        fds[i].fd = -1;
    }

    //main loop
    while(!stop_main_thread){
        nread = poll(fds, maxidx + 1, POLL_WAIT_TIME);

        i = 0;
        //server socket
        if (fds[i].revents & POLLIN){

            int c, fdc = 0;
            for (c=0;c<MAX_CLIENT;c++){
                fdc = clientidx+c;
                if ( fds[fdc].fd <0 ){
                    fds[fdc].fd = accept(sfd,
                                      (struct sockaddr *)&connections[c].cliaddr,
                                      &clilen);
                    fds[fdc].events = POLLIN;
                    break;
                }
            };
            if (c==MAX_CLIENT){

                continue;
            }
            if (fdc>maxidx) maxidx = fdc;
            if (--nread == 0) continue;
        }

        //worker thread
        for (i=1;i<WTHR_NUM+1;i++){
            if (fds[i].revents & (POLLIN | POLLERR)){
                read(fds[i].fd, &ipc_msg, sizeof(ipc_msg));
                if (ipc_msg.pmsg != NULL) {
                    send_msg_to_client(&ipc_msg);
                }

                if (get_new_job(&ipc_msg)<0){
                    wthrs[i-1].isrun = false;
                }
                else{
                    write(wthrs[i-1].wpipe, &ipc_msg, sizeof(ipc_msg));
                }

                if (--nread<=0) break;
            }
        }

        //event thread
        if (fds[i].revents & (POLLIN | POLLERR)){
            read(fds[i].fd, &ipc_msg, sizeof(ipc_msg));
            send_msg_to_client(&ipc_msg);
            if (--nread<=0) continue;
        }

        //client socket
        for (i=clientidx;i<maxidx;i++){

            if (fds[i].revents & (POLLIN | POLLERR)){

                ipc_msg.pconn = &connections[i-clientidx];
                ipc_msg.pmsg = recv_msg(fds[i].fd, &ipc_msg.buf);

                if (ipc_msg.pmsg == NULL){
                    fds[i].fd = -1;
                    continue;
                }
                assign_job(&ipc_msg);

                if (--nread<=0) break;
            }
        }

        break;

    }

    stop_worker_thread = true;

    int status;
    for (i=0;i<WTHR_NUM;i++){
        pthread_join(wthrs[i].tid, &status);
    }

    pthread_join(ethr.tid, &status);

    queue_destroy();

    return 0;
}
