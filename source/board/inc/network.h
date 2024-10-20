#ifndef __mx_lwip_H
#define __mx_lwip_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#include "ethernetif.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"

#if WITH_RTOS
#include "lwip/tcpip.h"
#endif /* WITH_RTOS */

extern ETH_HandleTypeDef heth;
extern struct netif gnetif;

typedef void ( *tNetwork_statusCallback )( void );

void network_init( tNetwork_statusCallback dhcp_callback );
bool network_isLinkUp( void );
void network_startDhcp( void );
bool network_getIpv4Address( ip4_addr_t *pAddr );

#ifdef __cplusplus
}
#endif
#endif /*__ mx_lwip_H */
