#include <stddef.h>
#include <stdlib.h>
#include "msg_queue.h"

typedef struct node_s {
    struct node_s* next;
    void* data;
} node_t;

typedef struct queue_s{
    node_t* head;
    node_t* tail;
    int size;
} queue_t;

static queue_t q;
void (*deallocator)(void* data);

void queue_init(){
    q.head = NULL;
    q.tail = NULL;
    q.size = 0;

}

int queue_size(){
    return q.size;
}

void enqueue(void* pdata){

    node_t * pnode = malloc(sizeof(node_t));
    pnode->next = NULL;
    pnode->data = pdata;

    if (q.size == 0){
        q.head = pnode;
        q.tail = pnode;
        q.size = 1;
    }
    else{
        q.tail->next = pnode;
        q.tail = pnode;
        q.size++;
    }
}

void* dequeue(){
    void* ret = NULL;
    if (q.size == 0){
        return ret;
    }
    else{
        ret = q.head->data;
        node_t* nhead = q.head->next;
        free(q.head);
        q.head = nhead;
        q.size--;
        if (q.head == NULL) q.tail = NULL;
        return ret;
    }
}

void set_destory_cb(void (*dealloc_func)(void* data)){
    deallocator = dealloc_func;
}

void queue_destroy(){

    node_t* cur = q.head;
    node_t* next;

    while(cur != NULL){
        next = cur->next;
        deallocator(cur->data);
        free(cur->data);
        free(cur);
        cur = next;
    }
}
