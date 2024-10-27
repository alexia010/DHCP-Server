#include <stdint.h>

#define DHCP_MAGIC_COOKIE 0x63825363
#define DHCP_OPTION_MSG_TYPE 53
#define REPLY 1
#define SEND 0

/// @brief 
struct dhcp_packet
{
    uint8_t op;        // operatie (1 pentru request, 2 pentru reply)
    uint8_t htype;     // tipul hardware (1 pentru Ethernet)
    uint8_t hlen;      // lungimea adresei hardware (6 pentru Ethernet)
    uint8_t hops;      // nr de hopuri (de obicei 0)
    uint32_t xid;      // ID-ul tranzactie
    uint16_t secs;     // Secunde de la începerea procesului DHCP
    uint16_t flags;    // Diverse flag-uri (de ex. broadcast 0x8000)
    uint32_t ciaddr;   // Client IP address (0.0.0.0 dacă nu are)
    uint32_t yiaddr;   // Your (client) IP address - adresa oferită clientului
    uint32_t siaddr;   // Server IP address
    uint32_t giaddr;   // Gateway IP address
    uint8_t chaddr[16]; // Adresa hardware a clientului (adresa MAC)
    char sname[64];    // Numele serverului (opțional)
    char file[128];    // Numele fișierului boot (opțional)
    uint8_t options[312]; // Opțiuni DHCP (magic cookie + opțiuni)
};

enum dhcp_message_type {
    DHCPDISCOVER = 1,
    DHCPOFFER = 2,
    DHCPREQUEST = 3,
    DHCPDECLINE = 4,
    DHCPACK = 5,
    DHCPNAK = 6,
    DHCPRELEASE = 7,
    DHCPINFORM = 8
};
