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
#include "dhcp_packet.h"
#include "logger.h"
#include "dhcp_server.h"

#define DHCP_SERVER_PORT 2000
#define DHCP_CLIENT_PORT 2001
#define THREAD_POOL_SIZE 3

//creez un pool_thread
pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

void *handle_connection(void* pclient);
void check(int result, const char* message);
void* thread_function(void* argument);

server_configs *server;

int main(int argc, char*argv[])
{
    printf("Start\n");
    log_init("log.txt");

    int socketfd;

    struct sockaddr_in client_addr, server_addr;
    socklen_t addr_len = sizeof(client_addr);

    char buffer_received[MAX_DHCP_SIZE];

    //aloc server ul cu doate datele necesare
    server = malloc(sizeof(server_configs));
    initialize_server(server, SERVER_IP, NETMASK, GATEWAY, "intef.org", LEASE_TIME, PENDING_TIME, RENEWAL_TIME, REBINDING_TIME);
    server->networks_nr = initialize_networks(server->networks);

    //creez pool_thread
    for(int i = 0; i < THREAD_POOL_SIZE; i++)
        check(pthread_create(&thread_pool[i], NULL, thread_function, NULL), "Error at creating thread!");

    //creez socket udp pt server dhcp
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketfd == -1)
    {
        log_msg(ERROR, "main", "Error at socket creation!");
        exit(-1);
    }
     
    //configurarea optiunii de broadcast pentru un socket
    int broadcastEnable = 0;
    check(setsockopt(socketfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)), "Error at setting broadcast option!");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if((bind(socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1))
    {
        log_msg(ERROR, "main", "Error at binding!");
        exit(-1);
    }


    //in aceasta variabila voi avea pachetul sub forma de structura
    //dhcp_packet *packet_dhcp_received=malloc(sizeof(dhcp_packet));
    dhcp_packet packet_dhcp_received;
    while(1)
    {
        printf("Astept cerere dhcp...\n");
        log_msg(INFO, "main", "Waiting for DHCP request!");
        int rb = recvfrom(socketfd, buffer_received, MAX_DHCP_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        //int rb = recvfrom(socketfd, (pa*)&packet_dhcp_received, sizeof(dhcp_packet), 0, (struct sockaddr*)&client_addr, &addr_len);
        if(rb < 0)
        {
            log_msg(ERROR, "main", "Error at receiving packet from client!");
            exit(-1);
        }

        deserialize(buffer_received, rb, &packet_dhcp_received);

        printf("Cerere dhcp primita!\n");
       
        log_msg(INFO, "main", "DHCP request received!");

        //aloc memorie pentru structura mea client_data
        struct client_data *pclient = malloc(sizeof(struct client_data));
        if(pclient == NULL)
        {
            log_msg(ERROR, "main", "Error at memory allocation!");
            continue;
        }

        pclient->socketfd = socketfd;
        memcpy(&pclient->client_addr, &client_addr, sizeof(client_addr));
        memcpy(&pclient->packet, &packet_dhcp_received, sizeof(dhcp_packet));

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

    network *net = &server->networks[rand() % server->networks_nr];


    //procesam mesajele dhcp pe baza tipului pana cand coada este goala
  
    dhcp_option *options;
    dhcp_packet *packet_to_send = malloc(sizeof(dhcp_packet));
    if (!packet_to_send) 
    {
        log_msg(ERROR, "main", "Memory allocation failed!");
        return NULL;
    }
    queue_init(&packet_to_send->dhcp_options);
    while(options = queue_dequeue(&packet.dhcp_options))
    {
        if(options->id == DHCP_MESSAGE_TYPE)
        {
            char data[2];
            if(options->data[0] == DHCP_DISCOVER)   //aici am rezolvat dhcpdiscover si dhcpoffer
            {
                packet_to_send = process_dhcp_discover(server, packet, &net);
            }
            else if(options->data[0] == DHCP_REQUEST)
            {
                if(packet.header.ciaddr != 0) //dhcp renew
                {
                    if(renew_ip_address(server, packet, &net, options))
                    {
                        data[0] = DHCP_ACK;
                        packet.header.yiaddr = packet.header.ciaddr;
                    }
                    else
                        data[0] = DHCP_NAK;
                }
                else    //dynamic ip
                {
                    uint32_t ip_address = ((uint32_t)options->data[1] << 24) |
                      ((uint32_t)options->data[2] << 16) |
                      ((uint32_t)options->data[3] << 8)  |
                      (uint32_t)options->data[4];


                    packet.header.ciaddr = ip_address;
                    if(confirm_ip_address(server, packet, &net))
                    {
                        data[0] = DHCP_ACK;
                        packet.header.yiaddr = packet.header.ciaddr;
                    }
                    else
                        data[0] = DHCP_NAK;
                }
                packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
            }
            else if(options->data[0] == DHCP_DECLINE) //dhcp decline
            {
                if(decline_ip(server, packet, &net))
                {
                    data[0] = DHCP_ACK;
                    packet.header.ciaddr = 0;
                    packet.header.yiaddr = 0;
                }
                else 
                    data[0] = DHCP_NAK;
                packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
            }
            else if(options->data[0] == DHCP_RELEASE)
            {
                if(release_ip_address(server, packet, &net))
                   log_msg(INFO, "main", "Releasing IP address succesfully!");
                else
                    log_msg(ERROR, "main", "Error at releasing IP address!");
            }
            else if(options->data[0] == DHCP_INFORM)
            {
                set_header(&packet_to_send->header, BOOTREQUEST, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, packet.header.siaddr, packet.header.giaddr, packet.header.chaddr);
                
                dhcp_option *option;
                while ((option = queue_dequeue(&packet.dhcp_options)) != NULL) 
                {
                    switch (option->id) 
                    {
                        case DHCP_MESSAGE_TYPE:
                            if (option->data[0] == DHCP_INFORM) {
                                char ack_data[1] = {DHCP_ACK};
                                add_option_reply(&packet_to_send->dhcp_options, DHCP_MESSAGE_TYPE, 1, ack_data);
                            }
                            break;

                        case SUBNET_MASK:
                            {
                                char netmask_data[4];
                                convert_ip(net->netmask, netmask_data);
                               
                                add_option_reply(&packet_to_send->dhcp_options, SUBNET_MASK, 4, netmask_data);
                            }
                            break;

                        case DOMAIN_NAME_SERVER:
                            {
                                char dns_data[net->dns_count * 4];
                                for (int i = 0; i < net->dns_count; i++) //aici trebuie avut in vedere ca sa am fix maxdns in structura 
                                {
                                    convert_ip(net->dns_server, dns_data[i * 4]); //sa ma asigur 
                                }
                                add_option_reply(&packet_to_send->dhcp_options, DOMAIN_NAME_SERVER, net->dns_count * 4, dns_data);
                            }
                            break;

                        default:
                            log_msg(INFO, "main", "Received unknown option!");
                            break;
                    }
                }
            }
        }
        else  if(options->id == REQUESTED_IP_ADDRESS)
        {
            uint32_t requested_ip;
            requested_ip = (options->data[1] << 24) | (options->data[2] << 16) | (options->data[3] << 8) | options->data[4];
            char data[2];

            if(confirm_static_ip(server, packet, &net, requested_ip))
            {
                data[0] = DHCP_ACK;
                packet.header.yiaddr = requested_ip;
            }
            else
                data[0] = DHCP_NAK;

            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
        }       
    }
   
    //aici fac send;
    char buffer[MAX_DHCP_SIZE];
    int len = serialize_packet(packet_to_send, buffer);
    check(sendto(socketfd, &buffer, len, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)), "Error at sending DHCPOFFER!");

    return NULL;
}


void check(int result, const char* message)
{
    if(result < 0)
    {
        log_msg(ERROR, "main", message);
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
