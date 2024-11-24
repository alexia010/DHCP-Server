#include "dhcp_header.h"

void set_header(dhcp_header *header, op_types o, hardware_address_types h_type,hardware_address_types h_len, uint32_t xid, uint16_t secs,uint16_t flags, uint32_t c_address, uint32_t y_addr, uint32_t s_addr, uint32_t g_addr, uint8_t mac[16])
{
    header->op=o;
    header->htype=h_type;
    header->hlen=h_len;
    header->xid=xid;
    header->secs=secs;
    header->flags=flags;

    header->ciaddr = c_address;
    header->yiaddr = y_addr;
    header->giaddr = g_addr;
    header->siaddr = s_addr;
    memcpy(header->chaddr, mac, ETHERNET_LEN);
    memset(header->sname,0,64);
    memset(header->file,0,128);
}

void serialize_header(dhcp_header *h, char *buffer, size_t *len)
{
    size_t offset=0;
    
    memcpy(buffer + offset, &h->op, sizeof(h->op));
    offset += sizeof(h->op);
    memcpy(buffer + offset, &h->htype, sizeof(h->htype));
    offset += sizeof(h->htype);
    memcpy(buffer + offset, &h->hlen, sizeof(h->hlen));
    offset += sizeof(h->hlen);
    memcpy(buffer + offset, &h->hops, sizeof(h->hops));
    offset += sizeof(h->hops);
    memcpy(buffer + offset, &h->xid, sizeof(h->xid));
    offset += sizeof(h->xid);
    memcpy(buffer + offset, &h->secs, sizeof(h->secs));
    offset += sizeof(h->secs);
    memcpy(buffer + offset, &h->flags, sizeof(h->flags));
    offset += sizeof(h->flags);
    memcpy(buffer + offset, &h->ciaddr, sizeof(h->ciaddr));
    offset += sizeof(h->ciaddr);
    memcpy(buffer + offset, &h->yiaddr, sizeof(h->yiaddr));
    offset += sizeof(h->yiaddr);
    memcpy(buffer + offset, &h->siaddr, sizeof(h->siaddr));
    offset += sizeof(h->siaddr);
    memcpy(buffer + offset, &h->giaddr, sizeof(h->giaddr));
    offset += sizeof(h->giaddr);
    memcpy(buffer + offset, h->chaddr, sizeof(h->chaddr));
    offset += sizeof(h->chaddr);
    memcpy(buffer + offset, h->sname, sizeof(h->sname));
    offset += sizeof(h->sname);
    memcpy(buffer + offset, h->file, sizeof(h->file));
    offset += sizeof(h->file);

    *len = offset;
};


