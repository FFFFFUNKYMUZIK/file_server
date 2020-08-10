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

typedef struct filemeta_s{
    file_info_t fi;
} filemeta_t;

typedef struct filelist_s{
    filemeta_t filearr[MAX_FILE];
    int cnt;
    int fd;
    pthread_mutex_t lmtx;
} filelist_t;

extern filelist_t file_list;

void filelist_init();

int get_file_index_from_list(char* filename);

int add_file_to_list(file_info_t* pfi);

void filelist_destory();

#endif //_FILEMETA_H_
