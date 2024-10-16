#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/
#define LOG_OUTPUT_BUFFER_SIZE ( 400U )
#define LOG_QUEUE_SIZE         ( 20u )

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/
typedef struct
{
    UART_HandleTypeDef *uartHandle;
    osMessageQueueId_t logQueue;
    tLogLevel currentLogLevel;
} tLogger;

typedef struct
{
    char msg[LOG_OUTPUT_BUFFER_SIZE];
    size_t msgSize;
} tLogMessage;

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static tLogger m_logger_log;
static osThreadId_t m_logger_taskHandler;

static const char *m_logger_logLevelName[] = {
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARNING] = "WARNING",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_DEBUG] = "DEBUG",
};

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void loggerTask( void *argument );

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void logger_init( UART_HandleTypeDef *huart )
{
    m_logger_log.uartHandle = huart;
    m_logger_log.currentLogLevel = LOG_LEVEL_INFO;

    m_logger_log.logQueue = osMessageQueueNew( LOG_QUEUE_SIZE, sizeof( tLogMessage ), NULL );

    const osThreadAttr_t attr = {
        .name = "loggerTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 1024
    };
    m_logger_taskHandler = osThreadNew( loggerTask, NULL, &attr );
}

void logger_setLogLevel( tLogLevel level )
{
    m_logger_log.currentLogLevel = level;
}

void logger_print( tLogLevel level, const char *file, int line, const char *format, ... )
{
    if( level <= m_logger_log.currentLogLevel )
    {
        va_list args;
        va_start( args, format );
        tLogMessage logMessage = { 0 };
        size_t offset = snprintf( logMessage.msg, LOG_OUTPUT_BUFFER_SIZE, "[%s:%d] [%s] ", file, line, m_logger_logLevelName[level] );
        size_t msgSize = vsnprintf( logMessage.msg + offset, LOG_OUTPUT_BUFFER_SIZE - offset, format, args );
        msgSize += offset;
        va_end( args );

        if( ( '\n' != logMessage.msg[msgSize - 1] ) && ( msgSize < LOG_OUTPUT_BUFFER_SIZE - 1 ) )
        {
            logMessage.msg[msgSize] = '\n';
            msgSize++;
        }

        logMessage.msgSize = msgSize;

        osMessageQueuePut( m_logger_log.logQueue, &logMessage, 0, 0 );
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void loggerTask( void *argument )
{
    tLogMessage logMessage;
    while( 1 )
    {
        if( osOK == osMessageQueueGet( m_logger_log.logQueue, &logMessage, NULL, osWaitForever ) )
        {
            HAL_UART_Transmit( m_logger_log.uartHandle, (uint8_t *)logMessage.msg, logMessage.msgSize, 1000 );
        }
    }
}