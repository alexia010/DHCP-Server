#include "queue.h"

node *create_node(void *value)
{
    node *new_node=(node*)malloc(sizeof(node));

    if(!new_node)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    new_node->data=value;
    new_node->next=NULL;
    return new_node;
}

int queue_is_empty(queue *q)
{
    if(q->head==NULL)
    {  
        return 1;
    }

    return 0;
}

void queue_init(queue *q)
{
    q->head=NULL;
    q->tail=NULL;
}

void queue_enqueue(queue*q,void *value)
{
    node* new_node=create_node(value);

    if(!new_node)
    {
        return;
    }

    if(q->tail==NULL)
    {
        q->head=new_node;
        q->tail=new_node;
    }
    else
    {
        q->tail->next=new_node;
        q->tail=new_node;
    }
}

void *queue_dequeue(queue*q)
{
    if(q->head==NULL)
    {
        printf("empty queue\n");
        return NULL;
    }

    node *temp=q->head;
    void*data=temp->data;
    q->head=q->head->next;

    if(q->head==NULL)
    {
        q->tail=NULL;
    }

    free(temp);
    return data;
}