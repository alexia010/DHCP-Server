#ifndef DHCP_SERVER_h
#define DHCP_SERVER_H

#include "dhcp_packet.h"
#include "address_management.h"


#define SERVER_IP "172.20.10.3"
#define NETMASK "255.255.255.240"
#define GATEWAY "172.20.10.1"

#define LEASE_TIME "3600"
#define PENDING_TIME "30"
#define RENEWAL_TIME  "1800"
#define REBINDING_TIME "3000"

#define DB_PATH "/home/daria/Desktop/DHCP-Server/dhcp_server/config2.db"


#define MAX_NETWORKS 5

#include <sqlite3.h>



static sqlite3 *db=NULL;

struct server_configs
{
    uint32_t server_ip;
    uint32_t netmask;
    uint32_t gateway;

    char interface[20]; 

    time_t default_lease_time;
    time_t pendindg_time;
    time_t renewal_time;
    time_t rebinding_time;

    network networks[MAX_NETWORKS];
    int networks_nr;
};

typedef struct server_configs server_configs;


void initialize_server(server_configs *s, char *server_ip, const char *netmask,
                        const char *gateway, char interface[20], const char *l_time, 
                        const char *p_time, const char* r_time, const char* rb_time);

//void exec_quey(char* query, char*callback,void **);

int initialize_networks(network *n);

address_binding *create_binding(uint32_t ip_address, uint8_t mac[16], int binding_status, bool static_status);
bool verify_address_binding(uint32_t adresa_ip, queue* bindings);
uint32_t find_free_ip(network *net);
dhcp_packet* process_dhcp_discover(server_configs *server, dhcp_packet packet, network **net);
bool renew_ip_address(server_configs *server, dhcp_packet packet, network **net, dhcp_option *options);
bool confirm_ip_address(server_configs *server, dhcp_packet packet, network **net);
bool confirm_static_ip(server_configs *server, dhcp_packet packet, network **net, uint32_t ip_address);
bool decline_ip(server_configs *server, dhcp_packet packet, network **net);
bool release_ip_address(server_configs *server, dhcp_packet packet, network **net);

#endif