#include <stdbool.h>

#include "lwip/ip_addr.h"


typedef struct
{
    const char* brokerAddres;
    uint16_t brokerPort;
} tMqttClient_brokerInfo;

typedef struct
{
    /* data */
    const char* topic;
    const char* data;
} tMqttClient_dataPacket;


typedef enum
{
    CONNECTION_ACCEPTED,
    CONNECTION_REFUSED,
    CONNECTION_BROKER_NOT_AVAILABLE,
    CONNECTION_TIMEOUTED,
    CONNECTION_ERROR,
} tMqttClient_connectionResult;

typedef void (*tMqttClient_userCallback)(const char* topic, const char* payload, size_t payloadLength);
typedef void (*tMqttClient_disconnectCallback)(void);

void mqttClient_init( void );
void mqttClient_clientCreate( const char* clientId, const tMqttClient_brokerInfo* brokerInfo, const char* subTopic );
tMqttClient_connectionResult mqttClient_connect( void );
void mqttClient_sendMessage( const char* topic, const char* msg );
void mqttClient_registerCallbacks( tMqttClient_userCallback userCallback, tMqttClient_disconnectCallback disconnectCallback );
