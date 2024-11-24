#ifndef DHCP_CLIENT_H
#define DHCP_CLIENT_H

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

void check(int result, const char* message);
void convert_ip_to_bytes(uint32_t ip_address, unsigned char *data);
void print_dhcp_packet(const dhcp_packet *packet);

void create_discover(dhcp_packet **dhcp_packet_sent);
void create_request_dynamic(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received);
void create_request_static(dhcp_packet **dhcp_packet_sent, dhcp_packet *dhcp_packet_received, char *static_ip);
void create_request_renew(dhcp_packet **dhcp_packet_sent);
void create_release(dhcp_packet **dhcp_packet_sent);

void create_inform(dhcp_packet **dhcp_packet_sent, char *data_req, int len);

void send_packet(dhcp_packet *dhcp_packet_sent, int sockfd, struct sockaddr_in server_addr);
void receive_packet(dhcp_packet **dhcp_packet_received, int sockfd, struct sockaddr_in server_addr, socklen_t addr_len);

#endif