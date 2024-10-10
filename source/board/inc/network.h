#ifndef __mx_lwip_H
#define __mx_lwip_H
#ifdef __cplusplus
 extern "C" {
#endif

#include <stdbool.h>

#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "ethernetif.h"

#if WITH_RTOS
#include "lwip/tcpip.h"
#endif /* WITH_RTOS */

extern ETH_HandleTypeDef heth;
extern struct netif gnetif;

void network_init(void);


bool network_isIpv4AddressAssigned(void);
bool network_getIpv4Address(ip4_addr_t *pAddr);

#ifdef __cplusplus
}
#endif
#endif /*__ mx_lwip_H */
