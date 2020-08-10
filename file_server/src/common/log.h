#ifndef _LOG_H_
#define _LOG_H_

//refer to https://github.com/rxi/log.c.git

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_MAX
    };

#define LTRACE(...) log_(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LDEBUG(...) log_(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LINFO(...) log_(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LWARN(...) log_(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LERR(...) log_(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LFATAL(...) log_(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void log_start();
void log_end();
void set_log_level(int level);

void log_(int level, const char* file, int line, const char *fmt, ...);

#endif //_LOG_H_
