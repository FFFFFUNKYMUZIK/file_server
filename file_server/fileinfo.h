//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_FILEINFO_H
#define FILE_SERVER_FILEINFO_H
#include "config.h"

typedef struct fileinfo_s{
    int filesize;
    char filename[MAX_FILE_NAME];
    char ownername[MAX_OWNER_NAME];
} fileinfo_t;


#endif //FILE_SERVER_FILEINFO_H
