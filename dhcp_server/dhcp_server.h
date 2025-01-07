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

#define DB_PATH "/home/daria/Desktop/proiect_pso_numodific/DHCP-Server/dhcp_server/config2.db"


#define MAX_NETWORKS 5

#include <sqlite3.h>


//new

typedef struct ip_allocation {
    uint32_t ip_address;
    uint8_t mac_address[6];
    time_t lease_start;
    time_t lease_end;
    int is_static; // 1 dacă este static, 0 dacă este dinamic
    struct ip_allocation *next;
} ip_allocation;


typedef struct {
    ip_allocation *head;
    pthread_mutex_t mutex;
} ip_cache;


extern ip_cache ip_cache_instance;



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

    bool activeWhitelist;
};

typedef struct server_configs server_configs;


void initialize_server(server_configs *s, char *server_ip, const char *netmask,
                        const char *gateway, char interface[20], const char *l_time, 
                        const char *p_time, const char* r_time, const char* rb_time,bool activeWhitelist);

//void exec_quey(char* query, char*callback,void **);

int initialize_networks(network *n);

address_binding *create_binding(uint32_t ip_address, uint8_t mac[16], int binding_status, bool static_status);
bool verify_address_binding(uint32_t adresa_ip, queue* bindings);
uint32_t find_free_ip(network *net, ip_cache* cache);
dhcp_packet* process_dhcp_discover(server_configs *server, dhcp_packet packet, network **net);
bool renew_ip_address(server_configs *server, dhcp_packet packet, network **net, dhcp_option *options);
bool confirm_ip_address(server_configs *server, dhcp_packet packet, network **net);
bool confirm_static_ip(server_configs *server, dhcp_packet packet, network **net, uint32_t ip_address);
bool decline_ip(server_configs *server, dhcp_packet packet, network **net);
bool release_ip_address(server_configs *server, dhcp_packet packet, network **net);

void convert_ip_to_bytes(uint32_t ip_address, unsigned char *data);

void insert_ip_mac_lease(char *netw_name, uint32_t ip, uint8_t mac[16], time_t lease_time_start, time_t lease_time_end, bool is_static);
int get_network_id_by_name(const char *network_name);
void time_to_sql_format(time_t timestamp, char *time_str);
void modify_lease_in_database(uint32_t ip, time_t lease_start, time_t lease_end);
void delete_ip_in_database(uint32_t ip);

bool verify_whitelist(uint8_t chaddr[16]);

//new
void init_ip_cache(ip_cache *cache);
void add_ip_allocation(ip_cache *cache, uint32_t ip_address, uint8_t *mac_address, time_t lease_start, time_t lease_end, int is_static);
void save_cache_to_file(ip_cache *cache, const char *filename);
void renew_ip_allocation(ip_cache *cache, uint32_t ip_address, time_t new_lease_end);
void load_cache_from_file(ip_cache *cache, const char *filename);
int is_ip_in_cache(uint32_t ip_address);
ip_allocation *find_ip_allocation(ip_cache *cache, uint32_t ip_address);
void remove_ip_allocation(ip_cache *cache, uint32_t ip_address);

#endif