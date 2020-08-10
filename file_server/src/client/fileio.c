//#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libaio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <assert.h>

#include "fileio.h"
#include "../common/config.h"

int open_rfile(char* filename, int* filesize, char* fileowner){

    struct stat st;
    struct passwd * pwd;

//    int fd = open(filename, O_RDONLY | O_DIRECT);
    int fd = open(filename, O_RDONLY);
    if (fd<0){
        perror("open file fail\n");
        return -1;
    }

    stat(filename, &st);
    *filesize = (int)st.st_size;

//not using uid of file(it might be root)
    /*
    pwd = getpwuid(st.st_uid);
    strcpy(fileowner, pwd->pw_name);
    */
//use uid which launch this process
    pwd = getpwuid(getuid());
    strcpy(fileowner, pwd->pw_name);

    return fd;
}

int open_wfile(char* filename){

//    int fd = open(filename, O_WRONLY | O_CREAT | O_DIRECT, 0664);
    int fd = open(filename, O_WRONLY | O_CREAT, 0664);
    if (fd<0){
        perror("open file fail\n");
        return -1;
    }
    return fd;
}

int read_file(int fd, unsigned char* buf, int size, int offset){

    io_context_t ctx;
    struct iocb* piocbarr[MAX_AIO_SUBMIT];

    int num_iocb = (size + AIO_STRIPE - 1)/AIO_STRIPE;

    //temporary abortion
    assert(num_iocb<MAX_AIO_SUBMIT);
    assert(num_iocb>0);

    struct iocb* piocbsz;
    piocbsz = malloc(sizeof(struct iocb)* num_iocb);

    struct io_event* pe = malloc(sizeof (struct io_event) * num_iocb);
    memset(pe, 0, sizeof(struct io_event)*num_iocb);
    memset(&ctx, 0x00, sizeof(ctx));

    if(io_setup(MAX_AIO_SUBMIT, &ctx) != 0 ) {
        printf("io_setup error\n");
        goto exit;
    }

    int remain = size;
    int i;

    for (i=0;i<num_iocb;i++){

        io_prep_pread(piocbsz+i, fd, (void *)buf, (size_t)AIO_STRIPE<remain ? AIO_STRIPE : remain , (long long)offset);
        piocbarr[i] = piocbsz+i;
        //printf("%p %p %d %d \n", buf, piocbarr[i], offset, remain);

        buf+=AIO_STRIPE;
        offset+=AIO_STRIPE;
        remain -= AIO_STRIPE;
    }

    if (io_submit(ctx, num_iocb, piocbarr)!=num_iocb){
        perror("io submit error\n");
        goto exit;
    }

     struct timespec timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 1;

    int cnt = 0;

    while(1){
        if (io_getevents(ctx, num_iocb, num_iocb, pe, &timeout) == num_iocb){
            free(pe);
            free(piocbsz);
            io_destroy(ctx);
            return 0;
        }

        if (++cnt==3){
            perror("aio timeout!");
            goto exit;
        }
    }

exit:

    free(pe);
    free(piocbsz);
    io_destroy(ctx);
    return -1;
}

int write_file(int fd, unsigned char* buf, int size, int offset){

    io_context_t ctx;
    struct iocb* piocbarr[MAX_AIO_SUBMIT];

    int num_iocb = (size + AIO_STRIPE - 1)/AIO_STRIPE;

    //temporary abortion
    assert(num_iocb<MAX_AIO_SUBMIT);
    assert(num_iocb>0);

    struct iocb* piocbsz;
    piocbsz = malloc(sizeof(struct iocb)* num_iocb);

    struct io_event* pe = malloc(sizeof (struct io_event) * num_iocb);
    memset(pe, 0, sizeof(struct io_event)*num_iocb);
    memset(&ctx, 0x00, sizeof(ctx));

    if(io_setup(MAX_AIO_SUBMIT, &ctx) != 0 ) {
        printf("io_setup error\n");
        goto exit;
    }

    int remain = size;
    int i;

    for (i=0;i<num_iocb;i++){

        io_prep_pwrite(piocbsz+i, fd, (void *)buf, (size_t)AIO_STRIPE<remain ? AIO_STRIPE : remain , (long long)offset);
        piocbarr[i] = piocbsz+i;
        //printf("%p %p %d %d \n", buf, piocbarr[i], offset, remain);

        buf+=AIO_STRIPE;
        offset+=AIO_STRIPE;
        remain -= AIO_STRIPE;
    }

    if (io_submit(ctx, num_iocb, piocbarr)!=num_iocb){
        perror("io submit error\n");
        goto exit;
    }

     struct timespec timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 1;

    int cnt = 0;

    while(1){
        if (io_getevents(ctx, num_iocb, num_iocb, pe, &timeout) == num_iocb){
            free(pe);
            free(piocbsz);
            io_destroy(ctx);
            return 0;
        }

        if (++cnt==3){
            perror("aio timeout!");
            goto exit;
        }
    }

exit:

    free(pe);
    free(piocbsz);
    io_destroy(ctx);
    return -1;
}

int close_file(int fd){
    return close(fd);
}
