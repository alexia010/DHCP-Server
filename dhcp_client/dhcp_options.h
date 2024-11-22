#ifndef DHCP_OPTIONS_H
#define DHCP_OPTIONS_H

#include "queue.h"
#include "logger.h"

#include <stdint.h>

enum dhcp_msg_type {
     DHCP_DISCOVER = 1,
     DHCP_OFFER    = 2,
     DHCP_REQUEST  = 3,
     DHCP_DECLINE  = 4, //?
     DHCP_ACK      = 5,
     DHCP_NAK      = 6,
     DHCP_RELEASE  = 7,
     DHCP_INFORM   = 8,
};

enum option{
     PAD = 0,
     END = 255,
     SUBNET_MASK = 1,
     //ROUTER=3,
     DOMAIN_NAME_SERVER=6,
     BROADCAST_ADDRESS=28,

     /* DHCP Extensions */

    REQUESTED_IP_ADDRESS = 50,
    IP_ADDRESS_LEASE_TIME = 51,
    //OPTION_OVERLOAD = 52,
    //TFTP_SERVER_NAME = 66,
   // BOOTFILE_NAME = 67,
    DHCP_MESSAGE_TYPE = 53,
    SERVER_IDENTIFIER = 54,
    PARAMETER_REQUEST_LIST = 55,
    //MESSAGE = 56,
    //MAXIMUM_DHCP_MESSAGE_SIZE = 57,
    RENEWAL_T1_TIME_VALUE = 58,
    REBINDING_T2_TIME_VALUE = 59,
   
   // VENDOR_CLASS_IDENTIFIER = 60,
    //CLIENT_IDENTIFIER = 61

};

typedef struct dhcp_option
{
     uint8_t id;                       
     uint8_t len;
     uint8_t data[128];    
};

typedef struct dhcp_option dhcp_option;


void parse_ip(const char*ip_txt,uint32_t *ip_bin);
void convert_ip(uint32_t ip_bin, char*ip_txt);
void parse_long(const char*s,time_t*p);
void parse_mac(const char*mac,uint8_t*mac_bin);

void convert_mac(uint8_t *mac_bin,char*mac_text);

void initialize_options_list(queue*q);
int add_option_request(queue*q,uint8_t id,uint8_t len,const char*data);
int add_option_reply(queue*q,uint8_t id,uint8_t len, const char*data);

size_t serialize_option_list(queue*q,uint8_t *buf,size_t len);

const char *byte_to_char(uint8_t p);

#endif