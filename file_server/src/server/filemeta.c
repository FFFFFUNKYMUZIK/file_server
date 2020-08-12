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

static int add_file_to_list(file_info_t* pfi){
    int cnt = file_list.cnt;
    if (cnt==MAX_FILE - 1){
        LERR("Too many file is added");
        return -1;
    }

    file_meta_t* pfm = &file_list.filearr[cnt];
    pthread_mutex_init(&pfm->lock, NULL);
    pfm->rcnt = 0;
    pfm->wcnt = 0;
    pfm->head = NULL;
    pfm->tail = NULL;
    pfm->wait_job_cnt = 0;

    file_list.filearr[cnt].fi = *pfi;

    pwrite(file_list.fd, &file_list.filearr[cnt], sizeof(file_meta_t), sizeof(file_meta_t)*cnt);
    cnt++;
    pwrite(file_list.fd, &cnt, sizeof(int), offsetof(file_list_t, cnt));
    file_list.cnt = cnt;


    LINFO("new file added : %s\n", pfi->filename);

    return cnt-1;
}

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

    int i;
    for (i=0;i<file_list.cnt;i++){
        file_meta_t * pfm = &file_list.filearr[i];
        pthread_mutex_init(&pfm->lock, NULL);
        pfm->rcnt = 0;
        pfm->wcnt = 0;
        pfm->head = NULL;
        pfm->tail = NULL;
        pfm->wait_job_cnt = 0;
        pfm->timestamp = time(NULL);
    }

    LINFO("meta file opened.\n");
}

int get_file_index_from_list(file_info_t* pfi, bool add_if_nonexist){
    pthread_mutex_lock(&file_list.lmtx);
    int cnt = file_list.cnt;
    int i;
    for (i=0;i<cnt;i++){
        if (strcmp(file_list.filearr[i].fi.filename, pfi->filename)==0){
            break;
        }
    }

    if (i==cnt){
        if(add_if_nonexist) {
            i = add_file_to_list(pfi);
        }
        else{
            i=-1;
        }
    }
    pthread_mutex_unlock(&file_list.lmtx);
    return i;
}

const file_meta_t* get_filemeta_by_idx(int fidx){
    if (fidx>=file_list.cnt) return NULL;

    return &file_list.filearr[fidx];
}

bool wait_lock(int fidx, int lock_cnt, bool read){
    pthread_mutex_lock(&file_list.lmtx);
    if (fidx>=file_list.cnt){
        pthread_mutex_unlock(&file_list.lmtx);
        return false;
    }
    pthread_mutex_unlock(&file_list.lmtx);

    file_meta_t* pfm = &file_list.filearr[fidx];

    pthread_mutex_lock(&pfm->lock);

    if (read){
        //if the file is being modified or there is at least one wating job
        if (pfm->wcnt>0 || pfm->head != NULL){
            wait_job_t *next =malloc(sizeof(wait_job_t));
            next->next = NULL;
            pthread_cond_init(&next->cond, NULL);
            next->read = read;
            next->cnt = lock_cnt;

            if (pfm->head == NULL){
                pfm->head = next;
                pfm->tail = next;
            }
            else {
                pfm->tail->next = next;
                pfm->tail = next;
            }

            pfm->wait_job_cnt++;

            //wait until receiving signal by previous job
            pthread_cond_wait(&next->cond, &pfm->lock);
            pthread_cond_destroy(&next->cond);
            free(next);
        }
            pfm->rcnt += lock_cnt;
    }
    else{
        //if the file is being modified or read, or there is at least one wating job
        if (pfm->wcnt>0 || pfm->rcnt>0 || pfm->head != NULL){
            wait_job_t *next =malloc(sizeof(wait_job_t));
            next->next = NULL;
            pthread_cond_init(&next->cond, NULL);
            next->read = read;
            next->cnt = lock_cnt;

            if (pfm->head == NULL){
                pfm->head = next;
                pfm->tail = next;
            }
            else {
                pfm->tail->next = next;
                pfm->tail = next;
            }

            pfm->wait_job_cnt++;

            //wait until receiving signal by previous job
            pthread_cond_wait(&next->cond, &pfm->lock);
            pthread_cond_destroy(&next->cond);
            free(next);
        }
            pfm->wcnt = lock_cnt;
    }

    LINFO("[%d] rwcnt : %d %d \n", fidx, pfm->rcnt, pfm->wcnt);

    pthread_mutex_unlock(&pfm->lock);
    return true;
}

void decrease_lock_cnt(int fidx, bool read){
    pthread_mutex_lock(&file_list.lmtx);
    if (fidx>=file_list.cnt){
        pthread_mutex_unlock(&file_list.lmtx);
        return;
    }
    pthread_mutex_unlock(&file_list.lmtx);

    file_meta_t* pfm = &file_list.filearr[fidx];

    pthread_mutex_lock(&pfm->lock);

    bool signal = (read && (--(pfm->rcnt)==0)) || (!read && (--(pfm->wcnt)==0));

    LINFO("[%d] rwcnt : %d %d \n", fidx, pfm->rcnt, pfm->wcnt);

    if (signal && pfm->head != NULL){
        bool readable = true;
        do {
            wait_job_t *next = pfm->head->next;
            readable = pfm->head->read;
            pfm->wait_job_cnt--;

            pthread_cond_signal(&pfm->head->cond);
            pfm->head = next;
            if (pfm->head == NULL){
                pfm->tail = NULL;
            }
        } while(pfm->head && readable && pfm->head->read);
    }

    pthread_mutex_unlock(&pfm->lock);
}

bool lock_timeout(int fidx){
    /*
    pthread_mutex_lock(&file_list.lmtx);
    if (fidx>=file_list.cnt){
        return false;
    }
    pthread_mutex_unlock(&file_list.lmtx);

    file_meta_t* pfm = &file_list.filearr[fidx];

    pthread_mutex_lock(&pfm->lock);

    time_t cur_timestamp = time(NULL);
            pfm->timestamp;

    pthread_mutex_unlock(&pfm->lock);
     */
    return false;
}

void release_lock(int fidx){
    /*
    pthread_mutex_lock(&file_list.lmtx);
    if (fidx>=file_list.cnt){
        return;
    }
    pthread_mutex_unlock(&file_list.lmtx);

    file_meta_t* pfm = &file_list.filearr[fidx];

    pthread_mutex_lock(&pfm->lock);


    pthread_mutex_unlock(&pfm->lock);
     */
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