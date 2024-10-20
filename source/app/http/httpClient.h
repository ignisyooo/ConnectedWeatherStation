#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <stdbool.h>

#include "lwip/ip_addr.h"

#define HTTP_CLIENT_MAX_PROTOCOL_LENGTH ( sizeof( "HTTPS" ) + 1 )
#define HTTP_CLIENT_MAX_HOSTNAME_LENGTH ( 128U )
#define HTTP_CLIENT_MAX_PATH_LENGTH     ( 256U )

#define HTTP_CLIENT_REQUEST_TYPE( X ) \
    X( NOT_SPECIFIED )                \
    X( GET )                          \
    X( HEAD )                         \
    X( POST )                         \
    X( PUT )

#define REQUEST_TYPE_ENUM( TYPE )   TYPE,
#define REQUEST_TYPE_STRING( TYPE ) #TYPE,

#define HTTP_REQUEST_BUFFER_SIZE  ( 128u )
#define HTTP_RESPONSE_BUFFER_SIZE ( 2048u )

typedef enum
{
    HTTP_CLIENT_REQUEST_TYPE( REQUEST_TYPE_ENUM )
} tHttpClient_requestType;

typedef void ( *tHttpClient_responeCallback )( void );
typedef void ( *tHttpClient_errorCallback )( uint32_t errorCode );

typedef struct
{
    char protocol[HTTP_CLIENT_MAX_PROTOCOL_LENGTH];
    char host[HTTP_CLIENT_MAX_HOSTNAME_LENGTH];
    char path[HTTP_CLIENT_MAX_PATH_LENGTH];
    tHttpClient_requestType requestType;
    uint16_t port;
    tHttpClient_responeCallback responseCallback;
    tHttpClient_errorCallback errorCallback;
    char requestBuffer[HTTP_REQUEST_BUFFER_SIZE];
    char responseBuffer[HTTP_RESPONSE_BUFFER_SIZE];
    size_t bytesReceived;
    bool headerReceived;
    bool isInitalized;
} tHttpClient_client;

tHttpClient_client *httpClient_createNewHttpClient( tHttpClient_responeCallback responseCallback, tHttpClient_errorCallback errorCallback );
void httpClient_configureRequest( tHttpClient_client *client, const char *url, uint16_t port, tHttpClient_requestType type );

#endif /* _HTTP_CLIENT_H_ */