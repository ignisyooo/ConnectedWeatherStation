#include "httpSessionMgr.h"

#include <lwip/inet.h>
#include <lwip/sockets.h>
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
#define HTTP_SEND_TIMOEUT_MS       ( 5000u )
#define HTTP_RECEIVE_TIMEOUT_MS    ( 20000u )

#define SESSION_STATE( X )                 \
    X( DISCONNECTED_WAIT_FOR_NEW_SESSION ) \
    X( CONNECTIING )                       \
    X( WAIT_FOR_CONNECTION )               \
    X( CONNECTED_SENDING_REQUEST )         \
    X( CONNECTED_WAIT_FOR_RESPONSE )       \
    X( ERROR_STATE )

#define SESSION_STATE_ENUM( NAME )   NAME,
#define SESSION_STATE_STRING( NAME ) #NAME,

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/

typedef enum
{
    SESSION_STATE( SESSION_STATE_ENUM )
} tHttpSessionMgr_state;

typedef struct
{
    osMessageQueueId_t sessionQueue;
    int socket_fd;
    tHttpClient_client *client;
    osThreadId_t taskHandler;
    tHttpSessionMgr_state state;
    bool isInitalized;
    bool isConnected;
    uint32_t errorCode;
} tHttpSessionMgr_session;

typedef void ( *tHttpSessionMgr_stateHandler )( void );

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void httpSessionTask( void *args );

static void closeSocket( int *socket_fd );
static void setState( tHttpSessionMgr_state newState );
static void handleWaitForRequestState( void );
static void handleConnectingState( void );
static void handleWaitForConnection( void );
static void handleSendRequestState( void );
static void handleWaitForResponse( void );
static void handleErrorState( void );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static tHttpSessionMgr_session m_httpSessionMgr_session;

static const tHttpSessionMgr_stateHandler m_httpSessionMgr_stateMachine[] = {
    [DISCONNECTED_WAIT_FOR_NEW_SESSION] = handleWaitForRequestState,
    [CONNECTIING] = handleConnectingState,
    [WAIT_FOR_CONNECTION] = handleWaitForConnection,
    [CONNECTED_SENDING_REQUEST] = handleSendRequestState,
    [CONNECTED_WAIT_FOR_RESPONSE] = handleWaitForResponse,
    [ERROR_STATE] = handleErrorState,
};

static const char *m_httpSessionMgr_requestTypes[] = {
    HTTP_CLIENT_REQUEST_TYPE( REQUEST_TYPE_STRING )
};

static const char *m_httpSessionMgr_sessionStateName[] = {
    SESSION_STATE( SESSION_STATE_STRING )
};

/************************************************************************************
 * PUBLIC FUNTCTION DEFINITIONS
 ***********************************************************************************/
void httpSessionMgr_init( void )
{
    if( !m_httpSessionMgr_session.isInitalized )
    {
        m_httpSessionMgr_session.socket_fd = -1;
        m_httpSessionMgr_session.client = NULL;
        m_httpSessionMgr_session.isConnected = false;

        m_httpSessionMgr_session.state = DISCONNECTED_WAIT_FOR_NEW_SESSION;

        m_httpSessionMgr_session.sessionQueue = osMessageQueueNew( HTTP_SESION_QUEUE_SIZE, sizeof( m_httpSessionMgr_session.client ), NULL );

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
    client->bytesReceived = 0;
    client->headerReceived = false;

    memset( client->requestBuffer, 0, HTTP_REQUEST_BUFFER_SIZE );
    memset( client->responseBuffer, 0, HTTP_RESPONSE_BUFFER_SIZE );

    if( ( m_httpSessionMgr_session.isInitalized ) && ( NULL != client ) &&
        ( client->isInitalized ) && ( NOT_SPECIFIED != client->requestType ) )
    {
        osMessageQueuePut( m_httpSessionMgr_session.sessionQueue, &client, 0, 0 );
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void setState( tHttpSessionMgr_state newState )
{
    LOG_DEBUG( "State transition: %s -> %s", m_httpSessionMgr_sessionStateName[m_httpSessionMgr_session.state], m_httpSessionMgr_sessionStateName[newState] );
    m_httpSessionMgr_session.state = newState;
}

static void closeSocket( int *socket_fd )
{
    if( *socket_fd >= 0 )
    {
        close( *socket_fd );
        *socket_fd = -1;
    }
}

static void handleWaitForRequestState( void )
{
    if( osOK == osMessageQueueGet( m_httpSessionMgr_session.sessionQueue, &m_httpSessionMgr_session.client, NULL, osWaitForever ) )
    {
        if( NULL != m_httpSessionMgr_session.client )
        {
            setState( CONNECTIING );
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
    ip_addr_t server_ip = { 0 };

    if( dnsResolver_resolveHostname( client->host, &server_ip ) )
    {
        m_httpSessionMgr_session.socket_fd = socket( AF_INET, SOCK_STREAM, 0 );
        if( m_httpSessionMgr_session.socket_fd < 0 )
        {
            LOG_ERROR( "Failed to create socket" );
            setState( ERROR_STATE );
            return;
        }

        // Setting the socket to non-blocking mode
        int flags = fcntl( m_httpSessionMgr_session.socket_fd, F_GETFL, 0 );
        fcntl( m_httpSessionMgr_session.socket_fd, F_SETFL, flags | O_NONBLOCK );

        struct sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_port = htons( client->port ),
            .sin_addr.s_addr = server_ip.addr
        };

        int result = connect( m_httpSessionMgr_session.socket_fd, (struct sockaddr *)&server_addr, sizeof( server_addr ) );
        if( result == 0 )
        {
            // Connection completed immediately successfully
            LOG_INFO( "Connected to %s", client->host );
            m_httpSessionMgr_session.isConnected = true;
            setState( CONNECTED_SENDING_REQUEST );
        }
        else if( errno == EINPROGRESS )
        {
            LOG_INFO( "Connection in progress..." );
            setState( WAIT_FOR_CONNECTION );
        }
        else
        {
            LOG_ERROR( "Connection failed immediately: %d", errno );
            closeSocket( &m_httpSessionMgr_session.socket_fd );
            setState( ERROR_STATE );
        }
    }
    else
    {
        LOG_ERROR( "DNS resolution failed for %s", client->host );
        setState( ERROR_STATE );
    }
}

static void handleWaitForConnection( void )
{
    struct timeval timeout;
    timeout.tv_sec = HTTP_CONNECTION_TIMEOUT_MS / 1000;
    timeout.tv_usec = ( HTTP_CONNECTION_TIMEOUT_MS % 1000 ) * 1000;

    fd_set write_fds;
    FD_ZERO( &write_fds );
    FD_SET( m_httpSessionMgr_session.socket_fd, &write_fds );

    int result = select( m_httpSessionMgr_session.socket_fd + 1, NULL, &write_fds, NULL, &timeout );
    if( result > 0 )
    {
        if( FD_ISSET( m_httpSessionMgr_session.socket_fd, &write_fds ) )
        {
            // Checking the connection status
            int so_error;
            socklen_t len = sizeof( so_error );
            getsockopt( m_httpSessionMgr_session.socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &len );

            if( so_error == 0 )
            {
                LOG_INFO( "Connection established" );
                m_httpSessionMgr_session.isConnected = true;
                setState( CONNECTED_SENDING_REQUEST );
            }
            else
            {
                LOG_ERROR( "Connection failed: %d", so_error );
                closeSocket( &m_httpSessionMgr_session.socket_fd );
                setState( ERROR_STATE );
            }
        }
    }
    else if( result == 0 )
    {
        // Timeout
        LOG_ERROR( "Connection timed out" );
        closeSocket( &m_httpSessionMgr_session.socket_fd );
        setState( ERROR_STATE );
    }
    else
    {
        LOG_ERROR( "select() error: %d", errno );
        closeSocket( &m_httpSessionMgr_session.socket_fd );
        setState( ERROR_STATE );
    }
}

static void handleSendRequestState( void )
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

        size_t totalSent = 0;

        while( totalSent < requestLength )
        {
            struct timeval timeout;
            timeout.tv_sec = HTTP_SEND_TIMOEUT_MS / 1000;
            timeout.tv_usec = ( HTTP_SEND_TIMOEUT_MS % 1000 ) * 1000;

            fd_set write_fds;
            FD_ZERO( &write_fds );
            FD_SET( m_httpSessionMgr_session.socket_fd, &write_fds );

            int result = select( m_httpSessionMgr_session.socket_fd + 1, NULL, &write_fds, NULL, &timeout );
            if( result > 0 )
            {
                if( FD_ISSET( m_httpSessionMgr_session.socket_fd, &write_fds ) )
                {
                    ssize_t sent = send( m_httpSessionMgr_session.socket_fd,
                                         client->requestBuffer + totalSent,
                                         requestLength - totalSent,
                                         0 );

                    if( sent > 0 )
                    {
                        totalSent += sent;
                    }
                    else if( sent < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) )
                    {
                        // Buffor is full, try again
                        continue;
                    }
                    else
                    {
                        LOG_ERROR( "Failed to send request: %d", errno );
                        closeSocket( &m_httpSessionMgr_session.socket_fd );
                        setState( ERROR_STATE );
                        return;
                    }
                }
            }
            else if( result == 0 )
            {
                // Timeout
                LOG_ERROR( "Send timeout" );
                closeSocket( &m_httpSessionMgr_session.socket_fd );
                setState( ERROR_STATE );
                return;
            }
            else
            {
                // select() error
                LOG_ERROR( "select() failed: %d", errno );
                closeSocket( &m_httpSessionMgr_session.socket_fd );
                setState( ERROR_STATE );
                return;
            }
        }

        LOG_INFO( "Request sent successfully" );
        setState( CONNECTED_WAIT_FOR_RESPONSE );
    }
    else
    {
        setState( DISCONNECTED_WAIT_FOR_NEW_SESSION );
    }
}

static void handleWaitForResponse( void )
{
    tHttpClient_client *client = m_httpSessionMgr_session.client;
    char buffer[512];
    ssize_t received = 0;
    size_t totalReceived = 0;

    while( true )
    {
        struct timeval timeout;
        timeout.tv_sec = HTTP_RECEIVE_TIMEOUT_MS / 1000;
        timeout.tv_usec = ( HTTP_RECEIVE_TIMEOUT_MS % 1000 ) * 1000;

        fd_set read_fds;
        FD_ZERO( &read_fds );
        FD_SET( m_httpSessionMgr_session.socket_fd, &read_fds );

        int result = select( m_httpSessionMgr_session.socket_fd + 1, &read_fds, NULL, NULL, &timeout );
        if( result > 0 )
        {
            if( FD_ISSET( m_httpSessionMgr_session.socket_fd, &read_fds ) )
            {
                received = recv( m_httpSessionMgr_session.socket_fd, buffer, sizeof( buffer ) - 1, 0 );
                if( received > 0 )
                {
                    buffer[received] = '\0';  // Null-terminate for logging/debugging
                    if( !client->headerReceived )
                    {
                        char *body_start = strstr( buffer, "\r\n\r\n" );
                        if( body_start != NULL )
                        {
                            client->headerReceived = true;
                            body_start += 4;

                            size_t body_len = received - ( body_start - buffer );
                            memcpy( client->responseBuffer, body_start, body_len );
                            totalReceived += body_len;
                        }
                    }
                    else
                    {
                        memcpy( client->responseBuffer + totalReceived, buffer, received );
                        totalReceived += received;
                    }
                }
                else if( received == 0 )
                {
                    LOG_DEBUG( "Connection closed by peer" );
                    break;
                }
                else if( errno == EAGAIN || errno == EWOULDBLOCK )
                {
                    // Wait for data
                    continue;
                }
                else
                {
                    LOG_ERROR( "recv() failed: %d", errno );
                    setState( ERROR_STATE );
                    return;
                }
            }
        }
        else if( result == 0 )
        {
            // Timeout
            LOG_ERROR( "Response timeout" );
            setState( ERROR_STATE );
            return;
        }
        else
        {
            // select() error
            LOG_ERROR( "select() failed: %d", errno );
            setState( ERROR_STATE );
            return;
        }
    }

    LOG_DEBUG( "Response fully received:\n%s\n", client->responseBuffer );
    client->responseBuffer[totalReceived] = '\0';
    client->bytesReceived = totalReceived;
    client->responseCallback( client->responseBuffer, client->bytesReceived );

    closeSocket( &m_httpSessionMgr_session.socket_fd );
    m_httpSessionMgr_session.isConnected = false;
    setState( DISCONNECTED_WAIT_FOR_NEW_SESSION );
}

static void handleErrorState( void )
{
    LOG_ERROR( "An error occurred. Returning to wait state." );

    if( m_httpSessionMgr_session.socket_fd >= 0 )
    {
        closeSocket( &m_httpSessionMgr_session.socket_fd );
    }

    if( m_httpSessionMgr_session.client != NULL && m_httpSessionMgr_session.client->errorCallback != NULL )
    {
        int errorCode = ( errno != 0 ) ? errno : -1;
        m_httpSessionMgr_session.client->errorCallback( errorCode );
    }

    m_httpSessionMgr_session.isConnected = false;
    setState( DISCONNECTED_WAIT_FOR_NEW_SESSION );
}

static void httpSessionTask( void *args )
{
    LOG_INFO( "Starting HTTP session manager task" );

    while( true )
    {
        m_httpSessionMgr_stateMachine[m_httpSessionMgr_session.state]();
        osDelay( 100 );
    }
}
