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


int main() 
{
    log_init("log.txt");
    srand(time(NULL));
    
    dhcp_packet *dhcp_packet_sent, *dhcp_packet_received;
    dhcp_packet_received = (dhcp_packet *)malloc(sizeof(dhcp_packet));
    if (dhcp_packet_received == NULL) 
    {
        log_msg(ERROR, "main", "Failed to allocate memory for DHCP packet.");
        exit(-1);
    }

	int sockfd;

	struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(DHCP_SERVER_PORT); 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	check(sockfd = socket(AF_INET, SOCK_DGRAM, 0), "Error at creating the socket!");


    //configurarea optiunii de broadcast pentru un socket
    int broadcastEnable = 0;
    check(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)), "Error at setting broadcast option!");
 
	socklen_t len;

    int retries = 0;
    dhcp_option *option;
    bool static_ip = false;

    while (1) 
    {
        // Step 1: Send DHCP Discover
        create_discover(&dhcp_packet_sent);
        send_packet(dhcp_packet_sent, sockfd, server_addr);

        // Step 2: Wait DHCP Offer
        receive_packet(&dhcp_packet_received, sockfd, server_addr, addr_len);
        option = queue_dequeue(&dhcp_packet_received->dhcp_options);
        if (option->data[0] != DHCP_OFFER) 
        {
            log_msg(WARNING, "dhcp_client", "Didn't receive OFFER, resending DISCOVER!");
            retries++;
            if (retries >= 3) 
            {
                log_msg(ERROR, "dhcp_client", "Wait time elapsed for OFFER!");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        // Step 3: Send DHCP Request (dynamic or static)
        if (!static_ip) 
            create_request_dynamic(&dhcp_packet_sent, dhcp_packet_received);
        else 
            create_request_static(&dhcp_packet_sent, dhcp_packet_received);
        
        send_packet(dhcp_packet_sent, sockfd, server_addr);

        // Step 4: Wait ACK or NAK
        receive_packet(&dhcp_packet_received, sockfd, server_addr, addr_len);
        
        option = queue_dequeue(&dhcp_packet_received->dhcp_options);
        if (option->data[0] == DHCP_ACK) 
        {
            log_msg(INFO, "dhcp_client", "ACK received, address allocated succesfully!");
            dhcp_packet_sent->header.ciaddr = dhcp_packet_received->header.yiaddr;
            printf("IP alocat: %s\n", inet_ntoa(*(struct in_addr *)&dhcp_packet_received->header.yiaddr));
            
            
            create_inform(&dhcp_packet_sent);
            send_packet(dhcp_packet_sent, sockfd, server_addr);
            receive_packet(&dhcp_packet_received, sockfd, server_addr, addr_len);
            printf("%x %x", dhcp_packet_received->header.siaddr, dhcp_packet_received->header.giaddr);
            
            break; 
        } 
        else if (option->data[0] == DHCP_NAK) 
        {
            log_msg(WARNING, "dhcp_client", "NAK received, trying again!");
            retries++;
            if (retries >= RETRIES) 
            {
                log_msg(ERROR, "dhcp_client", "Wait time elapsed for ACK/NAK");
                exit(EXIT_FAILURE);
            }
            continue; // Resume DISCOVER process
        }
    }

    // Step 5: Let's make renew
    while (1) 
    {
        sleep(5); // aici am nevoie de jumatatea timpului din lease dar asta dupa ce actualizez baza de date
        create_request_renew(&dhcp_packet_sent);
        send_packet(dhcp_packet_sent, sockfd, server_addr);

        receive_packet(&dhcp_packet_received, sockfd, server_addr, addr_len);
        option = queue_dequeue(&dhcp_packet_received->dhcp_options);
        if (option->data[0] == DHCP_ACK) 
        {
            log_msg(INFO, "dhcp_client", "IP address renewed succesfully!");

            create_release(&dhcp_packet_sent);
            send_packet(dhcp_packet_sent, sockfd, server_addr);
            log_msg(INFO, "dhcp_client", "IP released, client exiting.");
        } 
        else if (option->data[0] == DHCP_NAK) 
        {
            log_msg(WARNING, "dhcp_client", "Renewal failed! Starting over the allocation process!");
            break; // exit for restarting DISCOVER cycle
        }
    }

	close(sockfd);

	return 0;
}