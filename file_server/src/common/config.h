#ifndef _CONFIG_H_
#define _CONFIG_H_

//buffer size config
#define FILENAME_LEN_MAX 300
#define OWNERNAME_LEN_MAX 100

//server configuration
#define MAX_FILE 1000
#define MAX_CLIENT_WAIT 5
#define SERVER_LOG_LEVEL 0
#define META_FILE ".filelist"

#define MAX_CLIENT 200
#define WTHR_NUM 20
#define POLL_WAIT_TIME 1000

#define MAX_AIO_EVENT 10000

//client configuration

#define AIO_STRIPE 131072 // (1024*128*1B) = 128KB
#define MAX_AIO_SUBMIT  16
#define MAX_WRITE_BUF_SIZE 100000
#define MAX_READ_BUF_SIZE 100000

#define CLIENT_LOG_LEVEL 0

#endif //_CONFIG_H_
