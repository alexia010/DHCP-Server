#ifndef ADDR_MNG_H
#define ADDRMNG_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#include "queue.h"

#define MAX_DNS 4

enum binding_type
{
    DYNAMIC=0,
    STATIC=1
};

enum binding_status
{
    EMPTY = 0,
    ASSOCIATED,
    PENDING,
    EXPIRED,
    RELEASED
};


struct pool_indexes
{
    uint32_t first;    // first address of the pool
    uint32_t last;     // last address of the pool
    uint32_t current;  // current available address
};

typedef struct pool_indexes pool_indexes;

struct address_binding
{
    uint32_t ip_address;
    uint8_t cidt_len;
    uint8_t cidt[256];

    time_t binding_time;
    time_t lease_time;

    int status;         // binding_status
    bool is_static;

    /// ?? LIST_ENTRY(address_binding) pointers; // list pointers, see queue(3)
};

typedef struct address_binding address_binding;

struct network
{
    char netw_name[14];

    pool_indexes indexes;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t broadcast;

    uint32_t dns_server[MAX_DNS];
   

    //?? dhcp_options_list  optinos??
    queue *bindings;

};

typedef struct network network;



#endif