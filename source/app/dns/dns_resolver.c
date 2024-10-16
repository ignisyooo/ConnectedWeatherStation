#include "dns_resolver.h"

#include <stdio.h>

#include "cmsis_os.h"
#include "logger.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"

/***********************************************************************************
 * PRIVATE MACROS DEFINTIONS
 ***********************************************************************************/
#define DNS_QUERY_TIMEOUT ( 1000u )

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/

static bool m_dnsResolver_initalized;
static osSemaphoreId_t m_dnsResolver_syncSempahore;
static ip_addr_t resolved_addr;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
void dnsFoundCllback( const char *name, const ip_addr_t *ipaddr, void *callback_arg );

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void dnsResolver_init( void )
{
    if( !m_dnsResolver_initalized )
    {
        m_dnsResolver_syncSempahore = osSemaphoreNew( 1, 0, NULL );

        if( NULL != m_dnsResolver_syncSempahore )
        {
            m_dnsResolver_initalized = true;
            LOG_INFO( "Dns resolver initialized" );
        }
    }
}

bool dnsResolver_resolveHostname( const char *hostname, ip_addr_t *out_ipaddr )
{
    bool success = false;

    // Start DNS query (non-blocking, uses callback)
    err_t err = dns_gethostbyname( hostname, &resolved_addr, dnsFoundCllback, NULL );

    if( ERR_OK == err )
    {
        // If the IP address is already cached, copy it immediately
        LOG_INFO("IP address for %s: %s\n", hostname, ip4addr_ntoa((const ip4_addr_t *)&resolved_addr));
        ip_addr_copy( *out_ipaddr, resolved_addr );
        success = true;
    }
    else if( ERR_INPROGRESS == err )
    {
        // If the DNS query is in progress, wait for it to complete
        LOG_INFO( "Resolving %s...", hostname );
        if( osOK == osSemaphoreAcquire( m_dnsResolver_syncSempahore, DNS_QUERY_TIMEOUT ) )
        {
            // If the semaphore was successfully acquired, check the resolved IP
            if( !ip_addr_isany( &resolved_addr ) )
            {
                ip_addr_copy( *out_ipaddr, resolved_addr );
                success = true;
            }
        }
    }

    return success;
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
void dnsFoundCllback( const char *name, const ip_addr_t *ipaddr, void *callback_arg )
{
    if( NULL == ipaddr )
    {
        ip_addr_copy( resolved_addr, *ipaddr );
    }
    else
    {
        ip_addr_set_zero( &resolved_addr );
    }

    osSemaphoreRelease( m_dnsResolver_syncSempahore );
}