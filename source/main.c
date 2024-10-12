#include "clock.h"
#include "cmsis_os.h"
#include "error.h"
#include "gpio.h"
#include "logger.h"
#include "mqttClient.h"
#include "network.h"
#include "usart.h"

static void StartDefaultTask( void *argument );

int main( void )
{
    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    osKernelInitialize();

    logger_init( &huart3 );

    LOG_INFO( "Initialization completed!" );

    const osThreadAttr_t attributes = {
        .name = "defaultTask",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };

    osThreadNew( StartDefaultTask, NULL, &attributes );

    /* Start scheduler */
    osKernelStart();

    while( 1 )
    {
        /* We should never get here as control is now taken by the scheduler */
    }
}

static void StartDefaultTask( void *argument )
{
    network_init();
    mqttClient_init();

    const tMqttClient_brokerInfo info = {
        .addresType = IPV4_ADDRESS,
        .brokerAddres = "192.168.0.69",
        .brokerPort = 1883
    };

    mqttClient_clientCreate( "st-client", &info, NULL );
    // mqttClient_connect();
    bool connected = false;

    while( 1 )
    {
        if( !connected && network_isIpv4AddressAssigned() )
        {
            mqttClient_connect();
            connected = true;
        }
        HAL_GPIO_TogglePin( LED_BLUE_GPIO_Port, LED_BLUE_Pin );
        osDelay( 1000 );
        mqttClient_sendMessage("test", "Hello from st");
    }
}

/* FREERTOS HOOKS */
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName )
{
    LOG_ERROR( "Stack overflow detected on task %s\r\n", pcTaskName );
    while( 1 )
    {
    }
}

void vApplicationMallocFailedHook( void )
{
    LOG_ERROR( "Memory allocation failed\r\n" );
    while( 1 )
    {
    }
}
