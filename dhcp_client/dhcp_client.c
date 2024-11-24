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
    uint32_t ip, br;
    size_t data_length = 0;

    data[0] = DHCP_DISCOVER;
    data_length += 1;

    parse_ip("0.0.0.0", &ip);
    parse_ip("255.255.255.255", &br);
    parse_mac("aa:bb:cc:dd:ee:ff", mac);   //aici intervine larisa cu alocarea mac urilor

    int rand_nb = rand() % 99 + 1;
    *dhcp_packet_sent = create_dhcp_packet_client(BOOTREQUEST, ETHERNET, ETHERNET_LEN, rand_nb, 0, 0, ip, ip, br, ip, mac, DHCP_MESSAGE_TYPE, data_length, data);
    log_msg(INFO, "dhcp_client/create_discover", "DHCP_DISCOVER packet created!");
}

void create_request_renew(dhcp_packet **dhcp_packet_sent)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client/create_request_renew", "Null DHCP packet in renew request!");
        return;
    }

    uint32_t ip_address = (*dhcp_packet_sent)->header.ciaddr;
    if(ip_address == 0)
    {
        log_msg(ERROR, "dhcp_client/create_request_renew", "Invalid IP address! (ciaddr is 0.0.0.0)");
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

    log_msg(INFO, "dhcp_client/create_request_renew", "DHCP renew packet created!");
}

void create_release(dhcp_packet **dhcp_packet_sent)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client/create_release", "Null DHCP packet in release request!");
        return;
    }

    uint32_t ip_address = (*dhcp_packet_sent)->header.ciaddr;
    if(ip_address == 0)
    {
        log_msg(ERROR, "dhcp_client/create_release", "Invalid IP address (ciaddr is 0.0.0.0)!");
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
    
    log_msg(INFO, "dhcp_client/create_release", "DHCP release packet created!");
}

void create_inform(dhcp_packet **dhcp_packet_sent, char *data_req, int len)
{
    if (!dhcp_packet_sent) 
    {
        log_msg(ERROR, "dhcp_client/create_inform", "Null DHCP packet in inform request!");
        exit(-1);
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
                                                    DHCP_MESSAGE_TYPE, data_length, data);

    data[0] = data_req[0];
    add_option_request(&(*dhcp_packet_sent)->dhcp_options, PARAMETER_REQUEST_LIST, data_length, data);

    if (len == 2)
    {
        data[0] = data_req[1];
        add_option_request(&(*dhcp_packet_sent)->dhcp_options, PARAMETER_REQUEST_LIST, data_length, data);
    }

    log_msg(INFO, "dhcp_client/create_inform", "DHCP inform packet created!");
}

void create_request_dynamic(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received)
{
    if (!dhcp_packet_sent || !dhcp_packet_received) 
    {
        log_msg(ERROR, "dhcp_client/create_request_dynamic", "Null DHCP packet received or sent!");
        return;
    }

    uint32_t ip = dhcp_packet_received->header.yiaddr;
    if (ip == 0) 
    {
        log_msg(ERROR, "dhcp_client/create_request_dynamic", "Received invalid IP address (yiaddr is 0.0.0.0)!");
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

    log_msg(INFO, "dhcp_client/create_request_dynamic", "Created request for dynamic IP!");
}

void create_request_static(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, char *static_ip)
{
    if (!dhcp_packet_sent || !dhcp_packet_received) 
    {
        log_msg(ERROR, "dhcp_client/create_request_static", "Null DHCP packet received or sent!");
        return;
    }

    uint32_t ip;
    parse_ip(static_ip, &ip);   // i'm gonna hardcode one ip if we're not testing
    
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
        log_msg(ERROR, "dhcp_client/send_packet", "Null DHCP packet!");
        return;
    }

    if (sockfd < 0) {
        log_msg(ERROR, "dhcp_client/send_packet", "Invalid socket descriptor!");
        return;
    }

    char buffer_sent[MAX_DHCP_SIZE];
    int size = serialize_packet(dhcp_packet_sent, buffer_sent);

    if (size <= 0) {
        log_msg(ERROR, "dhcp_client/send_packet", "Serialization failed!");
        return;
    }

    check(sendto(sockfd, &buffer_sent, size, 0, (const struct sockaddr *) &server_addr, sizeof(server_addr)), "Error at sending DHCP message!");
    log_msg(INFO, "dhcp_client/send_packet", "DHCP message sent!");
}

void receive_packet(dhcp_packet **dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, socklen_t addr_len)
{
    char buffer_received[MAX_DHCP_SIZE];
    int rb = recvfrom(sockfd, buffer_received, MAX_DHCP_SIZE, 0, (struct sockaddr*)&server_addr, &addr_len);
    if(rb < 0)
    {
        log_msg(ERROR, "dhcp_client/receive_packet", "Error at receiving packet from client!");
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
        log_msg(ERROR, "dhcp_client/check", message);
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


void print_dhcp_packet(const dhcp_packet *packet) 
{
    if (packet == NULL) 
    {
        printf("DHCP packet is NULL.\n");
        return;
    }

    // Print the DHCP header fields
    printf("DHCP Packet Header:\n");
    printf("-------------------\n");
    printf("Operation (op): %u\n", packet->header.op);
    printf("Hardware Type (htype): %u\n", packet->header.htype);
    printf("Hardware Address Length (hlen): %u\n", packet->header.hlen);
    printf("Hops: %u\n", packet->header.hops);
    printf("Transaction ID (xid): 0x%x\n", packet->header.xid);
    printf("Seconds Elapsed (secs): %u\n", ntohs(packet->header.secs));
    printf("Flags: 0x%x\n", ntohs(packet->header.flags));
    uint32_t ip = packet->header.ciaddr;
    uint32_t ip2 = ((ip >> 24) & 0xFF) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | ((ip << 24) & 0xFF000000);
    printf("Client IP Address (ciaddr): %s\n", inet_ntoa(*(struct in_addr *)&ip2));
    ip = packet->header.yiaddr;
    ip2 = ((ip >> 24) & 0xFF) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | ((ip << 24) & 0xFF000000);
    printf("Your IP Address (yiaddr): %s\n", inet_ntoa(*(struct in_addr *)&ip2));
    ip = packet->header.siaddr;
    ip2 = ((ip >> 24) & 0xFF) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | ((ip << 24) & 0xFF000000);
    printf("Next Server IP Address (siaddr): %s\n", inet_ntoa(*(struct in_addr *)&ip2));
    ip = packet->header.giaddr;
    ip2 = ((ip >> 24) & 0xFF) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | ((ip << 24) & 0xFF000000);
    printf("Relay Agent IP Address (giaddr): %s\n", inet_ntoa(*(struct in_addr *)&ip2));



    // Print the client hardware address (MAC address)
    printf("Client Hardware Address (chaddr): ");
    for (int i = 0; i < packet->header.hlen; ++i) 
    {
        printf("%02x", packet->header.chaddr[i]);
        if (i < packet->header.hlen - 1) 
            printf(":");
    }
    printf("\n");

    // Print the server host name
    printf("Server Host Name (sname): %s\n", packet->header.sname);

    // Print the boot file name
    printf("Boot File Name (file): %s\n", packet->header.file);

    printf("\nDHCP Options:\n");

    // Traverse the options queue and print each option
    
    node *current_option = packet->dhcp_options.head;
    
    while (current_option != NULL) 
    {
        dhcp_option *opt = (dhcp_option*)current_option->data;
        printf("Option ID: %u\n", opt->id);
        printf("Option Length: %u\n", opt->len);
        printf("Option Data: ");
        for (int i = 0; i < opt->len; ++i) 
        {
            printf("%02x ", opt->data[i]);
        }
        printf("\n");
        printf("-------------\n");

        if(opt->id == PARAMETER_REQUEST_LIST)
        {
            if(opt->data[0] == DOMAIN_NAME_SERVER)
            {
                int val = 1;
                while(opt->len > 4)
                {
                    uint32_t ip_address = (opt->data[val]) | (opt->data[val + 1] << 8) | (opt->data[val + 2] << 16) | (opt->data[val + 3] << 24);
                    char str[20];
                    sprintf(str, "%u.%u.%u.%u", 
                            (ip_address & 0xFF), 
                            (ip_address >> 8) & 0xFF, 
                            (ip_address >> 16) & 0xFF, 
                            (ip_address >> 24) & 0xFF);

                    char msg[50];
                    printf("DNS SERVER: %s\n\n", str);
                    opt->len -= 4;
                    val = val + 4;
                }
            }
            else if(opt->data[0] == SUBNET_MASK)
            {
                char str[20];
                printf("SUBNET MASK: ");
                for (int i = 1; i < opt->len; i++)
                {
                    if(opt->data[i] != '.')
                        printf("%u", opt->data[i] - 48);
                    else printf(".");
                }
                printf("\n");
            } 
        }
        
        current_option = current_option->next;
    }
    printf("\n\n-------------\n");
}