#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>

#include "../common/config.h"
#include "../common/msg.h"
#include "../common/sock.h"
#include "../common/log.h"

#include "wthr.h"
#include "ipc_msg.h"
#include "msg_queue.h"
#include "filemeta.h"

int sfd;

conn_t connections[MAX_CLIENT];
bool stop_main_thread = false;

void msg_free(void* pipc_msg){
    ipc_msg_t* type_pipc_msg = pipc_msg;
    free(type_pipc_msg->pmsg);
    if (type_pipc_msg->pmsg->buflen != 0) free(type_pipc_msg->buf);
}

static inline void send_msg_to_client(ipc_msg_t* pipc_msg){

    send_msg(pipc_msg->pconn->socket_fd, pipc_msg->pmsg, pipc_msg->buf);
    LINFO("send msg to client : fd(%d), OP(%d)\n", pipc_msg->pconn->socket_fd, pipc_msg->pmsg->op);
    msg_free(pipc_msg);
}

void assign_job(ipc_msg_t* pipc_msg){

    for (int i=0;i<WTHR_NUM;i++){
        if (!wthrs[i].isrun){
            int ret;
            ret = write(wthrs[i].wpipe, pipc_msg, sizeof(ipc_msg_t));
            wthrs[i].isrun = true;
            return;
        }
    }

    ipc_msg_t* pipc_msg_alloc = malloc(sizeof(ipc_msg_t));
    memcpy(pipc_msg_alloc, pipc_msg, sizeof(ipc_msg_t));

    enqueue(pipc_msg_alloc);
}

int get_new_job(ipc_msg_t* pipc_msg){

    ipc_msg_t* pipc_msg_alloc = dequeue();

    if (pipc_msg_alloc == NULL){
        LINFO("no job in job queue\n");
        return -1;
    }

    memcpy(pipc_msg, pipc_msg_alloc, sizeof(ipc_msg_t));
    free(pipc_msg_alloc);
    return 0;
}


int create_thrs(int tnum, thr_t thrarr[], void* (*thrfunc)(void*)){

    int i;
    int wpipe[2], rpipe[2];
    thr_arg_t* arg;

    for (i=0;i<tnum;i++){

        //wpipe[0] : wthr(read)
        //wpipe[1] : cthr(write)
        //rpipe[0] : cthr(read)
        //rpipe[1] : wthr(write)
        if (pipe(wpipe) < 0 || pipe(rpipe) <0 ){
            LERR("pipe create error!\n");
            return -1;
        }

        arg = malloc(sizeof(thr_arg_t));
        arg->rpipe = wpipe[0];
        arg->wpipe = rpipe[1];
        thrarr[i].rpipe = rpipe[0];
        thrarr[i].wpipe = wpipe[1];

        if (pthread_create(&thrarr[i].tid, NULL, thrfunc, arg) > 0){
            LERR("thread create error!\n");
            goto out;
        }
        LINFO("thread(tid : %X) is activated! \n", thrarr[i].tid);
        thrarr[i].isrun = false;
    }

    return 0;

    out:

        free(arg);
        return -1;
}

static void print_usage(char* p){
    printf("usage : %s [port]\n", p);
}

void sigint_handler(int signo){
    close(sfd);
    LINFO("\n[SIGINT] close connection.\n");
    stop_main_thread = true;
    //exit(1);
}



int main(int argc, char* argv[]){

    log_start("SERVER");
    set_log_level(SERVER_LOG_LEVEL);

    //file meta list
    filelist_init();

    //msg
    ipc_msg_t ipc_msg;

    if (create_thrs(WTHR_NUM, &wthrs[0], &wthr_main)<0){
        return -1;
    }

    if (create_thrs(1, &ethr, &ethr_main)<0){
        return -1;
    }

    queue_init();
    LINFO("Initialize msg queue!\n");
    set_destory_cb(&msg_free);

    LINFO("Server start!\n");

    if (argc != 2){
        print_usage(argv[0]);
        exit(1);
    }

    signal(SIGINT,sigint_handler);

    //open listen socket
    sfd = open_listen_socket(argv[1], MAX_CLIENT_WAIT);
    LINFO("open listen socket: fd(%d)", sfd);

    // setup fd array
    // server socket, wthr, ethr, client
    struct pollfd fds[1 + WTHR_NUM + 1 + MAX_CLIENT];
    int tot_fd = WTHR_NUM+MAX_CLIENT+2;

    int i=0;
    //register server listen socket fd to poll fd array
    fds[i].fd = sfd;
    fds[i].events = POLLIN;

    //register wthr listen pipe to poll fd array
    for (i=1;i<=WTHR_NUM;i++){
        fds[i].fd = wthrs[i-1].rpipe;
        fds[i].events = POLLIN;
    }
    //register ethr listen pipe to poll fd array
    fds[i].fd = ethr.rpipe;
    fds[i].events = POLLIN;

    int nread = 0;
    int maxidx = i;
    int clientidx = i+1;
    int cur_con = 0;

    socklen_t clilen = sizeof(struct sockaddr_in);

    for (i=clientidx;i<tot_fd;i++){
        fds[i].fd = -1;
    }

    //main loop
    LINFO("main listen loop started!\n");
    while(!stop_main_thread){
        nread = poll(fds, maxidx + 1, POLL_WAIT_TIME);

        if (nread==0) continue;

        i = 0;
        //server socket
        //LINFO("check client connection request...\n");
        if (fds[i].revents & POLLIN){

            int c, fdc = 0;
            for (c=0;c<MAX_CLIENT;c++){
                fdc = clientidx+c;
                if ( fds[fdc].fd <0 ){
                    fds[fdc].fd = accept(sfd,
                                      (struct sockaddr *)&connections[c].cliaddr,
                                      &clilen);
                    if (fds[fdc].fd < 0) continue;
                    connections[c].socket_fd = fds[fdc].fd;

                    LINFO("New connection!\n");
                    LINFO("Client IP address : %s, fd(%d)\n", inet_ntoa(connections[c].cliaddr.sin_addr), fds[fdc].fd);

                    fds[fdc].events = POLLIN;
                    cur_con++;

                    LINFO("Current Client : (%d / %d)", cur_con, MAX_CLIENT);
                    break;
                }
            };
            if (c==MAX_CLIENT){
                LWARN("Too Many Clients (%d / %d)", cur_con, MAX_CLIENT);
                continue;
            }
            if (fdc>maxidx) maxidx = fdc;
            if (--nread == 0) continue;
        }

        //LINFO("check worker thread msg...\n");
        //worker thread
        for (i=1;i<=WTHR_NUM;i++){
            if (fds[i].revents & (POLLIN | POLLERR)){
                read(fds[i].fd, &ipc_msg, sizeof(ipc_msg));
                if (ipc_msg.pmsg != NULL) {
                    send_msg_to_client(&ipc_msg);
                }

                if (get_new_job(&ipc_msg)<0){
                    //LINFO("thread(tid:%X) is going to sleep...\n", wthrs[i-1].tid);
                    wthrs[i-1].isrun = false;
                }
                else{
                    write(wthrs[i-1].wpipe, &ipc_msg, sizeof(ipc_msg));
                }

                if (--nread<=0) break;
            }
        }

        if (nread<=0) continue;

        //LINFO("check event thread msg...\n");
        //event thread
        if (fds[i].revents & (POLLIN | POLLERR)){
            read(fds[i].fd, &ipc_msg, sizeof(ipc_msg));
            send_msg_to_client(&ipc_msg);
            if (--nread<=0) continue;
        }

        //LINFO("check client msg...\n");
        //client socket
        for (i=clientidx;i<=maxidx;i++){

            if (fds[i].revents & (POLLIN | POLLERR)){

                ipc_msg.pconn = &connections[i-clientidx];
                ipc_msg.pmsg = recv_msg(fds[i].fd, &ipc_msg.buf);

                if (ipc_msg.pmsg == NULL){

                    cur_con--;
                    LINFO("Client is disconnected : fd(%d)", fds[i].fd);
                    LINFO("Current Client : (%d / %d)", cur_con, MAX_CLIENT);

                    fds[i].fd = -1;

                    continue;
                }
                assign_job(&ipc_msg);

                if (--nread<=0) break;
            }
        }


    }

    stop_worker_thread = true;

    int status;
    for (i=0;i<WTHR_NUM;i++){
        pthread_join(wthrs[i].tid, &status);
    }

    pthread_join(ethr.tid, &status);

    filelist_destory();
    queue_destroy();
    log_end();

    return 0;
}
