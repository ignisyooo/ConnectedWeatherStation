
#include "mqttClient.h"

#include <stdbool.h>

#include "cmsis_os.h"
#include "logger.h"
#include "lwip/apps/mqtt.h"

typedef enum
{
    NOT_CREATED,
    CREATED_NON_CONNECTED,
    CONNECTED
} tMqttClient_clientStatus;

typedef struct
{
    const char* clientId;
    int16_t brokerPort;
    ip_addr_t brokerIp;
    tMqttClient_clientStatus status;
} tMqttClient_config;

#define MQTT_CLIENT_QUEUE_SIZE ( 10U )

static osMessageQueueId_t m_mqttClient_mqttQueue;
static bool m_mqttClient_initalized;

static tMqttClient_config m_mqttClient_clientCfg;
struct mqtt_connect_client_info_t m_mqttClient_connectionInfo;

static mqtt_client_t* m_mqttClient_client;

// static void mqttInit( void );
static void mqttClientTask( void* args );
static void mqtt_connectionCb( mqtt_client_t* client, void* arg, mqtt_connection_status_t status );
static void sendMessageOverMqtt( const tMqttClient_dataPacket* pMsg );

void mqttClient_init( void )
{
    if( !m_mqttClient_initalized )
    {
        m_mqttClient_mqttQueue = osMessageQueueNew( MQTT_CLIENT_QUEUE_SIZE, sizeof( tMqttClient_dataPacket ), NULL );

        if( NULL != m_mqttClient_mqttQueue )
        {
            const osThreadAttr_t attributes = {
                .name = "mqttTask",
                .stack_size = 2048,
                .priority = (osPriority_t)osPriorityNormal,
            };

            m_mqttClient_initalized = true;

            osThreadNew( mqttClientTask, NULL, &attributes );
        }
        else
        {
            LOG_ERROR( "Failed to create mqtt queue!" );
        }
    }
}

void mqttClient_clientCreate( const char* clientId, const tMqttClient_brokerInfo* brokerInfo, const char* subTopic )
{
    m_mqttClient_clientCfg.clientId = clientId;
    m_mqttClient_clientCfg.brokerPort = brokerInfo->brokerPort;

    if( IPV4_ADDRESS == brokerInfo->addresType )
    {
        ipaddr_aton( brokerInfo->brokerAddres, &m_mqttClient_clientCfg.brokerIp );
    }
    else
    {
        LOG_INFO( "DNS not implemented!" );
    }

    m_mqttClient_connectionInfo.client_id = m_mqttClient_clientCfg.clientId;
    m_mqttClient_connectionInfo.keep_alive = 50;

    m_mqttClient_clientCfg.status = CREATED_NON_CONNECTED;

    LOG_INFO( "Created new mqtt client" );
    LOG_INFO( "mqtt client id: %s", m_mqttClient_clientCfg.clientId );
    LOG_INFO( "mqtt broker address %s", ip4addr_ntoa( &m_mqttClient_clientCfg.brokerIp ) );
    LOG_INFO( "mqtt broker port %d", m_mqttClient_clientCfg.brokerPort );
}

void mqttClient_connect( void )
{
    if( m_mqttClient_initalized )
    {
        if( ERR_OK != mqtt_client_connect( m_mqttClient_client, &m_mqttClient_clientCfg.brokerIp,
                                           m_mqttClient_clientCfg.brokerPort, mqtt_connectionCb, NULL, &m_mqttClient_connectionInfo ) )
        {
            LOG_ERROR( "Cannot connect to broker" );
        }
    }
    else
    {
        LOG_ERROR( "Cannot connect to broker, mqtt not initalized" );
    }
}

void mqttClient_sendMessage( const char* topic, const char* msg )
{
    if( ( NULL != topic ) && ( NULL != msg ) )
    {
        tMqttClient_dataPacket data = {
            .data = msg,
            .topic = topic
        };

        osMessageQueuePut( m_mqttClient_mqttQueue, &data, NULL, osWaitForever );
    }
}

static void mqttClientTask( void* args )
{
    LOG_INFO( "Starting mqtt" );

    m_mqttClient_client = mqtt_client_new();
    tMqttClient_dataPacket msg = { 0 };

    while( 1 )
    {
        if( osOK == osMessageQueueGet( m_mqttClient_mqttQueue, &msg, NULL, osWaitForever ) )
        {
            sendMessageOverMqtt( &msg );
        }
    }
}

static void mqtt_connectionCb( mqtt_client_t* client, void* arg, mqtt_connection_status_t status )
{
    if( status == MQTT_CONNECT_ACCEPTED )
    {
        LOG_INFO( "mqtt_api_connectionCb: Successfully connected" );
    }
    else
    {
        LOG_INFO( "mqtt_connection_cb: Disconnected, reason: %d", status );
    }
}

static void sendMessageOverMqtt( const tMqttClient_dataPacket* pMsg )
{
    if( mqtt_client_is_connected( m_mqttClient_client ) )
    {
        uint8_t qos = 0;
        uint8_t retain = 0;
        size_t messageLength = strlen( pMsg->data ) + 1;
        err_t err = mqtt_publish( m_mqttClient_client, pMsg->topic, pMsg->data, messageLength, qos, retain, NULL, NULL );
        if( ERR_OK != err )
        {
            LOG_ERROR( "Publish err: %d\r\n", err );
        }
    }
    else
    {
        LOG_ERROR( "Cannot send message! Client is not connected" );
    }
}
