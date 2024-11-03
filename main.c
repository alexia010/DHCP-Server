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

//#include "my_queue.h"

#include "dhcp_packet.h"
#include "logger.h"

#include "dhcp_server.h"



int main()
{

    server_configs *s=malloc(sizeof(server_configs));
    initialize_server(s,SERVER_IP,NETMASK,GATEWAY,"intefffffff.org",LEASE_TIME,PEDDING_TIME,RENEWAL_TIME,REBINDING_TIME);
    s->networks_nr=initialize_networks(s->networks);

    char data[2];
    data[0]=DHCP_REQUEST;
    dhcp_packet*p=create_dhcp_request_packet(BOOTREQUEST,ETHERNET,ETHERNET_LEN,1,45,0,"172.20.10.3","172.20.10.15","172.20.10.1","10.10.10.1","AA:BB:CC:DD:EE:FF",53,1,data);


    char buffer[MAX_DHCP_SIZE];
    size_t pck_size=serialize_packet(p,buffer);
    dhcp_packet*p1=malloc(sizeof(dhcp_packet));

    deserialize(buffer,pck_size,p1);

    printf("%s",buffer);

    
    return 0;

    
}
 