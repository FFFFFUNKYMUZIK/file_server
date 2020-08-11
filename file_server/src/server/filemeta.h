#ifndef _FILEMETA_H_
#define _FILEMETA_H_

#include <pthread.h>
#include "../common/config.h"
#include "../common/fileinfo.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libaio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../common/log.h"
#include "filemeta.h"

typedef struct file_meta_s{
    file_info_t fi;
} file_meta_t;

typedef struct file_list_s{
    file_meta_t filearr[MAX_FILE];
    int cnt;
    int fd;
    pthread_mutex_t lmtx;
} file_list_t;

void filelist_init();

int get_file_index_from_list(char* filename);

int add_file_to_list(file_info_t* pfi);

const file_meta_t* get_filemeta_by_idx(int fidx);

int get_file_cnt(void);

void filelist_destory();

#endif //_FILEMETA_H_
