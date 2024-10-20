#include "networkMgr.h"

#include "cmsis_os.h"
#include "dns_resolver.h"
#include "httpSessionMgr.h"
#include "logger.h"
#include "mqttClient.h"
#include "network.h"
#include "timeSync.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/
typedef enum
{
    NET_INIT = 0,
    NET_WAIT_FOR_LINK,
    NET_WAIT_FOR_DHCP,
    NET_INIT_SERVICES,
    NET_RUNNING,
    NET_ERROR
} tNetworkMgr_state;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void networkMgrTask( void *args );
static void dhcpCompleteCallback( void );
static void startNetworkServices( void );
static char *getAssignedIpAddressAsString( void );
/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static tNetworkMgr_state m_networkMgr_state = NET_INIT;
static bool m_networkMgr_dhcpDone = false;
static bool m_networkMgr_initalized = false;
/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void networkMgr_init( void )
{
    if( !m_networkMgr_initalized )
    {
        const osThreadAttr_t attributes = {
            .name = "networkMgrTask",
            .stack_size = 2048,
            .priority = (osPriority_t)osPriorityNormal,
        };

        osThreadNew( networkMgrTask, NULL, &attributes );
    }
}

bool networkMgr_isReady( void )
{
    return ( m_networkMgr_state == NET_RUNNING );
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
void networkMgrTask( void *args )
{
    while( true )
    {
        switch( m_networkMgr_state )
        {
            case NET_INIT:
            {
                network_init( dhcpCompleteCallback );
                m_networkMgr_state = NET_WAIT_FOR_LINK;
            }
            break;

            case NET_WAIT_FOR_LINK:
            {
                if( network_isLinkUp() )
                {
                    network_startDhcp();
                    m_networkMgr_state = NET_WAIT_FOR_DHCP;
                }
                else
                {
                    // Wait for link up
                    osDelay( 1000 );
                }
            }
            break;

            case NET_WAIT_FOR_DHCP:
            {
                if( m_networkMgr_dhcpDone )
                {
                    LOG_INFO( "DHCP succeeded, IPv4 address assigned: %s\n", getAssignedIpAddressAsString() );
                    m_networkMgr_state = NET_INIT_SERVICES;
                }
                else
                {
                    // Wait for dhcp address assignment
                    osDelay( 500 );
                }
            }
            break;

            case NET_INIT_SERVICES:
            {
                startNetworkServices();
                m_networkMgr_state = NET_RUNNING;
            }
            break;
            case NET_RUNNING:
            {
                if( !network_isLinkUp() )
                {
                    // If the link is lost, we return to the initial state
                    m_networkMgr_state = NET_WAIT_FOR_LINK;
                }
                osDelay( 1000 );
            }
            break;

            case NET_ERROR:
            {
                LOG_ERROR( "Error detected. Starting network service from beggining!" );
                m_networkMgr_state = NET_WAIT_FOR_LINK;
            }
            break;
            default:
            {
                LOG_ERROR( "Unkown state!!!" );
            }
            break;
        }
    }
}

static char *getAssignedIpAddressAsString( void )
{
    char *result = NULL;
    ip4_addr_t addr;

    if( network_getIpv4Address( &addr ) )
    {
        result = ip4addr_ntoa( &addr );
    }

    return result;
}

static void startNetworkServices( void )
{
    LOG_INFO( "Stating network services" );
    dnsResolver_init();
    mqttClient_init();
    httpSessionMgr_init();
    timeSync_init();
}

static void dhcpCompleteCallback( void )
{
    m_networkMgr_dhcpDone = true;
}
