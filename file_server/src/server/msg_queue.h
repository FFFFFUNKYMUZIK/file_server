#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

void queue_init();
void set_destory_cb(void (*dealloc_func)(void* data));
int queue_size();

void enqueue(void* data);
void* dequeue();
void queue_destroy();

#endif //_MSG_QUEUE_H_
