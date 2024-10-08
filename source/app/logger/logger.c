#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


/*********************************************************************************
* PRIVATE MACROS 
***********************************************************************************/
#define LOG_OUTPUT_BUFFER_SIZE 200U

/*********************************************************************************
* PRIVATE TYPES DECLARATION
***********************************************************************************/
typedef struct 
{
    UART_HandleTypeDef *uartHandle;
    SemaphoreHandle_t uartSemaphore;
    tLogLevel currentLogLevel;
} tLogger;


/*********************************************************************************
* PRIVATE VARIABLES DECLERATION
***********************************************************************************/
static tLogger m_logger_log;

/*********************************************************************************
* PRIVATE FUNTCTION DECLERATION
***********************************************************************************/
static void sendToUart(const char *msg, size_t msgSize);

/*********************************************************************************
* PUBLIC FUNTCTION DEFINTIONS
***********************************************************************************/
void logger_init(UART_HandleTypeDef *huart)
{
    m_logger_log.uartHandle = huart;
    m_logger_log.currentLogLevel = LOG_LEVEL_INFO;

    m_logger_log.uartSemaphore = xSemaphoreCreateBinary();

    if( NULL != m_logger_log.uartSemaphore )
    {
        xSemaphoreGive( m_logger_log.uartSemaphore );
    }
}

void logger_setLogLevel(tLogLevel level)
{
    m_logger_log.currentLogLevel = level;
}

void logger_print(tLogLevel level, const char *file, int line, const char *format, ...)
{
    if (level <= m_logger_log.currentLogLevel)
    {
        va_list args;
        va_start(args, format);
        char logMsg[LOG_OUTPUT_BUFFER_SIZE] = { 0 };
        size_t offset = snprintf(logMsg, LOG_OUTPUT_BUFFER_SIZE, "[%s:%d] ", file, line);
        size_t msgSize = vsnprintf(logMsg + offset, LOG_OUTPUT_BUFFER_SIZE - offset, format, args);
        msgSize += offset;
        va_end(args);

        if( ( '\n' != logMsg[msgSize - 1]) && (msgSize < LOG_OUTPUT_BUFFER_SIZE - 1) )
        {
            logMsg[msgSize] = '\n'; 
            msgSize++;
        }

        sendToUart( logMsg, msgSize );
    }
}

/*********************************************************************************
* PRIVATE FUNTCTION DEFINITIONS
***********************************************************************************/
static void sendToUart(const char *msg, size_t msgSize)
{
    xSemaphoreTake(m_logger_log.uartSemaphore, portMAX_DELAY);
    HAL_UART_Transmit(m_logger_log.uartHandle, (uint8_t *)msg, msgSize, 1000);
    xSemaphoreGive(m_logger_log.uartSemaphore);
}