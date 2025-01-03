#ifndef DHCP_HEADER_H
#define DHCP_HEADER_H

#include <stdint.h>
#include "dhcp_options.h"

#define DHCP_HEADER_SIZE 236    // without size of options

enum ports{
    BOOTPS = 67,
    BOOTPC = 68
};

typedef enum ports ports;

enum op_types{
    BOOTREQUEST = 1,
    BOOTREPLY   = 2,   
};

typedef enum op_types op_types;

enum hardware_address_types{
    ETHERNET     = 0x01,
    ETHERNET_LEN = 0x06,
};

typedef enum hardware_address_types hardware_address_types;

/* DHCP message */

typedef struct dhcp_header {
    uint8_t op;                                  //uint8_t op;      // message op code, message type
    uint8_t htype;                 // hardware address type
    uint8_t hlen;                                // hardware address length
    uint8_t hops;                                 // incremented by relay agents

    uint32_t xid;    // transaction ID

    uint16_t secs;   // seconds since address acquisition or renewal
    uint16_t flags;  // flags

    uint32_t ciaddr; // client IP address
    uint32_t yiaddr; // 'your' client IP address
    uint32_t siaddr; // IP address of the next server to use in bootstrap
    uint32_t giaddr; // relay agent IP address

    uint8_t chaddr[16]; // client hardware address BIG-ENDIAN

    uint8_t sname[64]; // server host name

    uint8_t file[128]; // boot file name
}dhcp_header;

void set_header(dhcp_header *header, op_types o, hardware_address_types h_type,hardware_address_types h_len, uint32_t xid, uint16_t secs,uint16_t flags, uint32_t c_address, uint32_t y_addr, uint32_t s_addr, uint32_t g_addr, uint8_t mac[16]);
void serialize_header(dhcp_header*h,char*buffer,size_t*len );

#endif
