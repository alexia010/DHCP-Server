#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "my_queue.h"
#include "dhcp_packet.h"
#include "logger.h"
#include "dhcp_server.h"

volatile sig_atomic_t running = 1;

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
void handle_dhcp_discover(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send);
void handle_dhcp_request(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send, dhcp_option *options);
void handle_dhcp_request_static(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send, dhcp_option *options);
void handle_dhcp_decline(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send);
void handle_dhcp_release(dhcp_packet **packet_to_send, server_configs *server, dhcp_packet *packet, network **net);
void handle_dhcp_inform(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send);
void send_dhcp_packet(int socketfd, dhcp_packet **packet_to_send, struct sockaddr_in *client_addr);
void handle_sigint(int sig);

void get_memory_usage();
    
server_configs *server;

int main(int argc, char*argv[])
{
    signal(SIGINT, handle_sigint);
    log_init("log.txt");

    int socketfd;

    struct sockaddr_in client_addr, server_addr;
    socklen_t addr_len = sizeof(client_addr);

    char buffer_received[MAX_DHCP_SIZE];

    // Allocate server with necessary data
    server = malloc(sizeof(server_configs));
    initialize_server(server, SERVER_IP, NETMASK, GATEWAY, "intef.org", LEASE_TIME, PENDING_TIME, RENEWAL_TIME, REBINDING_TIME);
    server->networks_nr = initialize_networks(server->networks);

    // Creating pool thread
    for(int i = 0; i < THREAD_POOL_SIZE; i++)
        check(pthread_create(&thread_pool[i], NULL, thread_function, NULL), "Error at creating thread!");

    // Creating UDP socket for DHCP client
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketfd == -1)
    {
        log_msg(ERROR, "server/main", "Error at socket creation!");
        exit(-1);
    }
     
    // Configuration option broadcast for socket
    int broadcastEnable = 0;
    check(setsockopt(socketfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)), "Error at setting broadcast option!");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if((bind(socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1))
    {
        log_msg(ERROR, "server/main", "Error at binding!");
        exit(-1);
    }

    struct timeval tv;
    tv.tv_sec = 5; // 5 sec timeout
    tv.tv_usec = 0;
    setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    dhcp_packet packet_dhcp_received;
    printf("Waiting for DHCP requests...\n");
    while(running)
    {
        log_msg(INFO, "server/main", "Waiting for DHCP request!");
        int rb = recvfrom(socketfd, buffer_received, MAX_DHCP_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if(rb < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else{
                log_msg(ERROR, "server/main", "Error at receiving packet from client!");
                break;
            }
        }

        deserialize(buffer_received, rb, &packet_dhcp_received);
       
        log_msg(INFO, "server/main", "DHCP request received!");

        get_memory_usage();

        // Allocation for client_data
        struct client_data *pclient = malloc(sizeof(struct client_data));
        if(pclient == NULL)
        {
            log_msg(ERROR, "server/main", "Error at memory allocation!");
            continue;
        }

        pclient->socketfd = socketfd;
        memcpy(&pclient->client_addr, &client_addr, sizeof(client_addr));
        memcpy(&pclient->packet, &packet_dhcp_received, sizeof(dhcp_packet));

        // We start time measuring 
        struct timeval start, end;
        gettimeofday(&start, NULL);

        // No other thread modifies the queue while we're adding a client data
        pthread_mutex_lock(&mutex);

        // Adding data
        enqueue(pclient);

        // Signal that new data is in the queue
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);

        gettimeofday(&end, NULL); // End time measuring 

        double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        
        char log_msg_buffer[256]; 
        sprintf(log_msg_buffer, "Processig time for DHCP request: %.6f seconds", elapsed_time);

        // Log message
        log_msg(INFO, "server/main", log_msg_buffer);    }

    free_queue();

    for(int i = 0; i < THREAD_POOL_SIZE; i++)
        pthread_join(thread_pool[i], NULL);    // wait for all the threads to close
    
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition_var);

    printf("Server shutdown.\n");
    log_msg(INFO, "server/main", "Server shutdown complete.");
    close(socketfd);
    return 0;
}

void handle_dhcp_discover(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send) 
{
    *packet_to_send = process_dhcp_discover(server, *packet, net);
}

void handle_dhcp_request(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send, dhcp_option *options) 
{
    char data[10];

    if (packet->header.ciaddr != 0) { // DHCP Renew
        if (renew_ip_address(server, *packet, net, options)) {
            data[0] = DHCP_ACK;
            packet->header.yiaddr = packet->header.ciaddr;
        } else {
            data[0] = DHCP_NAK;
        }
    } else { // Dynamic IP allocation
        uint32_t ip_address = ((uint32_t)options->data[1] << 24) |
                              ((uint32_t)options->data[2] << 16) |
                              ((uint32_t)options->data[3] << 8) |
                              (uint32_t)options->data[4];

        packet->header.ciaddr = ip_address;
        if (confirm_ip_address(server, *packet, net)) {
            data[0] = DHCP_ACK;
            packet->header.yiaddr = packet->header.ciaddr;
        } else {
            data[0] = DHCP_NAK;
        }
    }

    parse_ip("0.0.0.0", &packet->header.ciaddr);
    *packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN,
                                                 packet->header.xid, packet->header.secs, packet->header.flags,
                                                 packet->header.ciaddr, packet->header.yiaddr,
                                                 server->server_ip, server->gateway, packet->header.chaddr,
                                                 DHCP_MESSAGE_TYPE, 1, data);

    if(data[0] == DHCP_ACK)
    {
        strncpy(data, LEASE_TIME, sizeof(data));
        data[4] = '\0';
        add_option_reply(&(*packet_to_send)->dhcp_options, IP_ADDRESS_LEASE_TIME, 4, data);
    }
}

void handle_dhcp_request_static(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send, dhcp_option *options)
{
    uint32_t requested_ip;
    requested_ip = (options->data[1] << 24) | (options->data[2] << 16) | (options->data[3] << 8) | options->data[4];
    char data[10];

    if(confirm_static_ip(server, *packet, net, requested_ip))
    {
        data[0] = DHCP_ACK;
        packet->header.yiaddr = requested_ip;
    }
    else
        data[0] = DHCP_NAK;

    *packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, 
                                                packet->header.xid, packet->header.secs, packet->header.flags, 
                                                packet->header.ciaddr, packet->header.yiaddr, 
                                                server->server_ip, server->gateway, packet->header.chaddr, 
                                                DHCP_MESSAGE_TYPE, 1, data);
    if(data[0] == DHCP_ACK)
    {
        strncpy(data, LEASE_TIME, sizeof(data));
        data[4] = '\0';
        add_option_reply(&(*packet_to_send)->dhcp_options, IP_ADDRESS_LEASE_TIME, 4, data);

    }
}

void handle_dhcp_decline(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send)
{
    char data[2];

    if (decline_ip(server, *packet, net)) {
        data[0] = DHCP_ACK;
        packet->header.ciaddr = 0;
        packet->header.yiaddr = 0;
    } else {
        data[0] = DHCP_NAK;
    }

    *packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN,
                                                 packet->header.xid, packet->header.secs, packet->header.flags,
                                                 packet->header.ciaddr, packet->header.yiaddr,
                                                 server->server_ip, server->gateway, packet->header.chaddr,
                                                 DHCP_MESSAGE_TYPE, 1, data);
}

void handle_dhcp_release(dhcp_packet **packet_to_send, server_configs *server, dhcp_packet *packet, network **net)
{
    char data[2];
    if (release_ip_address(server, *packet, net)) {
        data[0] = DHCP_ACK;
        log_msg(INFO, "server/handle_dhcp_release", "Releasing IP address successfully!");
    } else {
        data[0] = DHCP_NAK;
        log_msg(ERROR, "server/handle_dhcp_release", "Error at releasing IP address!");
    }
    
    *packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, 
                                                packet->header.xid, packet->header.secs, packet->header.flags, 
                                                packet->header.ciaddr, packet->header.yiaddr, 
                                                server->server_ip, server->gateway, packet->header.chaddr, 
                                                DHCP_MESSAGE_TYPE, 1, data);
}

void handle_dhcp_inform(server_configs *server, dhcp_packet *packet, network **net, dhcp_packet **packet_to_send)
{
    set_header(&(*packet_to_send)->header, BOOTREQUEST, ETHERNET, ETHERNET_LEN,
               packet->header.xid, packet->header.secs, packet->header.flags,
               packet->header.ciaddr, packet->header.yiaddr, packet->header.siaddr,
               packet->header.giaddr, packet->header.chaddr);

   
    dhcp_option *option;
    while ((option = queue_dequeue(&packet->dhcp_options)) != NULL) {
        switch (option->id) {
            case PARAMETER_REQUEST_LIST:
                if(option->data[0] == DOMAIN_NAME_SERVER)
                {
                    char dns_data[1 + (*net)->dns_count * 4];
                    dns_data[0] = DOMAIN_NAME_SERVER;
                    for (int i = 0; i < (*net)->dns_count; i++)
                    {
                        int val = i * 4 + 1;
                        convert_ip_to_bytes((*net)->dns_server[i], &dns_data[val]);
                        // Convert each DNS server address into 4 bytes
                    }
                    add_option_reply(&(*packet_to_send)->dhcp_options, PARAMETER_REQUEST_LIST, 1 + (*net)->dns_count * 4, dns_data);
                    break;
                }
                else if(option->data[0] == SUBNET_MASK)
                {
                    char netmask_data[100];
                    netmask_data[0] = SUBNET_MASK;
                    convert_ip((*net)->netmask, &netmask_data[1]);
                    add_option_reply(&(*packet_to_send)->dhcp_options, PARAMETER_REQUEST_LIST, strlen(netmask_data), netmask_data);
                    break;
                }

            default:
                log_msg(INFO, "server/handle_dhcp_inform", "Received unknown option!");
                break;
        }
    }
}

void send_dhcp_packet(int socketfd, dhcp_packet **packet_to_send, struct sockaddr_in *client_addr)
{
    char buffer[MAX_DHCP_SIZE];
    int len = serialize_packet(*packet_to_send, buffer);
    check(sendto(socketfd, &buffer, len, 0, (const struct sockaddr *)client_addr, sizeof(*client_addr)), 
          "Error at sending DHCP packet!");
}

void *handle_connection(void* pclient)
{
    struct client_data *data = (struct client_data*)pclient;
    int socketfd = data->socketfd;
    struct dhcp_packet packet = data->packet;
    struct sockaddr_in client_addr = data->client_addr;

    free(pclient);

    network *net = &server->networks[2];

    dhcp_option *options;
    dhcp_packet *packet_to_send = malloc(sizeof(dhcp_packet));
    if (!packet_to_send) {
        log_msg(ERROR, "server/handle_connection", "Memory allocation failed!");
        return NULL;
    }
    queue_init(&packet_to_send->dhcp_options);

    while ((options = queue_dequeue(&packet.dhcp_options))) {
        if (options->id == DHCP_MESSAGE_TYPE) {
            switch (options->data[0]) {
                case DHCP_DISCOVER:
                    handle_dhcp_discover(server, &packet, &net, &packet_to_send);
                    break;

                case DHCP_REQUEST:
                    handle_dhcp_request(server, &packet, &net, &packet_to_send, options);
                    break;

                case DHCP_DECLINE:
                    handle_dhcp_decline(server, &packet, &net, &packet_to_send);
                    break;

                case DHCP_RELEASE:
                    handle_dhcp_release(&packet_to_send, server, &packet, &net);
                    break;

                case DHCP_INFORM:
                    handle_dhcp_inform(server, &packet, &net, &packet_to_send);
                    break;

                default:
                    log_msg(INFO, "server/handle_connection", "Unhandled DHCP message type!");
                    break;
            }
        } else if (options->id == REQUESTED_IP_ADDRESS) {
           handle_dhcp_request_static(server, &packet, &net, &packet_to_send, options);
        }
    }

    send_dhcp_packet(socketfd, &packet_to_send, &client_addr);

    free(packet_to_send);
    return NULL;
}

void check(int result, const char* message)
{
    if(result < 0)
    {
        log_msg(ERROR, "server/check", message);
        exit(EXIT_FAILURE);
    }
}

// Worker thread processing incoming client data
void* thread_function(void* argument)
{
    while(running)
    {
        // Lock the mutex to safely access shared resources 
        pthread_mutex_lock(&mutex);

        // Dequeue a client data structure from the queue
        struct client_data* pclient = dequeue();

        // If the queue is empty, wait for a signal from another thread indicating that new client data is available
        if(pclient == NULL && running)
        {
            // Wait on the condition variable, releasing the mutex while waiting
            pthread_cond_wait(&condition_var, &mutex);
            
            // After being signaled, try to dequeue again
            pclient = dequeue();
        }
        
        // Unlock the mutex after we've finished accessing shared resources
        pthread_mutex_unlock(&mutex);

        // If we successfully dequeued a client data, process the connection
        if(pclient != NULL)
            handle_connection(pclient);
    }
}

void handle_sigint(int sig) 
{
    log_msg(INFO, "server/main", "SIGINT received, shutting down...");

    // Set the 'running' flag to 0, which will stop the main loop
    running = 0;

    // Lock the mutex to ensure thread safety when modifying shared resource
    pthread_mutex_lock(&mutex);

    // Broadcast a signal to wake up any threads that are waiting on the condition variable
    pthread_cond_broadcast(&condition_var);

    // Unlock the mutex after broadcasting the signal
    pthread_mutex_unlock(&mutex);
}

void get_memory_usage() 
{
    FILE *f = fopen("/proc/self/status", "r");
    if (f == NULL) {
        perror("Error opening /proc/self/status");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            printf("Memory Usage: %s", line);
        }
    }

    fclose(f);
}