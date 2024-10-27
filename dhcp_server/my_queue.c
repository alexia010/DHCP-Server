#include <stdlib.h>

#include "my_queue.h"

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(struct client_data* client_data_ptr)
{
    node_t* newnode = malloc(sizeof(node_t));
    newnode->data = client_data_ptr;
    newnode->next = NULL;

    if(tail == NULL)
        head = newnode;
    else
        tail->next = newnode;
    
    tail = newnode;
}

struct client_data* dequeue()
{
    if(head == NULL)
        return NULL;
    else
    {
        struct client_data* result = head->data;
        node_t* temp = head;
        head = head->next;

        if(head == NULL)  
            tail = NULL;
        
        free(temp);
        return result;
    }
}

void free_queue()
{
    while(head != NULL)
    {
        node_t* temp = head;
        head = head->next;
        free(temp->data);
        free(temp);
    }
    tail = NULL;
}