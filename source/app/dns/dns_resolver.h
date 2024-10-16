#include <stdbool.h>

#include "lwip/ip_addr.h"

void dnsResolver_init( void );
bool dnsResolver_resolveHostname( const char *hostname, ip_addr_t *ipaddr );
