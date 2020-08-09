//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_FILEMETA_H
#define FILE_SERVER_FILEMETA_H

#include "config.h"
#include "fileinfo.h"

typedef struct filemeta_s{
    fileinfo_t fi;

} filemeta_t;

extern filemeta_t* filelist[MAX_FILE];

#endif //FILE_SERVER_FILEMETA_H
