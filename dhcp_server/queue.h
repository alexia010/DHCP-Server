#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdio.h>

struct node 
{
    void *data;
    struct node *next;
};

typedef struct node node;

node * create_node(void*value);

typedef struct 
{
    node *head;
    node *tail;
}queue;

int queue_is_empty(queue*q);
void queue_init(queue*q);
void queue_enqueue(queue*q,void*value);
void* queue_dequeue(queue*q);



#endif