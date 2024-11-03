#ifndef DHCP_PACKET_H
#define DHCP_PACKET_H

#include <stdio.h>
#include <stdint.h>

#include "dhcp_header.h"
#include "dhcp_options.h"

#define MAX_DHCP_SIZE 576


struct dhcp_packet
{
    dhcp_header header;
    queue dhcp_options;
};

typedef struct dhcp_packet dhcp_packet;


dhcp_packet* create_dhcp_request_packet(op_types o, hardware_address_types h_type,hardware_address_types h_len,uint32_t xid,\
    uint16_t secs,uint16_t flags,const char *c_address,const char *y_addr,const char*s_addr,const char*g_addr,const char *mac,\
    uint8_t id,uint8_t len,const char*data);

size_t serialize_packet(dhcp_packet*p,char*buffer);

void deserialize(char*buffer,size_t len, dhcp_packet*p);

#endif