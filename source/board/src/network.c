#include "network.h"
#include "error.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "ethernetif.h"
#include <string.h>

#include "logger.h"

/*********************************************************************************
* PRIVATE MACROS 
***********************************************************************************/
#define INTERFACE_THREAD_STACK_SIZE ( 1024 )


/*********************************************************************************
* PUBLIC VARIABLES DECLERATION
***********************************************************************************/
struct netif gnetif;

/*********************************************************************************
* PRIVATE VARIABLES DECLERATION
***********************************************************************************/

/* Variables Initialization */
static ip4_addr_t ipaddr;
static ip4_addr_t netmask;
static ip4_addr_t gw;
static osThreadAttr_t attributes;
static bool m_network_dhcpAddressAssigned = false;

/* Ethernet link thread Argument */
static struct link_str link_arg;
static osSemaphoreId Netif_LinkSemaphore = NULL;


/*********************************************************************************
* PRIVATE FUNTCTION DECLERATION
***********************************************************************************/
static void dhcpUpdateCallacbk(struct netif *netif);


/*********************************************************************************
* PUBLIC FUNTCTION DEFINTIONS
***********************************************************************************/
void network_init(void)
{
  /* Initilialize the LwIP stack with RTOS */
  tcpip_init( NULL, NULL );

  /* IP addresses initialization with DHCP (IPv4) */
  ipaddr.addr = 0;
  netmask.addr = 0;
  gw.addr = 0;

  /* add the network interface (IPv4/IPv6) with RTOS */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

  /* Registers the default network interface */
  netif_set_default(&gnetif);

  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }

  /* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, ethernetif_update_config);

  netif_set_status_callback(&gnetif, dhcpUpdateCallacbk);

  /* create a binary semaphore used for informing ethernetif of frame reception */
  Netif_LinkSemaphore = osSemaphoreNew(1, 1, NULL);

  link_arg.netif = &gnetif;
  link_arg.semaphore = Netif_LinkSemaphore;
  /* Create the Ethernet link handler thread */
/* USER CODE BEGIN OS_THREAD_NEW_CMSIS_RTOS_V2 */
  memset(&attributes, 0x0, sizeof(osThreadAttr_t));
  attributes.name = "LinkThr";
  attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
  attributes.priority = osPriorityBelowNormal;
  osThreadNew(ethernetif_set_link, &link_arg, &attributes);
/* USER CODE END OS_THREAD_NEW_CMSIS_RTOS_V2 */

  /* Start DHCP negotiation for a network interface (IPv4) */
  dhcp_start(&gnetif);

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
}

bool network_isIpv4AddressAssigned(void)
{
  return m_network_dhcpAddressAssigned;
}

bool network_getIpv4Address(ip4_addr_t *pAddr)
{
  bool retVal = false;

  if( m_network_dhcpAddressAssigned && ( NULL != pAddr ) )
  {
    memcpy( pAddr, &ipaddr, sizeof(ip4_addr_t));
    retVal = true;
  }

  return retVal;
}

/*********************************************************************************
* PRIVATE FUNTCTION DEFINITIONS
***********************************************************************************/
static void dhcpUpdateCallacbk(struct netif *netif)
{
  if (netif_is_up(netif))
  {
    if (0 !=netif->ip_addr.addr)
    {
      LOG_INFO("DHCP succeeded, IP address assigned: %s\n", ip4addr_ntoa(&netif->ip_addr));
      m_network_dhcpAddressAssigned = true;
    }
  }
}
