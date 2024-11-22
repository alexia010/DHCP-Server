#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "dhcp_packet.h"
#include "dhcp_options.h"

#define DATA_SIZE 255

void create_discover(dhcp_packet **dhcp_packet_sent)
{
    unsigned char data[DATA_SIZE];
    memset(data, 0, sizeof(data));

    uint8_t mac[16];
    uint32_t ip;
    size_t data_length = 0;

    data[0] = DHCP_DISCOVER;
    data_length += 1;

    parse_ip("0.0.0.0", &ip);
    parse_mac("aa:bb:cc:dd:ee:ff", mac);   //aici intervine larisa cu alocarea mac urilor

    int rand_nb = rand() % 99 + 1;
    *dhcp_packet_sent = create_dhcp_packet_client(BOOTREQUEST, ETHERNET, ETHERNET_LEN, rand_nb, 0, 0, ip, ip, ip, ip, mac, DHCP_MESSAGE_TYPE, data_length, data);
    log_msg(INFO, "dhcp_client", "DHCP_DISCOVER packet created!");
}

void create_request_renew(dhcp_packet **dhcp_packet_sent)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet in renew request!");
        return;
    }

    uint32_t ip_address = (*dhcp_packet_sent)->header.ciaddr;
    if(ip_address == 0)
    {
        log_msg(ERROR, "dhcp_client", "Invalid IP address! (ciaddr is 0.0.0.0)");
    }

    unsigned char data[DATA_SIZE];
    memset(data, 0, sizeof(data));
    size_t data_length = 0;

    data[0] = DHCP_REQUEST;
    data_length += 1;
    
    convert_ip_to_bytes(ip_address, &data[1]);
    data_length += 4;
    

    *dhcp_packet_sent = create_dhcp_packet_client(  BOOTREQUEST,
                                                    ETHERNET,
                                                    ETHERNET_LEN,
                                                    (*dhcp_packet_sent)->header.xid,
                                                    (*dhcp_packet_sent)->header.secs,
                                                    (*dhcp_packet_sent)->header.flags,
                                                    (*dhcp_packet_sent)->header.ciaddr,
                                                    (*dhcp_packet_sent)->header.yiaddr,
                                                    (*dhcp_packet_sent)->header.siaddr,
                                                    (*dhcp_packet_sent)->header.giaddr,
                                                    (*dhcp_packet_sent)->header.chaddr,
                                                    DHCP_MESSAGE_TYPE, data_length, data);

    log_msg(INFO, "dhcp_client", "DHCP request packet created!");
}

void create_release(dhcp_packet **dhcp_packet_sent)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet in release request!");
        return;
    }

    uint32_t ip_address = (*dhcp_packet_sent)->header.ciaddr;
    if(ip_address == 0)
    {
        log_msg(ERROR, "dhcp_client", "Invalid IP address (ciaddr is 0.0.0.0)!");
        return;
    }

    unsigned char data[DATA_SIZE];
    memset(data, 0, sizeof(data));
    size_t data_length = 0;

    data[0] = DHCP_RELEASE;
    data_length += 1;
    
    convert_ip_to_bytes(ip_address, &data[1]);
    data_length += 4;

    *dhcp_packet_sent = create_dhcp_packet_client(  BOOTREQUEST,
                                                    ETHERNET,
                                                    ETHERNET_LEN,
                                                    (*dhcp_packet_sent)->header.xid,
                                                    (*dhcp_packet_sent)->header.secs,
                                                    (*dhcp_packet_sent)->header.flags,
                                                    (*dhcp_packet_sent)->header.ciaddr,
                                                    (*dhcp_packet_sent)->header.yiaddr,
                                                    (*dhcp_packet_sent)->header.siaddr,
                                                    (*dhcp_packet_sent)->header.giaddr,
                                                    (*dhcp_packet_sent)->header.chaddr,
                                                    DHCP_MESSAGE_TYPE, data_length, data);
    
    log_msg(INFO, "dhcp_client", "DHCP release packet created!");
}

void create_inform(dhcp_packet **dhcp_packet_sent)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet in inform request!");
        return;
    }

    unsigned char data[DATA_SIZE];
    size_t data_length = 0;
    memset(data, 0, sizeof(data));

    
    data[0] = DHCP_INFORM;
    data_length += 1;

    *dhcp_packet_sent = create_dhcp_packet_client(  BOOTREQUEST,
                                                    ETHERNET,
                                                    ETHERNET_LEN,
                                                    (*dhcp_packet_sent)->header.xid,
                                                    (*dhcp_packet_sent)->header.secs,
                                                    (*dhcp_packet_sent)->header.flags,
                                                    (*dhcp_packet_sent)->header.ciaddr,
                                                    (*dhcp_packet_sent)->header.yiaddr,
                                                    (*dhcp_packet_sent)->header.siaddr,
                                                    (*dhcp_packet_sent)->header.giaddr,
                                                    (*dhcp_packet_sent)->header.chaddr,
                                                    DOMAIN_NAME_SERVER, data_length, data);

    log_msg(INFO, "dhcp_client", "DHCP inform packet created!");
}

void create_request_dynamic(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received)
{
    if (!dhcp_packet_sent || !dhcp_packet_received) 
    {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet received or sent!");
        return;
    }

    uint32_t ip = dhcp_packet_received->header.yiaddr;
    if (ip == 0) 
    {
        log_msg(ERROR, "dhcp_client", "Received invalid IP address (yiaddr is 0.0.0.0)!");
        return;
    }

    unsigned char data[DATA_SIZE];
    size_t data_length = 0;
    
    data[0] = DHCP_REQUEST;
    data_length += 1;
    
    convert_ip_to_bytes(ip, &data[1]);
    data_length += 4;

    *dhcp_packet_sent = create_dhcp_packet_client(  BOOTREQUEST,
                                                    ETHERNET,
                                                    ETHERNET_LEN,
                                                    (*dhcp_packet_sent)->header.xid,
                                                    (*dhcp_packet_sent)->header.secs,
                                                    (*dhcp_packet_sent)->header.flags,
                                                    (*dhcp_packet_sent)->header.ciaddr,
                                                    (*dhcp_packet_sent)->header.yiaddr,
                                                    dhcp_packet_received->header.siaddr,
                                                    dhcp_packet_received->header.giaddr,
                                                    (*dhcp_packet_sent)->header.chaddr,
                                                    DHCP_MESSAGE_TYPE, data_length, data);

    log_msg(INFO, "dhcp_client", "Created request for dynamic IP!");
}

void create_request_static(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received)
{
    if (!dhcp_packet_sent || !dhcp_packet_received) 
    {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet received or sent!");
        return;
    }

    uint32_t ip;
    parse_ip("172.16.15.2", &ip);   //aici ar trebui sa extrag de undeva
    
    unsigned char data[DATA_SIZE];
    size_t data_length = 0;
    data[0] = DHCP_REQUEST;
    data_length += 1;
   
    convert_ip_to_bytes(ip, &data[1]);
    data_length += 4;

    *dhcp_packet_sent = create_dhcp_packet_client(  BOOTREQUEST,
                                                    ETHERNET,
                                                    ETHERNET_LEN,
                                                    (*dhcp_packet_sent)->header.xid,
                                                    (*dhcp_packet_sent)->header.secs,
                                                    (*dhcp_packet_sent)->header.flags,
                                                    (*dhcp_packet_sent)->header.ciaddr,
                                                    (*dhcp_packet_sent)->header.yiaddr,
                                                    dhcp_packet_received->header.siaddr,
                                                    dhcp_packet_received->header.giaddr,
                                                    (*dhcp_packet_sent)->header.chaddr,
                                                    REQUESTED_IP_ADDRESS, data_length, data);
}

void send_packet(dhcp_packet *dhcp_packet_sent, int sockfd, struct sockaddr_in server_addr)
{
    if (!dhcp_packet_sent) {
        log_msg(ERROR, "dhcp_client", "Null DHCP packet!");
        return;
    }

    if (sockfd < 0) {
        log_msg(ERROR, "dhcp_client", "Invalid socket descriptor!");
        return;
    }

    char buffer_sent[MAX_DHCP_SIZE];
    int size = serialize_packet(dhcp_packet_sent, buffer_sent);

    if (size <= 0) {
        log_msg(ERROR, "dhcp_client", "Serialization failed!");
        return;
    }

    check(sendto(sockfd, &buffer_sent, size, 0, (const struct sockaddr *) &server_addr, sizeof(server_addr)), "Error at sending DHCP message!");
    log_msg(INFO, "dhcp_client", "DHCP message sent!");
    printf("message sent!\n");
}

void receive_packet(dhcp_packet **dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, socklen_t addr_len)
{
    char buffer_received[MAX_DHCP_SIZE];
    int rb = recvfrom(sockfd, buffer_received, MAX_DHCP_SIZE, 0, (struct sockaddr*)&server_addr, &addr_len);
    if(rb < 0)
    {
        log_msg(ERROR, "main", "Error at receiving packet from client!");
        exit(-1);
    }

    dhcp_packet temp_packet;
    queue_init(&temp_packet.dhcp_options);
    deserialize(buffer_received, rb, &temp_packet);

    memcpy(*dhcp_packet_received, &temp_packet, sizeof(dhcp_packet));
}

void check(int result, const char* message)
{
    if(result < 0)
    {
        log_msg(ERROR, "main", message);
        exit(EXIT_FAILURE);
    }
}

void convert_ip_to_bytes(uint32_t ip_address, unsigned char *data)
{
    data[0] = (ip_address >> 24) & 0xFF;
    data[1] = (ip_address >> 16) & 0xFF;
    data[2] = (ip_address >> 8) & 0xFF;
    data[3] = ip_address & 0xFF;
}