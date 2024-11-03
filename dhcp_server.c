#include "dhcp_server.h"

void initialize_server(server_configs *s, char *server_ip, const char *netmask, const char *gateway, char interface[20], const char *l_time, const char *p_time, const char *r_time, const char *rb_time)
{
    uint32_t ip,nm,gt;

    parse_ip(SERVER_IP,&ip);

    s->server_ip=ip;

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

        n[index].bindings=NULL;
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

        sqlite3_finalize(stmt);

    }


    return index;
}


