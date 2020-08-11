#ifndef _CLIENT_PROC_H_
#define _CLIENT_PROC_H_

//table appearance
#define FILENUMBER_WIDTH 10
#define FILENAME_WIDTH  20
#define FILESIZE_WIDTH 20
#define FILEOWNER_WIDTH 20


void read_proc(int socket_fd);
void write_proc(int socket_fd);
void put_proc(int socket_fd);
void get_proc(int socket_fd);
void list_proc(int socket_fd);

#endif //_CLIENT_PROC_H_
