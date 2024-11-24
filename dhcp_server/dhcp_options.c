#include "dhcp_options.h"
#include <ctype.h>

#include <arpa/inet.h>
#include <stdint.h>

const uint8_t magic_cookie[4]={ 0x63, 0x82, 0x53, 0x63 };

void parse_ip(const char*ip_txt,uint32_t *ip_bin)
{
    struct sockaddr_in ip;

    ip.sin_family=AF_INET;

    if(inet_aton(ip_txt,&ip.sin_addr)==0)
    {
        log_msg(ERROR,"dhcp_options/parse_ip","invalid IP address");
        return ;
    }
    
   *ip_bin = ntohl(ip.sin_addr.s_addr);
 
    return;
}



void convert_ip(uint32_t ip_bin, char *ip_txt)
{
    struct in_addr ip;
    ip.s_addr=htonl(ip_bin);

    if(inet_ntop(AF_INET,&ip,ip_txt,INET_ADDRSTRLEN)==NULL)
    {
        log_msg(ERROR,"dhcp_options/parse_ip","Error converting uint_32 ip in txt ip");
        return;
    }

}

void parse_long(const char *s, time_t *p)
{
    *p=(time_t)strtol(s,NULL,0);
    
}

void parse_mac(const char *mac, uint8_t *mac_bin)
{

    memset(mac_bin,0,16);

    if(strlen(mac)!=17||
        mac[2]!=':'||mac[5]!=':'||mac[8]!=':'||mac[11]!=':'||mac[14]!=':')
        {
            log_msg(ERROR,"dhcp_options/parse_mac","invalid mac");
            return;
    }

    if (!isxdigit(mac[0]) || !isxdigit(mac[1]) || !isxdigit(mac[3]) || !isxdigit(mac[4]) || 
	!isxdigit(mac[6]) || !isxdigit(mac[7]) || !isxdigit(mac[9]) || !isxdigit(mac[10]) ||
	!isxdigit(mac[12]) || !isxdigit(mac[13]) || !isxdigit(mac[15]) || !isxdigit(mac[16])) 
	{
         log_msg(ERROR,"dhcp_options/parse_mac","invalid mac");
            return;
    }

    for (int i = 0; i < 6; i++) 
    {
        long byte= strtol(mac+(3*i), NULL, 16);
        ((uint8_t *) mac_bin)[i] = (uint8_t) byte;
    }

}

void convert_mac(uint8_t *mac_bin, char *mac_text)
{
    snprintf(mac_text, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_bin[0], mac_bin[1], mac_bin[2],
             mac_bin[3], mac_bin[4], mac_bin[5]);
}

void initialize_options_list(queue *q)
{

    queue_init(q);
}

int add_option_request(queue *q, uint8_t id, uint8_t len,const char*data)
{
    dhcp_option*opt=malloc(sizeof(dhcp_option));
    opt->id=id;
    opt->len=len;

    switch (id)
    {
    case SUBNET_MASK:
    case BROADCAST_ADDRESS:
    case SERVER_IDENTIFIER:
        opt->len=0;
        break;;
    case REQUESTED_IP_ADDRESS:
    case DHCP_MESSAGE_TYPE:
        if(len==1 || len == 5)
            switch (data[0])
            {
            case DHCP_DISCOVER:
            case DHCP_OFFER:
            case DHCP_REQUEST:
            case DHCP_DECLINE:
            case DHCP_ACK:
            case DHCP_NAK:
            case DHCP_RELEASE:
            case DHCP_INFORM:
                    memcpy(opt->data,data,len);
                break;
            default:
                log_msg(WARNING,"dhcp_options/add_option_request","Invalid dhcp message type.");
                return 1;
            }

        break;

    case IP_ADDRESS_LEASE_TIME:
    case RENEWAL_T1_TIME_VALUE:
    case REBINDING_T2_TIME_VALUE:
        memcpy(opt->data,data,len);
        break;

    case PARAMETER_REQUEST_LIST:
            switch (data[0])
            {
            case SUBNET_MASK:
            case DOMAIN_NAME_SERVER:
            case BROADCAST_ADDRESS:
            case REQUESTED_IP_ADDRESS:
            case IP_ADDRESS_LEASE_TIME:
            case SERVER_IDENTIFIER:
            case RENEWAL_T1_TIME_VALUE:
            case REBINDING_T2_TIME_VALUE:
                memcpy(opt->data,data,len);
                break;
            
            default:
                log_msg(WARNING,"dhcp_options/add_option_request","Invalid dhcp option.");
                return 1;
            }


            memcpy(opt->data,data,len);
        break;
        
    default:
       log_msg(WARNING,"dhcp_options/add_option_request","Invalid option");
       return 1;
    }

    queue_enqueue(q,opt);

    return 0;
}

size_t serialize_option_list(queue*q,uint8_t *buf,size_t len)
{
    uint8_t*p=buf;

    memcpy(p,magic_cookie,sizeof(magic_cookie));
    p+=4;

    len-=4;

    while(!queue_is_empty(q))
    {
        dhcp_option *opt=(dhcp_option*)queue_dequeue(q);

        if(len<=2+opt->len)
        {
            return 0;
            log_msg(WARNING,"dhcp_options/serialize_option_list","Option filled is full");
            return 1;
        }

        memcpy(p,opt,2+opt->len);
        p=p+2+opt->len;
        len=len-2-opt->len;
    }

    if(len<1)
    {
        log_msg(WARNING,"dhcp_options/serialize_option_list","Option filled is full");
        return 1;
    }

    *p=END;
    p++;
    len--;

    return p-buf;
}

int add_option_reply(queue*q,uint8_t id,uint8_t len, const char*data)
{
    dhcp_option *opt=malloc(sizeof(dhcp_option));

    opt->id=id;
    opt->len=len;

    switch (id)
    {
        case SUBNET_MASK:
        case BROADCAST_ADDRESS:
        case REQUESTED_IP_ADDRESS:
        case SERVER_IDENTIFIER:

            if(len==4)
            {
                memcpy(opt->data,data,len);

            }
            else
            {
                free(opt);
                log_msg(WARNING,"dhcp_options/add_option_reply","Invalid ip address.");
                return 1;
            }
        break;

        case DOMAIN_NAME_SERVER:
            if(len%4==0)
            {    
                memcpy(opt->data,data,len);
            }
            else
            {
                free(opt);
                log_msg(WARNING,"dhcp_options/add_option_reply","Invalid dns response.");
                return 1;
            }
            break;
        case IP_ADDRESS_LEASE_TIME:
        case RENEWAL_T1_TIME_VALUE:
        case REBINDING_T2_TIME_VALUE:
            if (len == 4) 
            {
                memcpy(opt->data, data, len);
            } 
            else 
            {
                free(opt);
                log_msg(WARNING,"dhcp_options/add_option_reply","Invalid time.");
                return 1;
            }
            break;
        case DHCP_MESSAGE_TYPE:
            if (len == 1) 
            {
                memcpy(opt->data, data, len);
            } 
            else 
            {
                free(opt);
                log_msg(WARNING,"dhcp_options/add_option_reply","Invalid dhcp_message_type.");
                return 1;
            }
            break;

        case PARAMETER_REQUEST_LIST:
            if (len > 0) 
            {
                memcpy(opt->data, data, len);
            } 
            else 
            {
                free(opt);
                log_msg(WARNING,"dhcp_options/add_option_reply","Invalid data for parameter request list option.");
                return 1;
            }
            break;
        
    default:
            free(opt);
            log_msg(WARNING,"dhcp_options/add_option_reply","Invalid option.");
            return 1;
    }

    queue_enqueue(q,opt);


}               

const char *byte_to_char(uint8_t p)
{
    char *str=malloc(sizeof(char)*4);

    sprintf(str,"%u",p);

    return str;
}