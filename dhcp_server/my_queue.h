#include "struct_client.h"

struct node_client
{
    struct node_client* next;
    struct client_data *data;
};
typedef struct node_client node_t;

void enqueue(struct client_data* client_socket);
struct client_data* dequeue();
void free_queue();