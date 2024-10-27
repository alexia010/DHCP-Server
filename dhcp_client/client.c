#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "dhcp_packet.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

void check(int result, const char* message);
void initialize_dhcp_packet(struct dhcp_packet *packet, const char *mac_address);

int main() 
{
	int sockfd;

    struct dhcp_packet packet;
	struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(DHCP_SERVER_PORT); 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

 

	check(sockfd = socket(AF_INET, SOCK_DGRAM, 0), "Eroare la creearea socket-ului!");

    initialize_dhcp_packet(&packet, "00:1A:2B:3C:4D:5E");
 
	socklen_t len;
 
	check(sendto(sockfd, (char*)&packet, sizeof(struct dhcp_packet), MSG_CONFIRM, (const struct sockaddr *) &server_addr, sizeof(server_addr)), "Eroare la DHCPDISCOVER");
    printf("DISCOVER message sent!\n");

 
	check(recvfrom(sockfd, (char *)&packet, sizeof(struct dhcp_packet), MSG_WAITALL, (struct sockaddr *)&server_addr, &len), "Eroare la receive DHCPDISCOVER from server");
    printf("Received DHCPOFFER from server (XID: %x)\n", ntohl(packet.xid));
    printf("Offered IP: %s\n", inet_ntoa(*(struct in_addr *)&packet.yiaddr));

    sleep(20);
	close(sockfd);

	return 0;
}


void check(int result, const char* message)
{
    if(result < 0)
    {
        perror(message);
        exit(EXIT_FAILURE);
    }
}


void initialize_dhcp_packet(struct dhcp_packet *packet, const char *mac_address)
{
    memset(packet, 0, sizeof(struct dhcp_packet)); // Curăță structura pachetului

    packet->op = REPLY; // BOOTREQUEST
    packet->htype = 1; // Ethernet
    packet->hlen = 6; // Lungimea adresei hardware (MAC)
    packet->xid = htonl(rand()); // ID tranzacție (aleator)
    packet->secs = 0; // Secunde scurse
    packet->flags = 0; // Flagi
    packet->ciaddr = 0; // Adresa IP a clientului
    packet->yiaddr = 0; // Adresa ta IP (client)
    packet->siaddr = 0; // Adresa IP a serverului
    packet->giaddr = 0; // Adresa IP a gateway-ului
    memset(packet->chaddr, 0, 16); // Adresa hardware a clientului
    memcpy(packet->chaddr, mac_address, 6); // Adresa MAC specificată
    packet->options[0] = DHCP_OPTION_MSG_TYPE; // Cod opțiune pentru tipul de mesaj
    packet->options[1] = 1; // Lungimea opțiunii
    packet->options[2] = DHCPDISCOVER; // Tip mesaj (DHCPDISCOVER)
    packet->options[3] = 0xFF; // Sfârșitul opțiunilor
}