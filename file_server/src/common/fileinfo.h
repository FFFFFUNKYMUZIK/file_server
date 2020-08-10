#ifndef _FILEINFO_H_
#define _FILEINFO_H_

#include "config.h"

typedef struct file_info_s{
    unsigned int filesize;
    char filename[FILENAME_LEN_MAX];
    char fileowner[OWNERNAME_LEN_MAX];
} file_info_t;

#endif //_FILEINFO_H_
