#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "dhcp_client.h"

#define DHCP_SERVER_PORT 2000
#define DHCP_CLIENT_PORT 2001
#define RETRIES 3

time_t lease_time_end = 0;
bool has_address = false;

void init_socket(int *sockfd, struct sockaddr_in *server_addr);
void dhcp_discovery(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received);
void handle_dhcp_offer(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, bool static_ip,  char* ip_req);
void handle_dhcp_ack(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, int sockfd, struct sockaddr_in server_addr,char *data_req, int len);
void renew_ip(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received);
void release_ip(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received);

int main() 
{
    log_init("log.txt");
    srand(time(NULL));
    bool running = true;
    int choice = 0;

    int sockfd;
    struct sockaddr_in server_addr;
    dhcp_packet *dhcp_packet_sent = NULL;
    dhcp_packet *dhcp_packet_received;

    bool static_ip = false;
    
    // Initialize the socket
    init_socket(&sockfd, &server_addr);

    // Allocate memory for the received DHCP packet
    dhcp_packet_received = (dhcp_packet *)malloc(sizeof(dhcp_packet));
    if (dhcp_packet_received == NULL) 
    {
        log_msg(ERROR, "client/main", "Failed to allocate memory for DHCP packet.");
        exit(EXIT_FAILURE);
    }

    while (running) {
        printf("\nWhat would you like to do?\n");
        printf("1. Request IP address\n");
        printf("2. Request IP address static\n");
        printf("3. Request information\n");
        printf("4. Renew IP address\n");
        printf("5. Release IP address\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("\n\nDiscovering DHCP server...\n");
                dhcp_discovery(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
                printf("Requesting dynamic IP...\n");
                static_ip = false;
                handle_dhcp_offer(&dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr, static_ip, NULL);
                break;
            case 2: {
                printf("Discovering DHCP server...\n");
                dhcp_discovery(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
                static_ip = true;
                char static_ip_str[INET_ADDRSTRLEN];
                printf("Please enter the static IP address (e.g., 192.168.1.100): ");
                scanf("%s", static_ip_str);
                printf("Requesting static IP...\n");
                handle_dhcp_offer(&dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr, static_ip, static_ip_str);
                break;
            }
            case 3:
                int option;
                char data[2];
                int len = 0;
                printf("Please enter the number corresponding to the options you want to request:\n");
                printf("1. Domain Name Server (DNS)\n");
                printf("2. Subnet Mask\n");
                printf("3. Both (DNS and Subnet Mask)\n");
                printf("Enter your choice (1, 2, or 3): ");
                scanf("%d", &option);
                if(has_address == false)
                {
                    log_msg(ERROR, "client/main", "You don't have an IP to fo this!");
                    break;
                }
                switch (option) {
                    case 1:
                        printf("You selected: Domain Name Server (DNS).\n");
                        data[0] = DOMAIN_NAME_SERVER;
                        len = 1;
                        break;
                    case 2:
                        printf("You selected: Subnet Mask.\n");
                        data[0] = SUBNET_MASK;
                        len = 1;
                        break;
                    case 3:
                        printf("You selected: Both (DNS and Subnet Mask).\n");
                        data[0] = DOMAIN_NAME_SERVER;
                        data[1] = SUBNET_MASK;
                        len = 2;
                        break;
                    default:
                        printf("Invalid choice. Please enter 1, 2, or 3.\n");
                        break;
                }
                printf("Requesting information from the server...\n");
                handle_dhcp_ack(&dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr, data, len);
                break;
            case 4:
                if(has_address == false)
                {
                    log_msg(ERROR, "client/main", "You don't have an IP to fo this!");
                    break;
                }
                printf("Renewing IP address...\n");
                renew_ip(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
                break;
            case 5:
                if(has_address == false)
                {
                    log_msg(ERROR, "client/main", "You don't have an IP to fo this!");
                    break;
                }
                printf("Releasing IP address...\n");
                release_ip(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
                break;
            case 6:
                printf("Exiting...\n");
                running = false;
                break;
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }

/*
    // dhcp_discovery(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
    // handle_dhcp_offer(&dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr, static_ip, "172.12.12.12");

    // the way that the server should run 
    // while (1) 
    // {
    //     time_t now;
    //     time(&now);

    //     // When the lease expires
    //     if (now >= lease_time_end) 
    //     {
    //         log_msg(WARNING, "dhcp_client", "Lease expired. Restarting discovery process...");
    //         dhcp_discovery(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);
    //         handle_dhcp_offer(&dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr, static_ip);
    //     }

    //     // Renew process
    //     renew_ip(sockfd, server_addr, &dhcp_packet_sent, dhcp_packet_received);

    //     // Sleep between checks
    //     sleep(10);
    // }
*/

    // Close the socket and free allocated memory
    close(sockfd);
    free(dhcp_packet_received);

    return 0;
}

// Function to initialize the UDP socket
void init_socket(int *sockfd, struct sockaddr_in *server_addr) 
{
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    check(*sockfd, "Error at creating the socket!");

    // Configure server address
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(DHCP_SERVER_PORT);
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);

    // Enable broadcast option for the socket
    int broadcastEnable = 1;
    check(setsockopt(*sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)), "Error at setting broadcast option!");
}

// Function to start DHCP discovery
void dhcp_discovery(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received) 


{
    int retries = 0;
    dhcp_option *option;

    while (1) 
    {
        // Step 1: Send DHCP Discover
        create_discover(dhcp_packet_sent);
        time_t send_time = time(NULL);
        printf("DHCP discover packet!\n");
        print_dhcp_packet(*dhcp_packet_sent);

        send_packet(*dhcp_packet_sent, sockfd, server_addr);
        log_msg(INFO, "client/dhcp_discovery", "DHCP discovery sent!");

        // Step 2: Wait for DHCP Offer
        receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
        time_t receive_time = time(NULL);
        printf("DHCP offer packet!\n");
        print_dhcp_packet(dhcp_packet_received);

        (*dhcp_packet_sent)->header.secs = (uint16_t)(receive_time - send_time);
        char message[50];
        snprintf(message, 50, "Time elapsed from discovery to offer is: %d!", (*dhcp_packet_sent)->header.secs);

        option = queue_dequeue(&dhcp_packet_received->dhcp_options);
        if(option == NULL)
        {
            log_msg(ERROR, "client/dhcp_discovery", "Option list is emply!");
            exit(-1);
        }
        if (option->data[0] == DHCP_OFFER) 
        {
            log_msg(INFO, "client/dhcp_discovery", "DHCP OFFER received.");
            log_msg(INFO, "client/dhcp_discovery", message);
            break;
        } 
        else 
        {
            log_msg(WARNING, "client/dhcp_discovery", "Didn't receive OFFER, resending DISCOVER!");
            retries++;
            if (retries >= RETRIES) 
            {
                log_msg(ERROR, "client/dhcp_discovery", "Wait time elapsed for OFFER!");
                exit(-1);
            }
        }
    }
}

// Function to handle DHCP Offer and Request
void handle_dhcp_offer(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, bool static_ip, char* ip_req) 
{
    // Send DHCP Request (dynamic or static)
    if (!static_ip) 
        create_request_dynamic(dhcp_packet_sent, dhcp_packet_received);
    else 
        create_request_static(dhcp_packet_sent, dhcp_packet_received, ip_req);
    
    printf("DHCP request packet!\n");
    print_dhcp_packet(*dhcp_packet_sent);
    send_packet(*dhcp_packet_sent, sockfd, server_addr);
    time_t time_send = time(NULL);

    // Step 4: Wait for ACK or NAK
    receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
    time_t time_receive = time(NULL);
    (*dhcp_packet_sent)->header.secs = (uint16_t)(time_receive - time_send);

    char message[50];

    printf("DHCP packet!\n");
    print_dhcp_packet(dhcp_packet_received);
    
    dhcp_option *option = queue_dequeue(&dhcp_packet_received->dhcp_options);
    if (option->data[0] == DHCP_ACK) 
    {
        snprintf(message, 50, "Time elapsed from request ip to ack is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/handle_dhcp_offer", message);
        option = queue_dequeue(&dhcp_packet_received->dhcp_options);

        // uint8_t to time_t 
        char buffer[5];
        for(int i = 0; i < option->len; i++)
        {
            buffer[i] = (char)option->data[i];
        }
        
        char *endptr;
        long numeric_value = strtol(buffer, &endptr, 10);

        if (*endptr != '\0') {
            log_msg(ERROR, "client/handle_dhcp_offer", "Lease time string contains invalid caracters!");
            exit(-1);
        }
        lease_time_end = (time_t)numeric_value + time(NULL);
        log_msg(INFO, "client/handle_dhcp_offer", "Lease time end set successfully.");
        // end uint8_t to time_t

        log_msg(INFO, "client/handle_dhcp_offer", "ACK received, address allocated successfully!");
        (*dhcp_packet_sent)->header.ciaddr = dhcp_packet_received->header.yiaddr;

        has_address = true;
        // used when not testing
        // handle_dhcp_ack(dhcp_packet_sent, dhcp_packet_received, sockfd, server_addr);
    } 
    else if (option->data[0] == DHCP_NAK) 
    {
        has_address = false;
        snprintf(message, 50, "Time elapsed from request ip to nak is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/handle_dhcp_offer", message);
        log_msg(WARNING, "client/handle_dhcp_offer", "NAK received, trying again!");
    }
}

// Function to handle DHCP ACK
void handle_dhcp_ack(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, char *data_req, int len) 
{
    create_inform(dhcp_packet_sent, data_req, len);
    printf("DHCP inform packet!\n");
    print_dhcp_packet(*dhcp_packet_sent);
    send_packet(*dhcp_packet_sent, sockfd, server_addr);
    time_t send_time = time(NULL);

    receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
    time_t receive_time = time(NULL);
    printf("DHCP inform result packet!\n");
    print_dhcp_packet(dhcp_packet_received);

    (*dhcp_packet_sent)->header.secs = (uint16_t)(receive_time - send_time);
    char message[50];
    snprintf(message, 50, "Time elapsed from inform to response inform is: %d !", (*dhcp_packet_sent)->header.secs);
    log_msg(INFO, "client/handle_dhcp_ack", message);

    // Start the IP renewal process
    //renew_ip(sockfd, server_addr, dhcp_packet_sent, dhcp_packet_received);
}

// Function to renew the IP address but for test
void renew_ip(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received) 
{
    sleep(5);
    
    create_request_renew(dhcp_packet_sent);

    printf("DHCP renew packet!\n");
    print_dhcp_packet(*dhcp_packet_sent);
    send_packet(*dhcp_packet_sent, sockfd, server_addr);
    time_t send_time = time(NULL);

    receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
    time_t receive_time = time(NULL);

    (*dhcp_packet_sent)->header.secs = (uint16_t)(receive_time - send_time);
    char message[50];

    dhcp_option *option = queue_dequeue(&dhcp_packet_received->dhcp_options);
    if (option->data[0] == DHCP_ACK) 
    {
        snprintf(message, 50, "Time elapsed from renew ip to ack is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/renew_ip", message);
        log_msg(INFO, "client/renew_ip", "IP address renewed successfully!");
    } 
    else if (option->data[0] == DHCP_NAK) 
    {
        snprintf(message, 50, "Time elapsed from renew ip to nak is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/renew_ip", message);
        log_msg(WARNING, "client/renew_ip", "Renewal failed! Starting over the allocation process!");
    }
}

/*
// void renew_ip(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received) 
// {
//     time_t now;
//     time_t renew_time = lease_time_end / 2;  // First time renew
//     time_t nak_retry_time = lease_time_end / 3;  // Retry in case of NAK

//     while (1) 
//     {
//         time(&now);

//         if (now >= renew_time) 
//         {
//             log_msg(INFO, "dhcp_client", "Sending DHCP RENEW request.");
//             create_request_renew(dhcp_packet_sent);
//             send_packet(*dhcp_packet_sent, sockfd, server_addr);

//             receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
//             dhcp_option *option = queue_dequeue(&dhcp_packet_received->dhcp_options);

//             if (option->data[0] == DHCP_ACK) 
//             {
//                 log_msg(INFO, "dhcp_client", "IP address renewed successfully!");
//                 lease_time_end = now + lease_time_end;  // Extend lease time
//                 return;
//             } 
//             else if (option->data[0] == DHCP_NAK) 
//             {
//                 log_msg(WARNING, "dhcp_client", "Renewal failed (NAK received). Retrying...");
//                 renew_time = now + nak_retry_time;  // Retry later
//             }
//         }

//         // If current time overlaped lease_time_end
//         if (now >= lease_time_end) 
//         {
//             log_msg(ERROR, "dhcp_client", "Lease expired! Releasing IP...");
//             release_ip(sockfd, server_addr, dhcp_packet_sent);
//             break; 
//         }

//         // How much time to wait until next event
//         time_t wait_time = renew_time > now ? renew_time - now : lease_time_end - now;

//         if (wait_time < 0)
//             wait_time = 1;

//         sleep(wait_time);  // Wait until next request
//     }
// }
*/

// Function to release the IP address
void release_ip(int sockfd, struct sockaddr_in server_addr, dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received) 
{
    if(!dhcp_packet_received || !dhcp_packet_sent)
    {
        log_msg(ERROR, "client/release_ip", "You have to do request IP first!");
        exit(-1);
    }

    create_release(dhcp_packet_sent);
    printf("DHCP release packet!\n");
    print_dhcp_packet(*dhcp_packet_sent);

    send_packet(*dhcp_packet_sent, sockfd, server_addr);
    time_t send_time = time(NULL);

    receive_packet(&dhcp_packet_received, sockfd, server_addr, sizeof(server_addr));
    time_t receive_time = time(NULL);

    (*dhcp_packet_sent)->header.secs = (uint16_t)(receive_time - send_time);
    char message[50];

    dhcp_option *option = queue_dequeue(&dhcp_packet_received->dhcp_options);
    if (option->data[0] == DHCP_ACK) 
    {
        has_address = false;
        snprintf(message, 50, "Time elapsed from release ip to ack is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/release_ip", message);
        log_msg(INFO, "client/release_ip", "IP address released successfully!");
    } 
    else if (option->data[0] == DHCP_NAK) 
    {
        has_address = true;
        snprintf(message, 50, "Time elapsed from release ip to nak is: %d!", (*dhcp_packet_sent)->header.secs);
        log_msg(INFO, "client/release_ip", message);
        log_msg(WARNING, "client/release_ip", "Released failed!");
    }
}