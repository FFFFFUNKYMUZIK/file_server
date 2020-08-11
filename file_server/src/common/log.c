#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>
#include "log.h"

static pthread_mutex_t fmtx;

static int lfd = -1;
static int llevel = 0;

const char* level_strings[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

void set_log_level(int level){
   llevel = level;
}

void log_start(char* log_prefix){

    if (mkdir("log", 0755)){
        if (errno != EEXIST){
            printf("Cannot create log dir : err(%d)\n", errno);
            return;
        }
    }

    char filename[1024];
    time_t t;
    time(&t);
    struct tm * ptm = localtime(&t);

    sprintf(filename, "log/%s_%02d%02d%02d_%02d%02d%02d.log",
            log_prefix, ptm->tm_year+1900, ptm->tm_mon+1,
            ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    if ((lfd = open(filename, O_CREAT|O_RDWR, 0755))==-1){
        printf("Cannot open log file : %s \n", filename);
    }

    pthread_mutex_init(&fmtx, NULL);

}

void log_end(){
    if (lfd!=-1){
        close(lfd);
    }
}

void log_(int level, const char* file, int line, const char *fmt, ...){
    if (level<llevel) return;

    char body[1024], buf[2048];
    va_list parg;
    va_start(parg, fmt);
    vsprintf(body, fmt, parg);
    va_end(parg);

    sprintf(buf, "[%s] %s : line %d | %s\n", level_strings[level], file, line, body);

    //print all information including file, line.
    //printf("%s", buf);
    printf("[%s] %s", level_strings[level], body);

    if (lfd!=-1){
        pthread_mutex_lock(&fmtx);
        write(lfd, buf, (size_t)strlen(buf));
        pthread_mutex_unlock(&fmtx);
    }
}

