#include "network.h"

#include <string.h>

#include "error.h"
#include "ethernetif.h"
#include "logger.h"
#include "lwip/init.h"
#include "lwip/netif.h"

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

static tNetwork_statusCallback dhcp_callback_fn;

/* Ethernet link thread Argument */
static struct link_str link_arg;
static osSemaphoreId Netif_LinkSemaphore = NULL;

/*********************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void dhcpUpdateCallacbk( struct netif *netif );

/*********************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void network_init( tNetwork_statusCallback dhcp_callback )
{
    dhcp_callback_fn = dhcp_callback;
    /* Initilialize the LwIP stack with RTOS */
    tcpip_init( NULL, NULL );

    /* IP addresses initialization with DHCP (IPv4) */
    ip_addr_t ipaddr = { 0 };
    ip_addr_t netmask = { 0 };
    ip_addr_t gw = { 0 };

    /* add the network interface (IPv4/IPv6) with RTOS */
    netif_add( &gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input );

    /* Registers the default network interface */
    netif_set_default( &gnetif );

    if( netif_is_link_up( &gnetif ) )
    {
        /* When the netif is fully configured this function must be called */
        netif_set_up( &gnetif );
    }
    else
    {
        /* When the netif link is down this function must be called */
        netif_set_down( &gnetif );
    }

    /* Set the link callback function, this function is called on change of link status*/
    netif_set_link_callback( &gnetif, ethernetif_update_config );

    netif_set_status_callback( &gnetif, dhcpUpdateCallacbk );

    /* create a binary semaphore used for informing ethernetif of frame reception */
    Netif_LinkSemaphore = osSemaphoreNew( 1, 1, NULL );

    link_arg.netif = &gnetif;
    link_arg.semaphore = Netif_LinkSemaphore;
    /* Create the Ethernet link handler thread */

    osThreadAttr_t attributes;
    memset( &attributes, 0x0, sizeof( osThreadAttr_t ) );
    attributes.name = "LinkThr";
    attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
    attributes.priority = osPriorityBelowNormal;
    osThreadNew( ethernetif_set_link, &link_arg, &attributes );
}

void network_startDhcp( void )
{
    dhcp_start( &gnetif );
}

bool network_isLinkUp( void )
{
    return netif_is_link_up( &gnetif );
}

bool network_getIpv4Address( ip4_addr_t *pAddr )
{
    bool retVal = false;

    if( dhcp_supplied_address( &gnetif ) && ( NULL != pAddr ) )
    {
        memcpy( pAddr, &gnetif.ip_addr, sizeof( ip4_addr_t ) );
        retVal = true;
    }

    return retVal;
}

/*********************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void dhcpUpdateCallacbk( struct netif *netif )
{
    if( netif_is_up( netif ) && dhcp_supplied_address( netif ) )
    {
        // If DHCP has finished assigning an address, call user callback
        if( dhcp_callback_fn )
        {
            dhcp_callback_fn();
        }
    }
}
