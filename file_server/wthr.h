//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_WTHR_H
#define FILE_SERVER_WTHR_H

#include <stdbool.h>
#include "config.h"

//worker thread

typedef struct thr_s{
    pthread_t tid;
    int rpipe, wpipe; // used for sending msg to wthr from outside
    bool isrun;
} thr_t;

typedef struct thr_arg_s{
    int rpipe, wpipe; // used inside of each wthr
} thr_arg_t;

extern thr_t wthrs[WTHR_NUM];
extern thr_t ethr;

extern bool stop_worker_thread;

void* wthr_main(void* arg);
void* ethr_main(void *arg);

#endif //FILE_SERVER_WTHR_H
