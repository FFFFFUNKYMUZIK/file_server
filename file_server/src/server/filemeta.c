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

filelist_t file_list;

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

    if (filesize < sizeof(filelist_t)){
        file_list.cnt = 0;
        pthread_mutex_init(&file_list.lmtx, NULL);
    }
    else{
        read(fd, &file_list, sizeof(filelist_t));
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

    //pwrite(file_list.fd, &file_list.filearr[cnt], sizeof(filemeta_t), sizeof(filemeta_t)*cnt);
    cnt++;
    //pwrite(file_list.fd, &cnt, sizeof(int), offsetof(filelist_t, cnt));
    file_list.cnt = cnt;

    LINFO("new file added : %s\n", pfi->filename);

    pthread_mutex_unlock(&file_list.lmtx);
    return cnt-1;
}

void filelist_destory(){
    pthread_mutex_destroy(&file_list.lmtx);
    close(file_list.fd);
}