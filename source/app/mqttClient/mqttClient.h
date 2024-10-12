#include <stdbool.h>

#include "lwip/ip_addr.h"

typedef enum
{
    ULR,
    IPV4_ADDRESS,
} tMqttClient_brokerAddressType;

typedef struct
{
    const char* brokerAddres;
    uint16_t brokerPort;
    tMqttClient_brokerAddressType addresType;
} tMqttClient_brokerInfo;

typedef struct
{
    /* data */
    const char* topic;
    const char* data;
} tMqttClient_dataPacket;

void mqttClient_init( void );

void mqttClient_clientCreate( const char* clientId, const tMqttClient_brokerInfo* brokerInfo, const char* subTopic );
void mqttClient_connect( void );
void mqttClient_sendMessage( const char* topic, const char* msg );
