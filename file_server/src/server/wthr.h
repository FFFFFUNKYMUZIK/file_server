#ifndef _WTHR_H_
#define _WTHR_H_

#include <stdbool.h>
#include "../common/config.h"

//worker thread

typedef struct thr_s{
    pthread_t tid;
    int rpipe, wpipe; // used for sending msg to wthr from outside
    bool isrun;
} thr_t;

typedef struct thr_arg_s{
    int rpipe, wpipe; // used inside of each wthr
    int self_thr_idx;
} thr_arg_t;

extern thr_t wthrs[WTHR_NUM];
extern thr_t ethr;
extern thr_t mthr;

extern bool stop_main_thread;
extern bool stop_worker_thread;

void* wthr_main(void* arg);
void* ethr_main(void *arg);
void* mthr_main(void *arg);

#endif //_WTHR_H_
