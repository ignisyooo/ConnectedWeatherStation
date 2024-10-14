
#include "mqttClient.h"

#include <stdbool.h>

#include "cmsis_os.h"
#include "logger.h"
#include "lwip/apps/mqtt.h"

/***********************************************************************************
 * PRIVATE MACROS DEFINTIONS
 ***********************************************************************************/
#define MQTT_CLIENT_QUEUE_SIZE   ( 10U )
#define MQTT_TOPIC_LENGTH        ( 100u )
#define MQTT_CONNECTION_TIMEOUT  ( 1000u )
#define MQTT_RECEIVE_BUFFER_SIZE ( 200U )

/***********************************************************************************
 * PRIVATE TYPES DEFINTIONS
 ***********************************************************************************/

typedef struct
{
    const char* clientId;
    int16_t brokerPort;
    ip_addr_t brokerIp;
    tMqttClient_userCallback userCallback;
    tMqttClient_disconnectCallback disconnectCallback;
} tMqttClient_config;

typedef struct
{
    char payload[MQTT_RECEIVE_BUFFER_SIZE];
    uint16_t payload_len;
    uint16_t received_len;
} tMqttClient_msgInfo;

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static osMessageQueueId_t m_mqttClient_mqttQueue;
static osSemaphoreId_t m_mqttClient_syncSemaphore;
static bool m_mqttClient_initalized;

static tMqttClient_config m_mqttClient_clientCfg;
struct mqtt_connect_client_info_t m_mqttClient_connectionInfo;
static tMqttClient_connectionResult m_mqttClient_connectionStatus;

static mqtt_client_t* m_mqttClient_client;
static char m_mqttClient_subTopic[MQTT_TOPIC_LENGTH];
static bool m_mqttClient_connectionInProgress;
static tMqttClient_msgInfo m_mqttClient_messageInfo;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void mqttClientTask( void* args );
static void connectionStatusCallback( mqtt_client_t* client, void* arg, mqtt_connection_status_t status );
static void sendMessageOverMqtt( const tMqttClient_dataPacket* pMsg );
static void subscribeTopic( mqtt_client_t* client, const char* topic );
static void subcribeResultCallback( void* arg, err_t result );
static void incomingDataCallback( void* arg, const u8_t* data, u16_t len, u8_t flags );
static void incommingPublishCallback( void* arg, const char* topic, u32_t tot_len );

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
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

            m_mqttClient_syncSemaphore = osSemaphoreNew( 1, 0, NULL );

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
    if( m_mqttClient_initalized )
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

        strncpy( m_mqttClient_subTopic, subTopic, MQTT_TOPIC_LENGTH );

        m_mqttClient_connectionInfo.client_id = m_mqttClient_clientCfg.clientId;
        m_mqttClient_connectionInfo.keep_alive = 50;

        LOG_INFO( "Created new mqtt client" );
        LOG_INFO( "mqtt client id: %s", m_mqttClient_clientCfg.clientId );
        LOG_INFO( "mqtt broker address %s", ip4addr_ntoa( &m_mqttClient_clientCfg.brokerIp ) );
        LOG_INFO( "mqtt broker port %d", m_mqttClient_clientCfg.brokerPort );
        LOG_INFO( "mqtt subscribe topic: %s", m_mqttClient_subTopic );
    }
}

tMqttClient_connectionResult mqttClient_connect( void )
{
    tMqttClient_connectionResult result = CONNECTION_ERROR;
    m_mqttClient_connectionInProgress = true;

    if( ERR_OK == mqtt_client_connect( m_mqttClient_client, &m_mqttClient_clientCfg.brokerIp,
                                       m_mqttClient_clientCfg.brokerPort, connectionStatusCallback, NULL, &m_mqttClient_connectionInfo ) )
    {
        if( osOK == osSemaphoreAcquire( m_mqttClient_syncSemaphore, osWaitForever ) )
        {
            subscribeTopic( m_mqttClient_client, m_mqttClient_subTopic );
            result = m_mqttClient_connectionStatus;
        }
        else
        {
            result = CONNECTION_TIMEOUTED;
        }
    }

    return result;
}

void mqttClient_sendMessage( const char* topic, const char* msg )
{
    if( ( NULL != topic ) && ( NULL != msg ) )
    {
        tMqttClient_dataPacket data = {
            .data = msg,
            .topic = topic
        };

        osMessageQueuePut( m_mqttClient_mqttQueue, &data, 0, osWaitForever );
    }
}

void mqttClient_registerCallbacks( tMqttClient_userCallback userCallback, tMqttClient_disconnectCallback disconnectCallback )
{
    m_mqttClient_clientCfg.disconnectCallback = disconnectCallback;
    m_mqttClient_clientCfg.userCallback = userCallback;
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
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

static void connectionStatusCallback( mqtt_client_t* client, void* arg, mqtt_connection_status_t status )
{
    switch( status )
    {
        case MQTT_CONNECT_ACCEPTED:
        {
            LOG_DEBUG( "connectionStatusCallback: connection accepted" );
            m_mqttClient_connectionStatus = CONNECTION_ACCEPTED;
        }
        break;
        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
        case MQTT_CONNECT_REFUSED_SERVER:
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
        {
            LOG_DEBUG( "connectionStatusCallback: received unnkown status" );
            m_mqttClient_connectionStatus = CONNECTION_REFUSED;
        }
        break;
        case MQTT_CONNECT_TIMEOUT:
        {
            m_mqttClient_connectionStatus = CONNECTION_TIMEOUTED;
            LOG_DEBUG( "connectionStatusCallback: timeout durring connection" );
        };
        break;
        case MQTT_CONNECT_DISCONNECTED:
        {
            LOG_DEBUG( "connectionStatusCallback: disconnected" );
            m_mqttClient_connectionStatus = CONNECTION_BROKER_NOT_AVAILABLE;
            if( ( NULL != m_mqttClient_clientCfg.disconnectCallback ) && !m_mqttClient_connectionInProgress )
            {
                m_mqttClient_clientCfg.disconnectCallback();
            }
        }
        break;
        default:
        {
            LOG_ERROR( "connectionStatusCallback: received unnkown status" );
        }
        break;
    }

    m_mqttClient_connectionInProgress = false;
    osSemaphoreRelease( m_mqttClient_syncSemaphore );
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

static void subcribeResultCallback( void* arg, err_t result )
{
    LOG_DEBUG( "Subscribe result: %d", result );
}

static void subscribeTopic( mqtt_client_t* client, const char* topic )
{
    uint8_t qos = 0;
    mqtt_set_inpub_callback( m_mqttClient_client, incommingPublishCallback, incomingDataCallback, NULL );
    err_t err = mqtt_subscribe( client, topic, qos, subcribeResultCallback, NULL );
    if( ERR_OK != err )
    {
        LOG_ERROR( "mqtt_subscribe return: %d\n", err );
    }
}

/* Callback required by mqtt API */
static void incomingDataCallback( void* arg, const u8_t* data, u16_t len, u8_t flags )
{
    LOG_DEBUG( "Incoming publish payload with length %d, flags %u", len, (unsigned int)flags );

    if( m_mqttClient_messageInfo.received_len + len < MQTT_RECEIVE_BUFFER_SIZE )
    {
        memcpy( m_mqttClient_messageInfo.payload + m_mqttClient_messageInfo.received_len, data, len );
        m_mqttClient_messageInfo.received_len += len;

        if( flags & MQTT_DATA_FLAG_LAST )
        {
            m_mqttClient_messageInfo.payload[m_mqttClient_messageInfo.received_len] = '\0';

            if( NULL != m_mqttClient_clientCfg.userCallback )
            {
                m_mqttClient_clientCfg.userCallback( m_mqttClient_subTopic, m_mqttClient_messageInfo.payload, m_mqttClient_messageInfo.received_len );
            }

            LOG_DEBUG( "Full message received on topic: %s, payload: %s", m_mqttClient_subTopic, m_mqttClient_messageInfo.payload );
        }
    }
    else
    {
        LOG_ERROR( "Payload too large, discarding message" );
    }
}

/* Callback required by mqtt API */
static void incommingPublishCallback( void* arg, const char* topic, u32_t tot_len )
{
    LOG_DEBUG( "Incoming publish at topic %s with total length %u", topic, (unsigned int)tot_len );

    memset( m_mqttClient_messageInfo.payload, 0, MQTT_RECEIVE_BUFFER_SIZE );

    m_mqttClient_messageInfo.payload_len = tot_len;
    m_mqttClient_messageInfo.received_len = 0;
}
