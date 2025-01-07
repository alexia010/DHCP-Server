#include "dhcp_server.h"

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

ip_cache ip_cache_instance;

#define CACHE_FILE "cache.txt" 




void initialize_server(server_configs *s, char *server_ip, const char *netmask, const char *gateway, char interface[20], const char *l_time, const char *p_time, const char *r_time, const char *rb_time,bool activeWhitelist)
{
    uint32_t ip,nm,gt;

    parse_ip(SERVER_IP,&ip);
    s->server_ip = ip;

    parse_ip(NETMASK,&nm);
    s->netmask=nm;

    parse_ip(GATEWAY,&gt);
    s->gateway=gt;

    strcpy(s->interface, interface);

    time_t l,pd,rn,rb;
    parse_long(l_time,&l);
    s->default_lease_time=l;

    parse_long(p_time,&pd);
    s->pendindg_time=pd;

    parse_long(r_time,&rn);
    s->renewal_time=rn;

    parse_long(rb_time,&rb);
    s->rebinding_time=rb;

    s->activeWhitelist=activeWhitelist;
    //open db
    int rc = sqlite3_open(DB_PATH, &db);

    if (rc) {
        log_msg(ERROR,"dhcp_server/initialize_server","Can't open database");
        exit(-1);
    } else {
        log_msg(INFO,"dhcp_server/initialize_server","Opened database successfully\n");
    }
    

}

int initialize_networks(network *n)
{
    char sql_query[200];
    sqlite3_stmt *stmt;
    strcpy(sql_query,(char*) "SELECT network_name, ip_range_start, ip_range_end, ip_current, subnet_mask, gateway, broadcast FROM network");


    int rc=sqlite3_prepare_v2(db,sql_query,-1,&stmt,NULL);

    if(rc!=SQLITE_OK)
    {
        log_msg(ERROR,"dhcp_server/initialize_networks","Failed to execute query"); 
        sqlite3_close(db);
        exit(-1);
    }

    int index=0;

    while((rc=sqlite3_step(stmt))==SQLITE_ROW)
    {
        if(index>=MAX_NETWORKS)
        {
            log_msg(WARNING,"dhcp_server/initialize_networks","Exceeded maximum number of ntworks.");
            break;
        }

        const char *network_name = (const char *)sqlite3_column_text(stmt, 0);
        const char *ip_range_start = (const char *)sqlite3_column_text(stmt, 1);
        const char *ip_range_end = (const char *)sqlite3_column_text(stmt, 2);
        const char*ip_current=(const char *)sqlite3_column_text(stmt, 3);
        const char* subnet_mask = (const char *)sqlite3_column_text(stmt, 4);
        const char* gateway = (const char *)sqlite3_column_text(stmt, 5);
        const char* broadcast = (const char *)sqlite3_column_text(stmt, 6);

        uint32_t s,e,c,mk,gt,br;
     
        parse_ip(ip_range_start,&s);
        parse_ip(ip_range_end,&e);
        parse_ip(ip_current,&c);
        parse_ip(subnet_mask,&mk);
        parse_ip(gateway,&gt);
        parse_ip(broadcast,&br);

        strcpy(n[index].netw_name,network_name);
        n[index].indexes.first=s;
        n[index].indexes.last=e;
        n[index].indexes.current=s;
        n[index].netmask=mk;
        n[index].gateway=gt;
        n[index].broadcast=br;

        n[index].bindings = (queue*)malloc(sizeof(queue));
        queue_init(n[index].bindings);
        index++;
    }


    sqlite3_finalize(stmt);


    for(int i=0;i<index;i++)
    {
        strcpy(sql_query,"SELECT d.dns_ip FROM network_dns AS nd JOIN dns_servers AS d ON nd.dns_id = d.id JOIN network AS n ON nd.network_id = n.id WHERE n.network_name = ?\0");

        rc=sqlite3_prepare_v2(db,sql_query, -1, &stmt, NULL);

        if (rc != SQLITE_OK) 
        {
            log_msg(ERROR, "dhcp_server/initialize_networks", "Failed to prepare DNS query:");
      
        }

        sqlite3_bind_text(stmt, 1,n[i].netw_name,-1,SQLITE_STATIC);

        int dns_count=0;

        while((rc=sqlite3_step(stmt))==SQLITE_ROW)
        {
            if(dns_count<MAX_DNS)
            {
                const char *dns_ip = (const char *)sqlite3_column_text(stmt, 0);

                uint32_t dns;
                parse_ip(dns_ip,&dns);
                n[i].dns_server[dns_count++]=dns;

            }

        }
        n[i].dns_count = dns_count;


        sqlite3_finalize(stmt);

    }
    
    return index;
}


address_binding * create_binding(uint32_t ip_address, uint8_t mac[16], int binding_status, bool static_status)
{
    address_binding *address = (address_binding*)malloc(sizeof(address_binding));

    address->ip_address = ip_address;
    address->cidt_len = ETHERNET_LEN;
    memcpy(address->cidt, mac, ETHERNET_LEN);
    address->binding_time = time(NULL);
    address->lease_time = address->binding_time + atoi(LEASE_TIME);
    address->status = binding_status;
    address->is_static = static_status;

    return address;
}

bool verify_address_binding(uint32_t adresa_ip, queue* bindings)
{
    // Check if the bindings queue is empty; if it is, the address is not bound.
    if (queue_is_empty(bindings))
        return false;

    // Traverse the linked list of bindings.
    node *current = bindings->head;
    while (current != NULL)
    {
        // Access the address binding data from the current node.
        address_binding *addr = (address_binding *)current->data;

        // Check if the IP address matches the one we're verifying.
        if (addr->ip_address == adresa_ip)
        {
            // If the binding is released or expired, the address is not bound.
            if (addr->status == RELEASED || addr->status == EXPIRED)
            {
                return false;
            }
            else
            {
                // Address is bound and active.
                return true;
            }
        }

        // Move to the next node in the queue.
        current = current->next;
    }

    // No matching binding was found; the address is not bound.
    return false;
}



int is_ip_in_cache(uint32_t ip_address) 
{
    ip_allocation* allocation = find_ip_allocation(&ip_cache_instance, ip_address);
    return allocation != NULL; // if ip is in cache return 1, else return 0
}

uint32_t find_free_ip(network* net, ip_cache* cache) 
{
    // Start with the first IP in the network's range.
    uint32_t current_ip = net->indexes.first;

    // Loop through all IPs in the range until the last one.
    while (current_ip <= net->indexes.last) 
    {
        // Lock the mutex to safely check the bindings queue.
        pthread_mutex_lock(&net->bindings->mutex);
        bool is_bound = verify_address_binding(current_ip, net->bindings);
        pthread_mutex_unlock(&net->bindings->mutex);

        // If the IP is not bound, check if it exists in the IP cache.
        if (!is_bound) {
            // Lock the mutex to safely check the cache.
            pthread_mutex_lock(&cache->mutex);
            bool in_cache = is_ip_in_cache(current_ip);
            pthread_mutex_unlock(&cache->mutex);

            // If the IP is neither bound nor in the cache, return it as free.
            if (!in_cache) {
                return current_ip;
            }
        }

        // Move to the next IP in the range.
        current_ip = htonl(ntohl(current_ip) + 1);
    }

    // No free IP was found in the range.
    return 0;
}

dhcp_packet* process_dhcp_discover(server_configs *server, dhcp_packet packet, network **net)
{
    // Allocate memory for the DHCP packet to be sent.
    dhcp_packet *packet_to_send = (dhcp_packet*)malloc(sizeof(dhcp_packet));

    // Check if whitelist functionality is active.
    if(server->activeWhitelist == 1)
    {
        // Verify if the client's MAC address is in the whitelist.
        bool exists = verify_whitelist(packet.header.chaddr);

        // If the MAC address is not in the whitelist, send a DHCP NAK response.
        if(exists == 0)
        {
            char data[2];
            data[0] = DHCP_NAK;
            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);

            // Log a warning with the MAC address.
            char wr_msg[200];
            char mac_str[18];
            convert_mac(packet.header.chaddr, mac_str);
            sprintf(wr_msg, "%s %s %s", "device with mac address", mac_str, "requested IP address");
            log_msg(WARNING, "verify_whitelist", wr_msg);
            return packet_to_send;
        }
    }

    // Lock the mutex for thread-safe access to the network's bindings queue.
    pthread_mutex_lock(&(*net)->bindings->mutex);

    // Check if there are available IPs in the current range.
    if((*net)->indexes.current <= (*net)->indexes.last)
    {
        uint32_t address_ip = (*net)->indexes.current;

        // If the current IP is uninitialized (0), log an error and exit.
        if(address_ip == 0)
        {
            log_msg(ERROR, "dhcp_server/process_dhcp_discover", "There are no available IPs!");
            exit(-1);
        }

        // Move to the next IP in the range.
        (*net)->indexes.current = htonl(ntohl((*net)->indexes.current) + 1);

        // Check if the IP is already in the cache.
        if (is_ip_in_cache(address_ip)) 
        {
            log_msg(ERROR, "dhcp_server/process_dhcp_discover", "The IP address is already in cache!");
            
            // Unlock and find a free IP.
            pthread_mutex_unlock(&(*net)->bindings->mutex);
            address_ip = find_free_ip(*net, &ip_cache_instance);
            
            // Re-lock the mutex for thread safety.
            pthread_mutex_lock(&(*net)->bindings->mutex);
        }

        // Ensure the IP is not already bound.
        while(verify_address_binding(address_ip, (*net)->bindings))
        {
            address_ip = (*net)->indexes.current;
            (*net)->indexes.current = htonl(ntohl((*net)->indexes.current) + 1);
        }

        // Send a DHCP OFFER response with the selected IP.
        char data[2];
        data[0] = DHCP_OFFER;
        packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, address_ip, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);

        // Create a binding for the IP and enqueue it in the bindings queue.
        address_binding *binding = create_binding(address_ip, packet.header.chaddr, PENDING, false);
        pthread_mutex_unlock(&(*net)->bindings->mutex); 
        queue_enqueue((*net)->bindings, binding);

        log_msg(INFO, "dhcp_server/process_dhcp_discover", "Binded an IP address for the client!");
    }
    else
    {
        // If no IPs are available in the range, find a free IP.
        uint32_t address_ip = find_free_ip(*net, &ip_cache_instance);

        if(address_ip == 0)
        {
            // Log an error and send a DHCP NAK if no IPs are available.
            log_msg(ERROR, "dhcp_server/process_dhcp_discover", "There are not any available addresses in the pool!");

            char data[2];
            data[0] = DHCP_NAK;
            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
        }
        else
        {
            // Send a DHCP OFFER response with the found IP.
            char data[2];
            data[0] = DHCP_OFFER;
            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, address_ip, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);

            // Create a binding for the IP and enqueue it in the bindings queue.
            address_binding *binding = create_binding(address_ip, packet.header.chaddr, PENDING, false);
            pthread_mutex_unlock(&(*net)->bindings->mutex);
            queue_enqueue((*net)->bindings, binding);
        }
    }

    // Unlock the mutex for the bindings queue.
    pthread_mutex_unlock(&(*net)->bindings->mutex);

    return packet_to_send;
}

bool renew_ip_address(server_configs *server, dhcp_packet packet, network **net, dhcp_option *options)
{
    // Log entry into the function for debugging.
    log_msg(INFO, "dhcp_server/renew_ip_address", "Entered function renew_ip_address");

    // Check if the bindings queue is empty. If it is, no IP address can be renewed.
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr;

    // Extract the IP address to be renewed from the DHCP options data.
    uint32_t adresa_ren = (options->data[1] << 24) | (options->data[2] << 16) | (options->data[3] << 8) | options->data[4];

    // Start iterating through the bindings queue.
    node *current = (*net)->bindings->head;

    // Protect access to the bindings queue with a mutex for thread safety.
    pthread_mutex_lock(&(*net)->bindings->mutex);

    while (current != NULL)
    {
        // Get the current binding from the queue.
        addr = (address_binding *)current->data;

        // Check if the current binding's IP address matches the requested IP address.
        if (addr->ip_address == adresa_ren)
        {
            int match = 1;

            // Compare the MAC address in the binding with the MAC address in the DHCP packet.
            for (int i = 0; i < 6; i++) {
                if (addr->cidt[i] != packet.header.chaddr[i]) {
                    match = 0; // If any byte does not match, set `match` to 0 and break.
                    break;
                }
            }

            // Log whether the MAC addresses match.
            if (match) {
                log_msg(INFO, "dhcp_server/renew_ip_address", "MAC addresses match!");
            } else {
                log_msg(INFO, "dhcp_server/renew_ip_address", "MAC addresses do not match!");
            }

            // If the MAC addresses match, proceed to renew the IP address.
            if (match)
            {
                // Check if the lease time has not expired yet.
                if (addr->lease_time > time(NULL))
                {
                    // Renew the IP allocation in the cache and update the database.
                    renew_ip_allocation(&ip_cache_instance, adresa_ren, time(NULL) + atoi(LEASE_TIME));
                    modify_lease_in_database(adresa_ren, time(NULL), time(NULL) + atoi(LEASE_TIME));

                    // Update the binding's lease and binding times.
                    addr->binding_time = time(NULL);
                    addr->lease_time = addr->binding_time + atoi(LEASE_TIME);

                    // Release the mutex as the operation is completed successfully.
                    pthread_mutex_unlock(&(*net)->bindings->mutex);

                    return true; // IP renewal successful.
                }
            }
        }

        // Move to the next node in the bindings queue.
        current = current->next;
    }

    // Release the mutex if the IP address was not found or renewed.
    pthread_mutex_unlock(&(*net)->bindings->mutex);

    return false; // IP renewal unsuccessful.
}

bool confirm_ip_address(server_configs *server, dhcp_packet packet, network **net)
{
    // Check if the bindings queue is empty. If it is, there are no IPs to confirm.
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr; // Pointer to store the current address binding.
    node *current = (*net)->bindings->head; // Start iterating from the head of the bindings queue.

    // Lock the mutex to ensure thread-safe access to the bindings queue.
    pthread_mutex_lock(&(*net)->bindings->mutex);

    while (current != NULL)
    {
        // Get the address binding stored in the current node.
        addr = (address_binding *)current->data;

        // Check if the IP address in the binding matches the client-requested IP address.
        if (addr->ip_address == packet.header.ciaddr)
        {
            // If the IP address has a status of PENDING, it is available for assignment.
            if (addr->status == PENDING)
            {
                log_msg(INFO, "dhcp_server/confirm_ip_address", "Requested IP address has status PENDING! It's OK :)");
                
                // Change the status of the binding to ASSOCIATED.
                addr->status = ASSOCIATED;

                // Add the IP allocation to the cache for tracking.
                add_ip_allocation(&ip_cache_instance, addr->ip_address, addr->cidt, addr->binding_time, addr->lease_time, addr->is_static);

                // Insert the IP, MAC, and lease details into the database for persistence.
                insert_ip_mac_lease((*net)->netw_name, addr->ip_address, addr->cidt, addr->binding_time, addr->lease_time, addr->is_static);

                // Unlock the mutex before returning success.
                pthread_mutex_unlock(&(*net)->bindings->mutex);
                return true;
            }
            else if (addr->status == ASSOCIATED)
            {
                // Log an error if the requested IP is already assigned to another client.
                log_msg(ERROR, "dhcp_server/confirm_ip_address", "Requested IP address has status ASSOCIATED! It is already assigned to another client.");

                // Unlock the mutex before returning failure.
                pthread_mutex_unlock(&(*net)->bindings->mutex);
                return false;
            }
        }

        // Move to the next binding in the queue.
        current = current->next;
    }

    // Unlock the mutex if no matching IP address is found in the bindings.
    pthread_mutex_unlock(&(*net)->bindings->mutex);

    // Return false if the requested IP address could not be confirmed.
    return false;
}

bool confirm_static_ip(server_configs *server, dhcp_packet packet, network **net, uint32_t ip_address)
{
    // Check if the bindings queue is not empty, meaning there are existing bindings to examine.
    if (!queue_is_empty((*net)->bindings))
    {
        address_binding *addr; // Pointer to hold the current address binding.
        node *current = (*net)->bindings->head; // Start iterating from the head of the bindings queue.

        // Lock the mutex to ensure thread-safe access to the bindings queue.
        pthread_mutex_lock(&(*net)->bindings->mutex);

        while (current != NULL)
        {
            addr = (address_binding *)current->data;

            // Check if the IP address in the binding matches the given static IP address.
            if (addr->ip_address == ip_address)
            {
                // If the status is PENDING, it is available for assignment.
                if (addr->status == PENDING)
                {
                    log_msg(INFO, "dhcp_server/confirm_static_ip", "Requested IP address has status PENDING! It's OK :)");
                    addr->status = ASSOCIATED;

                    // Add the IP allocation to the cache for tracking with updated lease time.
                    add_ip_allocation(&ip_cache_instance, addr->ip_address, addr->cidt, addr->binding_time, addr->lease_time + time(NULL), addr->is_static);

                    // Insert the IP, MAC, and lease details into the database for persistence.
                    insert_ip_mac_lease((*net)->netw_name, addr->ip_address, addr->cidt, addr->binding_time, addr->lease_time + time(NULL), addr->is_static);

                    // Unlock the mutex and return success.
                    pthread_mutex_unlock(&(*net)->bindings->mutex);
                    return true;
                }
                // If the status is ASSOCIATED, the IP is already assigned to another client.
                else if (addr->status == ASSOCIATED)
                {
                    log_msg(ERROR, "dhcp_server/confirm_static_ip", "Requested IP address has status ASSOCIATED! It is already assigned to another client.");

                    // Unlock the mutex and return failure.
                    pthread_mutex_unlock(&(*net)->bindings->mutex);
                    return false;
                }
            }

            // Move to the next binding in the queue.
            current = current->next;
        }

        // Unlock the mutex if no matching IP address is found.
        pthread_mutex_unlock(&(*net)->bindings->mutex);
    }

    // Check if the given static IP is within the valid range of the network.
    if (ip_address < (*net)->indexes.last && ip_address > (*net)->indexes.first)
    {
        // Create a new binding for the static IP address.
        address_binding *binding = create_binding(ip_address, packet.header.chaddr, ASSOCIATED, true);

        // Lock the mutex to ensure thread-safe addition of the new binding.
        pthread_mutex_lock(&(*net)->bindings->mutex);
        queue_enqueue((*net)->bindings, binding); // Add the new binding to the queue.

        // Add the IP allocation to the cache for tracking.
        add_ip_allocation(&ip_cache_instance, ip_address, packet.header.chaddr, time(NULL), time(NULL) + atoi(LEASE_TIME), true);

        // Insert the new static binding into the database for persistence.
        insert_ip_mac_lease((*net)->netw_name, ip_address, packet.header.chaddr, time(NULL), atoi(LEASE_TIME) + time(NULL), true);

        // Unlock the mutex and log the success message.
        pthread_mutex_unlock(&(*net)->bindings->mutex);
        log_msg(INFO, "dhcp_server/confirm_static_ip", "IP requested is in the network. Operation successful!");
        return true;
    }

    // If the IP is not in the valid range, log an error message and return failure.
    log_msg(ERROR, "dhcp_server/confirm_static_ip", "IP requested is not in the valid network range. Operation failed.");
    return false;
}

bool decline_ip(server_configs *server, dhcp_packet packet, network **net)
{
    // Check if the bindings queue is empty. If it is, there are no IP bindings to process.
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr; // Pointer to hold the current address binding.
    node *current = (*net)->bindings->head; // Start iterating from the head of the bindings queue.

    // Lock the mutex to ensure thread-safe access to the bindings queue.
    pthread_mutex_lock(&(*net)->bindings->mutex);

    while (current != NULL)
    {
        addr = (address_binding *)current->data;

        // Check if the IP address in the binding matches the IP address in the packet.
        if (addr->ip_address == packet.header.ciaddr)
        {
            // Free the memory allocated for the address binding.
            free(addr);

            // If the node to be removed is the head of the queue.
            if (current == (*net)->bindings->head)
            {
                // Update the head of the queue to the next node.
                (*net)->bindings->head = current->next;

                // If the head becomes NULL, the queue is now empty; update the tail as well.
                if ((*net)->bindings->head == NULL)
                    (*net)->bindings->tail = NULL;
            }
            else
            {
                // Find the previous node to the current node.
                node *prev = (*net)->bindings->head;
                while (prev->next != current)
                    prev = prev->next;

                // Update the previous node to skip the current node.
                prev->next = current->next;

                // If the node to be removed is the tail, update the tail to the previous node.
                if (current == (*net)->bindings->tail)
                    (*net)->bindings->tail = prev;
            }

            // Free the memory allocated for the current node.
            free(current);

            // Unlock the mutex and return true indicating the IP address was declined successfully.
            pthread_mutex_unlock(&(*net)->bindings->mutex);
            return true;
        }

        // Move to the next node in the queue.
        current = current->next;
    }

    // Unlock the mutex if no matching IP address was found.
    pthread_mutex_unlock(&(*net)->bindings->mutex);

    // Return false if the IP address was not found in the bindings.
    return false;
}

bool release_ip_address(server_configs *server, dhcp_packet packet, network **net)
{
    // Check if the bindings queue is empty. If it's empty, there are no IP bindings to release.
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr; // Pointer to hold the current address binding.

    // Lock the mutex to ensure thread-safe access to the bindings queue.
    pthread_mutex_lock(&(*net)->bindings->mutex);

    node *current = (*net)->bindings->head; // Start iterating from the head of the bindings queue.

    while (current != NULL)
    {
        addr = (address_binding *)current->data;

        // Check if the IP address in the binding matches the IP address in the packet.
        if (addr->ip_address == packet.header.ciaddr)
        {
            // Remove the IP allocation from the IP cache.
            remove_ip_allocation(&ip_cache_instance, packet.header.ciaddr);

            // Delete the IP entry from the database.
            delete_ip_in_database(addr->ip_address);

            // Update the binding's status and reset its timing attributes.
            addr->status = RELEASED;   // Mark the binding as released.
            addr->binding_time = 0;    // Reset the binding time.
            addr->lease_time = 0;      // Reset the lease time.

            // Unlock the mutex as the operation is complete.
            pthread_mutex_unlock(&(*net)->bindings->mutex);
            return true; // Return true indicating the IP address was successfully released.
        }

        // Move to the next node in the queue.
        current = current->next;
    }

    // Unlock the mutex if no matching IP address was found.
    pthread_mutex_unlock(&(*net)->bindings->mutex);

    // Return false if the IP address was not found in the bindings.
    return false;
}

void convert_ip_to_bytes(uint32_t ip_address, unsigned char *data)
{
    data[0] = (ip_address >> 24) & 0xFF;
    data[1] = (ip_address >> 16) & 0xFF;
    data[2] = (ip_address >> 8) & 0xFF;
    data[3] = ip_address & 0xFF;
}

void insert_ip_mac_lease(char *netw_name, uint32_t ip, uint8_t mac[16], time_t lease_time_start, time_t lease_time_end, bool is_static)
{
    int network_id = get_network_id_by_name(netw_name);

    sqlite3 *db;
    char *err_msg = 0;
    char sql[1024], lease_start_str[20], lease_end_str[20], mac_str[25], ip_str[20];

    // Open connection with database
    int rc = sqlite3_open(DB_PATH, &db);
    if (rc) {
        log_msg(ERROR, "dhcp_server/insert_ip_mac_lease", "Can't open database for adding client detailes!");
        exit(-1);
    }

    time_to_sql_format(lease_time_start, lease_start_str);
    time_to_sql_format(lease_time_end, lease_end_str);
    convert_mac(mac, mac_str);
    sprintf(ip_str, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);

    // command for insert
    snprintf(sql, 1024,
            "INSERT INTO ip_mac_leases (network_id, ip_address, mac_address, lease, lease_end, is_static) "
            "VALUES (%d, '%s', '%s', '%s', '%s', %d);",
            network_id, 
            ip_str, 
            mac_str,
            lease_start_str,
            lease_end_str, 
            is_static ? 1 : 0);

    // execute sql cmd
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        log_msg(ERROR, "dhcp_server/insert_ip_mac_lease", "SQL error!");
        sqlite3_free(err_msg);
        sqlite3_close(db);
    }

    // close connection to db
    sqlite3_close(db);
    log_msg(INFO, "dhcp_server/insert_ip_mac_lease", "Insert in ip_mac_lease executed successfully!");
}

int get_network_id_by_name(const char *network_name)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char *err_msg = 0;
    int rc;
    int network_id = -1;  // implicit value if network not found

    // Open connection to the database
    rc = sqlite3_open(DB_PATH, &db);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/get_network_id_by_name", "Can't open database!");
        exit(-1);
    }

    // Prepare the SQL query
    const char *sql = "SELECT id FROM network WHERE network_name = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        log_msg(ERROR, "dhcp_server/get_network_id_by_name", "Failed to prepare statement!");
        sqlite3_close(db);
        exit(-1);
    }

    // Bind the network_name parameter to the query
    sqlite3_bind_text(stmt, 1, network_name, -1, SQLITE_STATIC);

    // Execute the query and get the result
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        network_id = sqlite3_column_int(stmt, 0);  // Extract the network ID
    } else if (rc == SQLITE_DONE) 
        log_msg(ERROR, "dhcp_server/get_network_id_by_name", "No network fount with the specified name!");
    else 
        log_msg(ERROR, "dhcp_server/get_network_id_by_name", "Error while executing the statement!");


    // Clean up
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return network_id;
}

void time_to_sql_format(time_t timestamp, char *time_str)
{
    struct tm *tm_info;
    tm_info = localtime(&timestamp);
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", tm_info);
}

void modify_lease_in_database(uint32_t ip, time_t lease_start, time_t lease_end)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE ip_mac_leases SET lease = ?, lease_end = ? WHERE ip_address = ?;";
    char ip_str[20];
    sprintf(ip_str, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    int rc;
    char lease_str[20], lease_end_str[20];
    time_to_sql_format(lease_start, lease_str);
    time_to_sql_format(lease_end, lease_end_str);
    
    rc = sqlite3_open(DB_PATH, &db);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Can't open database!");
        exit(-1);
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Failed to prepare the statement!");
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_bind_text(stmt, 1, lease_str, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Failed to bind lease_str parameter!");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_bind_text(stmt, 2, lease_end_str, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Failed to bind lease_end_str parameter!");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_bind_text(stmt, 3, ip_str, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Failed to bind ip_str parameter!");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_msg(ERROR, "dhcp_server/modify_lease_in_database", "Execution failed!");

    } else {
        log_msg(INFO, "dhcp_server/modify_lease_in_database", "Query executed successfully!");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void delete_ip_in_database(uint32_t ip)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM ip_mac_leases WHERE ip_address = ?;";
    char ip_str[20];
    sprintf(ip_str, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    int rc;
    
    rc = sqlite3_open(DB_PATH, &db);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/delete_ip_in_database", "Cannot open database!");
        exit(-1);
    }

     rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/delete_ip_in_database", "Failed to prepare statement!");
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_bind_text(stmt, 1, ip_str, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        log_msg(ERROR, "dhcp_server/delete_ip_in_database", "Failed to bind ip_str parameter!");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        log_msg(ERROR, "dhcp_server/delete_ip_in_database", "Execution failed!");
        } else {
        log_msg(INFO, "dhcp_server/delete_ip_in_database", "Lease updated successfully!");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

bool verify_whitelist(uint8_t chaddr[16])
{
    char mac_str[18];
    convert_mac(chaddr,mac_str);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM mac_whitelist WHERE mac_address = ?";

    // create statement
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        char error_msg[200];
        sprintf(error_msg,"%s : %s","Error preparing statement",sqlite3_errmsg(db));
        log_msg(ERROR,"verify_whitelist", error_msg);
        return false;
    }

    // binding parameter
    if (sqlite3_bind_text(stmt, 1, mac_str, -1, SQLITE_STATIC) != SQLITE_OK) {
        char error_msg[200];
        sprintf(error_msg,"%s : %s","Error binding value",sqlite3_errmsg(db));
        log_msg(ERROR,"verify_whitelist", error_msg);
        sqlite3_finalize(stmt);
        return false;
    }

    // execute
    int result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
      
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        
        // Dacă COUNT(*) > 0, adresa MAC se află în whitelist
        return count;
    } else {
        char error_msg[200];
        sprintf(error_msg,"%s : %s","Error executing statement",sqlite3_errmsg(db));
        log_msg(ERROR,"verify_whitelist", error_msg);
        sqlite3_finalize(stmt);

        return false;
    }
}


// new 
// Function to initialize the IP cache
void init_ip_cache(ip_cache *cache) 
{
    // Set the head of the cache list to NULL (empty cache)
    cache->head = NULL;

    // Initialize the mutex to ensure thread safety when accessing the cache
    pthread_mutex_init(&cache->mutex, NULL);
}

// Function to add an IP allocation to the cache
void add_ip_allocation(ip_cache *cache, uint32_t ip_address, uint8_t *mac_address, time_t lease_start, time_t lease_end, int is_static) 
{
    // Allocate memory for a new IP allocation record
    ip_allocation *new_allocation = malloc(sizeof(ip_allocation));
    
    // If memory allocation fails, print an error and return
    if (!new_allocation) {
        perror("Failed to allocate memory for IP allocation");
        return;
    }

    // Assign values to the fields of the new allocation
    new_allocation->ip_address = ip_address; // Set IP address
    memcpy(new_allocation->mac_address, mac_address, 6); // Copy MAC address
    new_allocation->lease_start = lease_start; // Set lease start time
    new_allocation->lease_end = lease_end; // Set lease end time
    new_allocation->is_static = is_static; // Set whether the lease is static or dynamic
    new_allocation->next = NULL; // Set the next pointer to NULL (end of list)

    // Lock the mutex to ensure thread safety while modifying the cache
    pthread_mutex_lock(&cache->mutex);

    // Add the new allocation to the front of the list (linked list)
    new_allocation->next = cache->head; 
    cache->head = new_allocation; // Set the head to the new allocation

    // Unlock the mutex after modifying the cache
    pthread_mutex_unlock(&cache->mutex);

    // Save the updated cache to a file for persistence
    save_cache_to_file(cache, CACHE_FILE);
}

void save_cache_to_file(ip_cache *cache, const char *filename) 
{
    pthread_mutex_lock(&cache->mutex);

    FILE *file = fopen(filename, "w");
    if (!file) {
        log_msg(ERROR, "dhcp_server/save_cache_to_file", "Error at opening file for saving cache");
        pthread_mutex_unlock(&cache->mutex);
        return;
    }

    ip_allocation *current = cache->head;
    while (current) 
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &current->ip_address, ip_str, INET_ADDRSTRLEN);

        char lease_start_str[20], lease_end_str[20];

        time_to_sql_format(current->lease_start, lease_start_str);
        time_to_sql_format(current->lease_end, lease_end_str);

        fprintf(file, "%s,%02x:%02x:%02x:%02x:%02x:%02x,%s,%s,%d\n",
                ip_str,
                current->mac_address[0], current->mac_address[1], current->mac_address[2],
                current->mac_address[3], current->mac_address[4], current->mac_address[5],
                lease_start_str, lease_end_str, current->is_static);

        current = current->next;
    }

    fclose(file);
    pthread_mutex_unlock(&cache->mutex);
}

void renew_ip_allocation(ip_cache *cache, uint32_t ip_address, time_t new_lease_end) 
{
    log_msg(INFO, "dhcp_server/renew_ip_allocation", "Before cache mutex lock");
    pthread_mutex_lock(&cache->mutex);
    log_msg(INFO, "dhcp_server/renew_ip_allocation", "After cache mutex lock");

    ip_allocation *current = cache->head;
    while (current) {
        if (current->ip_address == ip_address) 
        {
            current->lease_end = new_lease_end;
            break;
        }
        current = current->next;
    }

    log_msg(INFO, "dhcp_server/renew_ip_allocation", "Before cache mutex unlock");
    pthread_mutex_unlock(&cache->mutex);
    log_msg(INFO, "dhcp_server/renew_ip_allocation", "After cache mutex unlock");

    save_cache_to_file(cache, CACHE_FILE);
}

void remove_ip_allocation(ip_cache *cache, uint32_t ip_address) 
{
    pthread_mutex_lock(&cache->mutex);

    ip_allocation *current = cache->head;
    ip_allocation *prev = NULL;

    while (current) {
        if (current->ip_address == ip_address) {
            if (prev) {
                prev->next = current->next;
            } else {
                cache->head = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&cache->mutex);

    save_cache_to_file(cache, CACHE_FILE);
}

void load_cache_from_file(ip_cache *cache, const char *filename) 
{
    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (!file) 
    {
        // Log error if file fails to open
        log_msg(ERROR, "dhcp_server/load_cache_from_file", "Failed to open file");
        return;
    }

    char line[256];
    // Read each line from the file
    while (fgets(line, sizeof(line), file)) {
        
        // Remove the newline character at the end of the line
        line[strcspn(line, "\n")] = 0;  

        uint32_t ip_address;
        uint8_t mac_address[6];
        time_t lease_start, lease_end;
        int is_static;

        // Tokenize the line using comma as the delimiter
        char *token = strtok(line, ",");
        
        // Parse IP address from the first token
        if (token != NULL) {
            if (inet_pton(AF_INET, token, &ip_address) != 1) {
                // If IP conversion fails, log and skip the line
                printf("Failed to convert IP address: %s\n", token);
                continue;  
            }
        }

        // Parse MAC address from the second token
        token = strtok(NULL, ",");
        if (token != NULL) {
            if (sscanf(token, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac_address[0], &mac_address[1], &mac_address[2],
                   &mac_address[3], &mac_address[4], &mac_address[5]) != 6) {
                // If MAC address parsing fails, log and skip the line
                printf("Failed to parse MAC address: %s\n", token);
                continue;  
            }
        }

        // Parse lease start date from the third token
        token = strtok(NULL, ",");
        char lease_start_str[20];
        if (token != NULL) {
            strncpy(lease_start_str, token, sizeof(lease_start_str) - 1);
            lease_start_str[sizeof(lease_start_str) - 1] = '\0';
        }

        // Parse lease end date from the fourth token
        token = strtok(NULL, ",");
        char lease_end_str[20];
        if (token != NULL) {
            strncpy(lease_end_str, token, sizeof(lease_end_str) - 1);
            lease_end_str[sizeof(lease_end_str) - 1] = '\0';
        }

        // Parse static lease flag from the fifth token
        token = strtok(NULL, ",");
        if (token != NULL) {
            is_static = atoi(token); // Convert string to integer
        }

        // Convert lease start and end dates to time_t using strptime and mktime
        struct tm tm_start = {0};
        struct tm tm_end = {0};

        if (strptime(lease_start_str, "%Y-%m-%d %H:%M:%S", &tm_start) == NULL) {
            // If lease start date parsing fails, log and skip the line
            printf("Failed to parse lease start time: %s\n", lease_start_str);
            continue;
        }

        if (strptime(lease_end_str, "%Y-%m-%d %H:%M:%S", &tm_end) == NULL) {
            // If lease end date parsing fails, log and skip the line
            printf("Failed to parse lease end time: %s\n", lease_end_str);
            continue;
        }

        // Convert the parsed time structures to time_t values
        lease_start = mktime(&tm_start);
        lease_end = mktime(&tm_end);

        // Add the parsed allocation to the cache
        add_ip_allocation(cache, ip_address, mac_address, lease_start, lease_end, is_static);
    }

    // Close the file after processing
    fclose(file);
}

ip_allocation *find_ip_allocation(ip_cache *cache, uint32_t ip_address) 
{
   
    ip_allocation *current = cache->head;
    while (current) {
        if (current->ip_address == ip_address) {
            pthread_mutex_unlock(&cache->mutex);
            return current;
        }
        current = current->next;
    }
    return NULL;
}