#include "dhcp_server.h"

void initialize_server(server_configs *s, char *server_ip, const char *netmask, const char *gateway, char interface[20], const char *l_time, const char *p_time, const char *r_time, const char *rb_time)
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

    //open db
    int rc = sqlite3_open(DB_PATH, &db);

    if (rc) {
        log_msg(ERROR,"initialize_server","Can't open database");
        exit(-1);
    } else {
        log_msg(INFO,"initialize_server","Opened database successfully\n");
    }
    

}

// void exec_quey(char *query, char *cb, void **data)
// {
//     char *err_msg=0;  
//     int rc=sqlite3_exec(db,query,cb,&data,err_msg);   
//     if(rc!=SQLITE_OK)
//     {
//         log_msg(ERROR,"exec_query",err_msg);
//         exit(-1);
//     }
// }
// static int ntw_callback(void*data,int row_count, char**row_values,char*column_names)
// {
//     network *result=(network*)data;
//     if(row_count>=MAX_NETWORKS)
//     {
//         log_msg(WARNING,"ntw_callback","Too many networks");
//         row_count=MAX_NETWORKS;
//     }
//     static int index=0;
//     strncpy(result[index].netw_name,row_values[0])
// }

int initialize_networks(network *n)
{
    char sql_query[200];
    sqlite3_stmt *stmt;
    strcpy(sql_query,(char*) "SELECT network_name, ip_range_start, ip_range_end, ip_current, subnet_mask, gateway, broadcast FROM network");


    int rc=sqlite3_prepare_v2(db,sql_query,-1,&stmt,NULL);

    if(rc!=SQLITE_OK)
    {
        log_msg(ERROR,"initialize_networks","Failed to execute query"); 
        sqlite3_close(db);
        exit(-1);
    }

    int index=0;

    while((rc=sqlite3_step(stmt))==SQLITE_ROW)
    {
        if(index>=MAX_NETWORKS)
        {
            log_msg(WARNING,"initialize_networks","Exceeded maximum number of ntworks.");
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
            log_msg(ERROR, "initialize_networks", "Failed to prepare DNS query:");
      
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
    if (queue_is_empty(bindings))
        return false;

    node *current = bindings->head;
    while (current != NULL)
    {
        address_binding *addr = (address_binding *)current->data;
        if (addr->ip_address == adresa_ip)
        {
            if (addr->status == RELEASED || addr->status == EXPIRED)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        current = current->next;
    }

    return false;
}

uint32_t find_free_ip(network *net)
{
    uint32_t current_ip = net->indexes.first;

    while(current_ip <= net->indexes.last)
        if(verify_address_binding(current_ip, net->bindings))
            return current_ip;
        current_ip++;

    return 0;
}

dhcp_packet* process_dhcp_discover(server_configs *server, dhcp_packet packet, network **net)
{
    dhcp_packet *packet_to_send = (dhcp_packet*)malloc(sizeof(dhcp_packet));

    if((*net)->indexes.current <= (*net)->indexes.last)
    {
        uint32_t address_ip =  (*net)->indexes.current;
        (*net)->indexes.current += 1;
        while(verify_address_binding(address_ip, (*net)->bindings))
        {
            address_ip =  (*net)->indexes.current;
            (*net)->indexes.current += 1; //????????????????????????????????????cond oprire
        }

        //trimit dhcp offer
        char data[2];
        data[0] = DHCP_OFFER;
        packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, address_ip, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
        address_binding *binding = create_binding(address_ip, packet.header.chaddr, PENDING, false);
        //queue_init(net->bindings);
        printf("%x\n", binding->ip_address);
        queue_enqueue((*net)->bindings, binding);
    }
    else
    {
        uint32_t address_ip = find_free_ip(net);
        if(address_ip == 0)
        {
            log_msg(ERROR, "server", "There are not any available addresses in the pool!\n");

            //trimit packet nak
            char data[2];
            data[0] = DHCP_NAK;
            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, packet.header.yiaddr, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
        }
        else{
            char data[2];
            data[0] = DHCP_OFFER;
            packet_to_send = create_dhcp_packet_server(BOOTREPLY, ETHERNET, ETHERNET_LEN, packet.header.xid, packet.header.secs, packet.header.flags, packet.header.ciaddr, address_ip, server->server_ip, server->gateway, packet.header.chaddr, DHCP_MESSAGE_TYPE, 1, data);
            address_binding *binding = create_binding(address_ip, packet.header.chaddr, PENDING, false);
            queue_enqueue((*net)->bindings, binding);
        }
    }

    return packet_to_send;
}


bool renew_ip_address(server_configs *server, dhcp_packet packet, network **net, dhcp_option *options)
{
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr;
    uint32_t adresa_ren = (options->data[1] << 24) | (options->data[2] << 16) | (options->data[3] << 8) | options->data[4];

    node *current = (*net)->bindings->head;
    while (current != NULL)
    {
        addr = (address_binding *)current->data;
        if (addr->ip_address == adresa_ren)
        {
            if (memcmp(addr->cidt, packet.header.chaddr, sizeof(packet.header.chaddr)) == 0)
            {
                if (addr->lease_time <= time(NULL))
                {
                    addr->binding_time = time(NULL);
                    addr->lease_time = addr->binding_time + LEASE_TIME;
                    return true;
                }
            }
        }

        current = current->next;
    }

    return false;
}

bool confirm_ip_address(server_configs *server, dhcp_packet packet, network **net)
{
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr;
    node *current = (*net)->bindings->head;

    while (current != NULL)
    {
        addr = (address_binding *)current->data;
        if (addr->ip_address == packet.header.ciaddr)
        {
            if (addr->status == PENDING)
            {
                log_msg(INFO, "dhcp_server", "Requested IP address has status PENDING! It's OK :)");
                addr->status = ASSOCIATED;
                return true;
            }
            else if(addr->status == ASSOCIATED)
            {
                log_msg(ERROR, "dhcp_server", "Requested IP address has status ASSOCIATED! It is already assigned to another client.");
                return false;
            }
        }
        current = current->next;
    }

    return false;
}

bool confirm_static_ip(server_configs *server, dhcp_packet packet, network **net, uint32_t ip_address)
{
    if (!queue_is_empty((*net)->bindings))
    {
        address_binding *addr;
        node *current = (*net)->bindings->head;

        while (current != NULL)
        {
            addr = (address_binding *)current->data;
            if (addr->ip_address == ip_address)
            {
                if (addr->status == PENDING)
                {
                    log_msg(INFO, "dhcp_server", "Requested IP address has status PENDING! It's OK :)");
                    addr->status = ASSOCIATED;
                    return true;
                }
                else if(addr->status == ASSOCIATED)
                {
                    log_msg(ERROR, "dhcp_server", "Requested IP address has status ASSOCIATED! It is already assigned to another client.");
                    return false;
                }
            }
            current = current->next;
        }
    }

    if(ip_address < (*net)->indexes.last && ip_address > (*net)->indexes.first)
    {
        log_msg(INFO, "dhcp_server", "IP requested is in the network. Operation successful!");
        return true;
    }

    log_msg(ERROR, "dhcp_server", "IP requested is not in the valid network range. Operation failed.");
    return false;
}

bool decline_ip(server_configs *server, dhcp_packet packet, network **net)
{
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr;
    node *current = (*net)->bindings->head;

    while (current != NULL)
    {
        addr = (address_binding *)current->data;
        if (addr->ip_address == packet.header.ciaddr)
        {
            free(addr);
            if (current == (*net)->bindings->head)
            {
                (*net)->bindings->head = current->next;
                if ((*net)->bindings->head == NULL)
                    (*net)->bindings->tail = NULL;
            }
            else
            {
                node *prev = (*net)->bindings->head;
                while (prev->next != current)
                    prev = prev->next;
                
                prev->next = current->next;
                if (current == (*net)->bindings->tail)
                    (*net)->bindings->tail = prev;
            }

            free(current);
            return true;
        }

        current = current->next;
    }

    return false;
}

bool release_ip_address(server_configs *server, dhcp_packet packet, network **net)
{
    if (queue_is_empty((*net)->bindings))
        return false;

    address_binding *addr;
    node *current = (*net)->bindings->head;

    while (current != NULL)
    {
        addr = (address_binding *)current->data;
        if (addr->ip_address == packet.header.ciaddr)
        {

            addr->status = RELEASED;
            addr->lease_time = 0;
            return true;
        }
        current = current->next;
    }
    return false;
}

