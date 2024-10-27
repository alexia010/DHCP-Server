#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "dhcp_packet.h"

struct client_data
{
    int socketfd;
    struct sockaddr_in client_addr;
    struct dhcp_packet packet;
};