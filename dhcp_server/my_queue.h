#include "struct_client.h"

struct node
{
    struct node* next;
    struct client_data *data;
};
typedef struct node node_t;

void enqueue(struct client_data* client_socket);
struct client_data* dequeue();
void free_queue();