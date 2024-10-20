
#include "httpSessionMgr.h"

#include <lwip/inet.h>
#include <lwip/tcp.h>
#include <stdbool.h>

#include "cmsis_os.h"
#include "dns_resolver.h"
#include "logger.h"
#include "lwip/errno.h"
#include "lwip/ip_addr.h"
/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/
#define HTTP_SESION_QUEUE_SIZE ( 5u )

#define HTTP_CONNECTION_TIMEOUT_MS ( 5000u )
#define HTTP_RECEIVE_TIMEOUT_MS    ( 10000u )

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/

typedef enum
{
    DISCONNECTED_WAIT_FOR_NEW_SESSION,
    CONNECTIING,
    WAIT_FOR_CONNECTION,
    CONNECTED_SENDING_REQUEST,
    CONNECTED_WAIT_FOR_RESPONSE,
    ERROR_STATE
} tHttpSessionMgr_state;

typedef struct
{
    osMessageQueueId_t sessionQueue;
    osSemaphoreId_t connectSemaphore;
    osSemaphoreId_t receiveSemaphore;
    struct tcp_pcb *pcb;
    tHttpClient_client *client;
    osThreadId_t taskHandler;
    tHttpSessionMgr_state state;
    bool isInitalized;
    bool isConnected;
} tHttpSessionMgr_session;

typedef void ( *tHttpSessionMgr_stateHandler )( void );

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void httpSessionTask( void *args );

static void handleWaitForRequestState( void );
static void handleConnectingState( void );
static void handleWaitForConnectionState( void );
static void handleSendRequestState( void );
static void handleWaitForResponse( void );
static void handleErrorState( void );

static err_t tcpConnectedCallback( void *arg, struct tcp_pcb *tpcb, err_t err );
static err_t tcpReceiveCallback( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static tHttpSessionMgr_session m_httpSessionMgr_session;

static const tHttpSessionMgr_stateHandler m_httpSessionMgr_stateMachine[] = {
    [DISCONNECTED_WAIT_FOR_NEW_SESSION] = handleWaitForRequestState,
    [CONNECTIING] = handleConnectingState,
    [WAIT_FOR_CONNECTION] = handleWaitForConnectionState,
    [CONNECTED_SENDING_REQUEST] = handleSendRequestState,
    [CONNECTED_WAIT_FOR_RESPONSE] = handleWaitForResponse,
    [ERROR_STATE] = handleErrorState,
};

static const char *m_httpSessionMgr_requestTypes[] = {
    HTTP_CLIENT_REQUEST_TYPE( REQUEST_TYPE_STRING )
};

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void httpSessionMgr_init( void )
{
    if( !m_httpSessionMgr_session.isInitalized )
    {
        m_httpSessionMgr_session.pcb = NULL;
        m_httpSessionMgr_session.client = NULL;
        m_httpSessionMgr_session.isConnected = false;

        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;

        m_httpSessionMgr_session.sessionQueue = osMessageQueueNew( HTTP_SESION_QUEUE_SIZE, sizeof( m_httpSessionMgr_session.client ), NULL );
        m_httpSessionMgr_session.connectSemaphore = osSemaphoreNew( 1, 0, NULL );
        m_httpSessionMgr_session.receiveSemaphore = osSemaphoreNew( 1, 0, NULL );

        const osThreadAttr_t attr = {
            .name = "httpSessionMgr",
            .priority = (osPriority_t)osPriorityHigh1,
            .stack_size = 4096
        };

        m_httpSessionMgr_session.taskHandler = osThreadNew( httpSessionTask, NULL, &attr );
        m_httpSessionMgr_session.isInitalized = true;
    }
}

void httpSessionMgr_startNewSession( tHttpClient_client *client )
{
    if( ( m_httpSessionMgr_session.isInitalized ) && ( NULL != client ) &&
        ( client->isInitalized ) && ( NOT_SPECIFIED != client->requestType ) )
    {
        osMessageQueuePut( m_httpSessionMgr_session.sessionQueue, &client, 0, 0 );
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static err_t tcpConnectedCallback( void *arg, struct tcp_pcb *tpcb, err_t err )
{
    if( ERR_OK == err )
    {
        osSemaphoreRelease( m_httpSessionMgr_session.connectSemaphore );
    }
    else
    {
        LOG_ERROR( "Connection error: %d", err );
        tcp_close( tpcb );
    }

    return err;
}

static err_t tcpReceiveCallback( void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err )
{
    static size_t total_len = 0;
    tHttpClient_client *client = m_httpSessionMgr_session.client;

    if( p == NULL )
    {
        client->responseBuffer[total_len] = '\0';
        total_len = 0;
        tcp_close( tpcb );
        osSemaphoreRelease( m_httpSessionMgr_session.receiveSemaphore );
        return ERR_OK;
    }

    char *payload = (char *)p->payload;
    size_t payload_len = p->len;

    if( !client->headerReceived )
    {
        char *body_start = strstr( payload, "\r\n\r\n" );
        if( body_start != NULL )
        {
            client->headerReceived = true;
            body_start += 4;

            size_t body_len = payload_len - ( body_start - payload );
            memcpy( client->responseBuffer + total_len, body_start, body_len );
            total_len += body_len;
        }
    }
    else
    {
        memcpy( client->responseBuffer + total_len, payload, payload_len );
        total_len += payload_len;
    }

    client->responseBuffer[total_len] = '\0';

    LOG_DEBUG( "Part of response body received (length: %u), total length: %u", payload_len, total_len );

    tcp_recved( tpcb, p->len );
    pbuf_free( p );

    return ERR_OK;
}

static void handleWaitForRequestState( void )
{
    if( osOK == osMessageQueueGet( m_httpSessionMgr_session.sessionQueue, &m_httpSessionMgr_session.client, NULL, osWaitForever ) )
    {
        if( NULL != m_httpSessionMgr_session.client )
        {
            m_httpSessionMgr_session.state = CONNECTIING;
        }
        else
        {
            LOG_ERROR( "Received request from NULL client" );
        }
    }
}

static void handleConnectingState( void )
{
    tHttpClient_client *client = m_httpSessionMgr_session.client;
    m_httpSessionMgr_session.pcb = tcp_new();

    if( NULL == m_httpSessionMgr_session.pcb )
    {
        LOG_ERROR( "Error during PCB creation" );
        m_httpSessionMgr_session.state = ERROR_STATE;
    }
    else
    {
        ip_addr_t server_ip = { 0 };
        if( dnsResolver_resolveHostname( client->host, &server_ip ) )
        {
            tcp_arg( m_httpSessionMgr_session.pcb, client );

            err_t err = tcp_connect( m_httpSessionMgr_session.pcb, &server_ip, client->port, tcpConnectedCallback );

            if( ERR_OK == err )
            {
                LOG_INFO( "Connecting %s...", client->host );
                m_httpSessionMgr_session.state = WAIT_FOR_CONNECTION;
            }
            else
            {
                LOG_ERROR( "Connectiong error: %d", err );
                tcp_close( m_httpSessionMgr_session.pcb );
                m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
            }
        }
        else
        {
            LOG_ERROR( "Error during address resolving for %s", client->host );
            tcp_close( m_httpSessionMgr_session.pcb );
            m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
        }
    }
}

void handleWaitForConnectionState( void )
{
    if( osOK == osSemaphoreAcquire( m_httpSessionMgr_session.connectSemaphore, HTTP_CONNECTION_TIMEOUT_MS ) )
    {
        LOG_INFO( "Connected successfully" );
        m_httpSessionMgr_session.isConnected = true;
        tcp_recv( m_httpSessionMgr_session.pcb, tcpReceiveCallback );
        m_httpSessionMgr_session.state = CONNECTED_SENDING_REQUEST;
    }
    else
    {
        LOG_ERROR( "Connection timed out" );
        tcp_close( m_httpSessionMgr_session.pcb );
        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
    }
}

void handleSendRequestState( void )
{
    if( m_httpSessionMgr_session.isConnected )
    {
        tHttpClient_client *client = m_httpSessionMgr_session.client;
        size_t requestLength = snprintf( client->requestBuffer, HTTP_REQUEST_BUFFER_SIZE,
                                         "%s %s HTTP/1.1\r\n"
                                         "Host: %s\r\n"
                                         "Accept: */*\r\n"
                                         "Connection: close\r\n"
                                         "\r\n",
                                         m_httpSessionMgr_requestTypes[client->requestType],
                                         client->path, client->host );

        err_t err = tcp_write( m_httpSessionMgr_session.pcb, client->requestBuffer, requestLength, TCP_WRITE_FLAG_COPY );
        if( ERR_OK == err )
        {
            tcp_output( m_httpSessionMgr_session.pcb );
            LOG_INFO( "A %s request was sent to %s", m_httpSessionMgr_requestTypes[client->requestType], client->host );
            m_httpSessionMgr_session.state = CONNECTED_WAIT_FOR_RESPONSE;
        }
        else
        {
            LOG_ERROR( "Error while sending request: %d", err );
            tcp_close( m_httpSessionMgr_session.pcb );
            m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
        }
    }
    else
    {
        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
    }
}

static void handleWaitForResponse( void )
{
    if( osOK == osSemaphoreAcquire( m_httpSessionMgr_session.receiveSemaphore, HTTP_RECEIVE_TIMEOUT_MS ) )
    {
        tHttpClient_client *client = m_httpSessionMgr_session.client;
        LOG_DEBUG( "Final response received:\n%s\n", client->responseBuffer );
        // TODO: header parsing
        client->responseCallback();
        tcp_close( m_httpSessionMgr_session.pcb );
        m_httpSessionMgr_session.isConnected = false;
        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
    }
    else
    {
        LOG_ERROR( "Receiving data timed out" );
        tcp_close( m_httpSessionMgr_session.pcb );
        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
    }
}

static void handleErrorState( void )
{
    LOG_ERROR( "Error detected!!!!" );
    tcp_close( m_httpSessionMgr_session.pcb );
    m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;
}

static void httpSessionTask( void *args )
{
    LOG_INFO( "Starting http session mgr" );

    while( true )
    {
        m_httpSessionMgr_stateMachine[m_httpSessionMgr_session.state]();
        osDelay( 100 );
    }
}
