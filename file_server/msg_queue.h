//
// Created by cjh on 20. 8. 9..
//

#ifndef FILE_SERVER_MSG_QUEUE_H
#define FILE_SERVER_MSG_QUEUE_H

void queue_init();
int queue_size();

void enqueue(void* data);
void* dequeue();
void queue_destroy();

#endif //FILE_SERVER_MSG_QUEUE_H
