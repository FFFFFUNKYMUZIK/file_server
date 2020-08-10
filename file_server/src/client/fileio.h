#ifndef _FILEIO_H_
#define _FILEIO_H_

int open_rfile(char* filename, int* filesize, char* fileowner);
int open_wfile(char* filename);
int close_file(int fd);

int read_file(int fd, unsigned char* buf, int size, int offset);
int write_file(int fd, unsigned char* buf, int size, int offset);

#endif //_FILEIO_H_
