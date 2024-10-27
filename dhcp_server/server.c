#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>

#include "my_queue.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define THREAD_POOL_SIZE 20

//creez un pool_thread
pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

void *handle_connection(void* pclient);
void check(int result, const char* message);
void* thread_function(void* argument);


int main(int argc, char*argv[])
{
    int socketfd;

    struct sockaddr_in client_addr, server_addr;
    struct dhcp_packet packet;
    socklen_t addr_len = sizeof(client_addr);

    //creez pool_thread
    for(int i = 0; i < THREAD_POOL_SIZE; i++)
        check(pthread_create(&thread_pool[i], NULL, thread_function, NULL), "Eroare la creare thread!");

    //creez socket udp pt server dhcp
    check((socketfd = socket(AF_INET, SOCK_DGRAM, 0)), "Eroare la creearea socket-ului");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    check(bind(socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)), "Eroare la bind!");

    while(1)
    {
        printf("Astept cerere dhcp...\n");

        check(recvfrom(socketfd, (char*)&packet, sizeof(struct dhcp_packet), MSG_WAITALL, (struct sockaddr*)&client_addr, &addr_len), "Eroare la receive de la client!");
        printf("Cerere dhcp primita!\n");

        //aloc memorie pentru structura mea client_data
        struct client_data *pclient = malloc(sizeof(struct client_data));
        if(pclient == NULL)
        {
            perror("Eroare la alocarea memoriei!\n");
            continue;
        }

        pclient->socketfd = socketfd;
        memcpy(&pclient->client_addr, &client_addr, sizeof(client_addr));
        memcpy(&pclient->packet, &packet, sizeof(packet));

        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
    }

    free_queue();
    close(socketfd);

    for(int i = 0; i < THREAD_POOL_SIZE; i++)
        pthread_join(thread_pool[i], NULL);    //astept ca toate thread-urile sa se incheie
    
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition_var);

    return 0;
}


void *handle_connection(void* pclient)
{
    struct client_data *data = (struct client_data*)pclient;

    int socketfd = data->socketfd;
    struct dhcp_packet packet = data->packet;
    struct sockaddr_in client_addr = data->client_addr;

    free(pclient);  //nu mai avem nevoie de pclient

    printf("Am primit un pachet DHCP cu xid = %d, de la client!\n", ntohl(packet.xid));

    //verific daca pachetul este de tip DHCPDISCOVER
    if(packet.options[0] == DHCP_OPTION_MSG_TYPE && packet.options[2] == DHCPDISCOVER)
    {
        packet.op = REPLY;
        packet.yiaddr = inet_addr("192.168.1.100");
        packet.siaddr = inet_addr("192.168.1.1");
        packet.giaddr = 0 ; //asta i gateway mereu uit

        //trimit pachetul DHCPDISCOVER catre client
        check(sendto(socketfd, (char*)&packet, sizeof(struct dhcp_packet), MSG_CONFIRM, (const struct sockaddr*)&client_addr, sizeof(client_addr)), "Eroare la trimitere DHCPDISCOVER");
        printf("Am trimis DHCPDISCOVER cu yiaddr: %s\n", inet_ntoa(*(struct in_addr*)&packet.yiaddr));
    }

    return NULL;
}


void check(int result, const char* message)
{
    if(result < 0)
    {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

void* thread_function(void* argument)
{
    while(1)
    {
        pthread_mutex_lock(&mutex);
        struct client_data* pclient = dequeue();

        if(pclient == NULL)
        {
            pthread_cond_wait(&condition_var, &mutex);
            pclient = dequeue();
        }
        
        pthread_mutex_unlock(&mutex);

        if(pclient != NULL)
            handle_connection(pclient);
    }
}