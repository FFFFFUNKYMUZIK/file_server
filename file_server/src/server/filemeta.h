#ifndef _FILEMETA_H_
#define _FILEMETA_H_

#include <pthread.h>
#include "../common/config.h"
#include "../common/fileinfo.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libaio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "../common/log.h"
#include "filemeta.h"

typedef struct wait_job_s{
    struct wait_job_s * next;
    bool read;
    int cnt;
    pthread_cond_t cond;
} wait_job_t;

typedef struct file_meta_s{
    file_info_t fi;
    pthread_mutex_t lock;
    int rcnt;
    int wcnt;
    wait_job_t* head;
    wait_job_t* tail;
    int wait_job_cnt;
    time_t timestamp;
} file_meta_t;

typedef struct file_list_s{
    file_meta_t filearr[MAX_FILE];
    int cnt;
    int fd;
    pthread_mutex_t lmtx;
} file_list_t;

void filelist_init();
int get_file_index_from_list(file_info_t* pfi, bool add_if_nonexist);
const file_meta_t* get_filemeta_by_idx(int fidx);

bool wait_lock(int fidx, int lock_cnt, bool read);
void decrease_lock_cnt(int fidx, bool read);
bool lock_timeout(int fidx);
void release_lock(int fidx);

int get_file_cnt(void);
void filelist_destory();

#endif //_FILEMETA_H_
