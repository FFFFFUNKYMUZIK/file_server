#ifndef _CLIENT_PROC_H_
#define _CLIENT_PROC_H_

void read_proc(int socket_fd);
void write_proc(int socket_fd);
void put_proc(int socket_fd);
void get_proc(int socket_fd);
void list_proc(int socket_fd);

#endif //_CLIENT_PROC_H_
