#include "dhcp_packet.h"

dhcp_packet *create_dhcp_packet_client(op_types o, hardware_address_types h_type, hardware_address_types h_len, uint32_t xid, uint16_t secs, uint16_t flags,uint32_t c_address, uint32_t y_addr, uint32_t s_addr, uint32_t g_addr, uint8_t mac[16], uint8_t id, uint8_t len, const char*data)
{
    dhcp_packet*p = malloc(sizeof(dhcp_packet));
    queue_init(&p->dhcp_options);
    set_header(&p->header, o, h_type, h_len, xid, secs, flags, c_address, y_addr, s_addr, g_addr, mac);
    add_option_request(&p->dhcp_options, id, len, data);

    return p;
}

dhcp_packet *create_dhcp_packet_server(op_types o, hardware_address_types h_type, hardware_address_types h_len, uint32_t xid, uint16_t secs, uint16_t flags,uint32_t c_address, uint32_t y_addr, uint32_t s_addr, uint32_t g_addr, uint8_t mac[16], uint8_t id, uint8_t len, const char*data)
{
   
    dhcp_packet*p = malloc(sizeof(dhcp_packet));
    queue_init(&p->dhcp_options);
    set_header(&p->header, o, h_type, h_len, xid, secs, flags, c_address, y_addr, s_addr, g_addr, mac);
    add_option_reply(&p->dhcp_options, id, len, data);

    return p;
}

size_t serialize_packet(dhcp_packet *p, char *buffer)
{
    size_t total_len = 0;

    size_t header_len = 0;
    serialize_header(&p->header, buffer, &header_len);

    if(header_len != DHCP_HEADER_SIZE)
    {
        log_msg(WARNING,"serialize_packet","inappropriate header size");
        return -1;
    }

    size_t option_len = serialize_option_list(&p->dhcp_options, buffer + header_len, MAX_DHCP_SIZE - DHCP_HEADER_SIZE);

    total_len = header_len + option_len;

    return total_len;
}


void deserialize(char *buffer,size_t len, dhcp_packet *p)
{
    if(len<DHCP_HEADER_SIZE||len>MAX_DHCP_SIZE)
    {
        log_msg(WARNING,"deserialize","invalid buffer length");
    }
    int offset=0;
    
    memcpy(&(p->header.op), buffer + offset, 1); offset += 1;
    memcpy(&(p->header.htype), buffer + offset, 1); offset += 1;
    memcpy(&(p->header.hlen), buffer + offset, 1); offset += 1;
    memcpy(&(p->header.hops), buffer + offset, 1); offset += 1;
    memcpy(&(p->header.xid), buffer + offset, 4); offset += 4;
    memcpy(&(p->header.secs), buffer + offset, 2); offset += 2;
    memcpy(&(p->header.flags), buffer + offset, 2); offset += 2;
    memcpy(&(p->header.ciaddr), buffer + offset, 4); offset += 4;
    memcpy(&(p->header.yiaddr), buffer + offset, 4); offset += 4;
    memcpy(&(p->header.siaddr), buffer + offset, 4); offset += 4;
    memcpy(&(p->header.giaddr), buffer + offset, 4); offset += 4;
    memcpy(&(p->header.chaddr), buffer + offset, 16); offset += 16;
    memcpy(&(p->header.sname), buffer + offset, 64); offset += 64;
    memcpy(&(p->header.file), buffer + offset, 128); offset += 128;

    uint8_t magic_cookie[4];
    memcpy(magic_cookie, buffer + offset, 4); 
    offset += 4;

    if (memcmp(magic_cookie, "\x63\x82\x53\x63", 4) != 0) {
        log_msg(ERROR, "deserialize", "Magic cookie invalid.");
        return;
    }

    // uint8_t id;                       
    // uint8_t len;
    // uint8_t data[128];   

    while((uint8_t)buffer[offset]!=END)
    {
        if(offset>=len)
        {
            log_msg(WARNING,"deserialize","incorrect buffer format");
            return;
        }
        uint8_t id,len2; 
        id=buffer[offset++];
        len2=buffer[offset++];

        uint8_t data[128];  
        memcpy(data,buffer+offset,len2);
        offset+=len2;

        add_option_request(&p->dhcp_options,id,len2,data);
    }

}
