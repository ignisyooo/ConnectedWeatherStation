#include "clock.h"
#include "cmsis_os.h"
#include "displayController.h"
#include "dns_resolver.h"
#include "error.h"
#include "gpio.h"
#include "httpClient.h"
#include "httpSessionMgr.h"
#include "logger.h"
#include "mqttClient.h"
#include "network.h"
#include "networkMgr.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "sen55.h"
#include "i2c.h"

static void StartDefaultTask( void* argument );
static void userMqttDisconnectCallback( void );
static void userMqttDataCallback( const char* topic, const char* payload, size_t payloadLength );

static bool connected = false;

int main( void )
{
    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    RTC_Init();
    DMA_Init();
    SPI1_Init();
    I2C1_Init();

    osKernelInitialize();

    logger_init( &huart3 );

    LOG_INFO( "Initialization completed!" );

    const osThreadAttr_t attributes = {
        .name = "defaultTask",
        .stack_size = 10024,
        .priority = (osPriority_t)osPriorityHigh,
    };

    osThreadNew( StartDefaultTask, NULL, &attributes );

    /* Start scheduler */
    osKernelStart();

    while( 1 )
    {
        /* We should never get here as control is now taken by the scheduler */
    }
}

static void StartDefaultTask( void* argument )
{
    displayController_init();
    networkMgr_init();
    sen55_init();

    logger_setLogLevel( LOG_LEVEL_DEBUG );

    while( 1 )
    {
        HAL_GPIO_TogglePin( LED_BLUE_GPIO_Port, LED_BLUE_Pin );
        osDelay( 1000 );
    }
}

/* FREERTOS HOOKS */
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char* pcTaskName )
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
