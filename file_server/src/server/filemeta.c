#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libaio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../common/log.h"
#include "filemeta.h"

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

static file_list_t file_list;

void filelist_init(){
    int fd = open(META_FILE, O_CREAT | O_RDWR, 0644);
    if (fd<0){
        LFATAL("open meta file fail!\n");
        exit(1);
    }

    memset(file_list.filearr, 0, sizeof(file_list.filearr));

    int filesize;
    struct stat st;
    stat(META_FILE, &st);
    filesize = (int)st.st_size;
    int listsize = sizeof(file_list_t);

    if (filesize < listsize){
        file_list.cnt = 0;

        int ret;
        if ((ret = lseek(fd, listsize-1, SEEK_SET))<0){
            LERR("meta file resize error! (seek)\n");
            exit(1);
        }
        if ((ret = write(fd, "", 1))<0){
            LERR("meta file resize error! (write)\n");
            exit(1);
        }

        pthread_mutex_init(&file_list.lmtx, NULL);
    }
    else{
        read(fd, &file_list, sizeof(file_list_t));
        pthread_mutex_init(&file_list.lmtx, NULL);
    }
    file_list.fd = fd;

    LINFO("meta file opened.\n");
}

int get_file_index_from_list(char* filename){
    pthread_mutex_lock(&file_list.lmtx);
    int cnt = file_list.cnt;
    int i;
    for (i=0;i<cnt;i++){
        if (strcmp(file_list.filearr[i].fi.filename, filename)==0){
            break;
        }
    }

    pthread_mutex_unlock(&file_list.lmtx);
    if (i==cnt) return -1;
    return i;
}

int add_file_to_list(file_info_t* pfi){
    pthread_mutex_lock(&file_list.lmtx);
    int cnt = file_list.cnt;
    if (cnt==MAX_FILE - 1){
        LERR("Too many file is added");
        return -1;
    }

    file_list.filearr[cnt].fi = *pfi;

    pwrite(file_list.fd, &file_list.filearr[cnt], sizeof(file_meta_t), sizeof(file_meta_t)*cnt);
    cnt++;
    pwrite(file_list.fd, &cnt, sizeof(int), offsetof(file_list_t, cnt));
    file_list.cnt = cnt;

    LINFO("new file added : %s\n", pfi->filename);

    pthread_mutex_unlock(&file_list.lmtx);
    return cnt-1;
}

const file_meta_t* get_filemeta_by_idx(int fidx){
    if (fidx>=file_list.cnt) return NULL;

    return &file_list.filearr[fidx];
}

int get_file_cnt(void){
    int cnt;
    pthread_mutex_lock(&file_list.lmtx);
    cnt = file_list.cnt;
    pthread_mutex_unlock(&file_list.lmtx);
    return cnt;
}

void filelist_destory(){
    pthread_mutex_destroy(&file_list.lmtx);
    close(file_list.fd);
}