#include "httpClient.h"

#include "cmsis_os.h"
#include "logger.h"
#include "portable.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/

#define URL_PROTOCOL_END_MARKER        ( "://" )
#define URL_PROTOCOL_END_MARKER_LEGNTH ( 3u )

#define URL_HOSTNAME_END_MARKER        ( '/' )
#define URL_HOSTNAME_END_MARKER_LEGNTH ( 1u )

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static bool parseUrl( const char *url, char *protocol, char *hostname, char *path );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
tHttpClient_client *httpClient_createNewHttpClient( tHttpClient_responeCallback responseCallback, tHttpClient_errorCallback errorCallback )
{
    tHttpClient_client *newClient = pvPortMalloc( sizeof( tHttpClient_client ) );

    if( NULL != newClient )
    {
        newClient->requestType = NOT_SPECIFIED;
        newClient->port = 0;
        newClient->responseCallback = responseCallback;
        newClient->errorCallback = errorCallback;
        newClient->isInitalized = true;
        newClient->headerReceived = false;
        newClient->bytesReceived = 0;
    }

    return newClient;
}

void httpClient_configureRequest( tHttpClient_client *client, const char *url, uint16_t port, tHttpClient_requestType type )
{
    if( ( NULL != client ) && ( client->isInitalized ) )
    {
        if( parseUrl( url, client->protocol, client->host, client->path ) )
        {
            client->requestType = type;
            client->port = port;
        }
    }
}

void httpClient_deleteClient( tHttpClient_client** client )
{
    if( NULL != client )
    {
        vPortFree(*client);
        *client = NULL;
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static bool parseUrl( const char *url, char *protocol, char *hostname, char *path )
{
    bool result = false;
    const char *url_ptr = url;
    const char *protocol_end = NULL;
    const char *hostname_start = NULL;
    const char *hostname_end = NULL;

    protocol_end = strstr( url_ptr, URL_PROTOCOL_END_MARKER );
    if( NULL != protocol_end )
    {
        strncpy( protocol, url_ptr, protocol_end - url_ptr );
        protocol[protocol_end - url_ptr] = '\0';

        hostname_start = protocol_end + URL_PROTOCOL_END_MARKER_LEGNTH;

        hostname_end = strchr( hostname_start, URL_HOSTNAME_END_MARKER );

        if( NULL == hostname_end )
        {
            // No path, so everything else is hostname
            strcpy( hostname, hostname_start );
            strcpy( path, "/" );
        }
        else
        {
            strncpy( hostname, hostname_start, hostname_end - hostname_start );
            hostname[hostname_end - hostname_start] = '\0';

            strcpy( path, hostname_end );
            result = true;
        }
    }

    return result;
}