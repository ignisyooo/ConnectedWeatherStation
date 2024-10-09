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
typedef struct {
    UART_HandleTypeDef *uartHandle;
    SemaphoreHandle_t uartSemaphore;
    tLogLevel currentLogLevel;
} tLogger;


/*********************************************************************************
* PRIVATE VARIABLES DECLERATION
***********************************************************************************/
static tLogger m_logger_log;
static char m_logger_outputBuffer[LOG_OUTPUT_BUFFER_SIZE];


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
}

void logger_txCompleteCallback(void)
{
    if( NULL != m_logger_log.uartHandle )
    {
        xSemaphoreGiveFromISR(m_logger_log.uartSemaphore, NULL);
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
        size_t offset = snprintf(m_logger_outputBuffer, LOG_OUTPUT_BUFFER_SIZE, "[%s:%d] ", file, line);
        size_t msgSize = vsnprintf(m_logger_outputBuffer + offset, LOG_OUTPUT_BUFFER_SIZE - offset, format, args);
        msgSize += offset;
        va_end(args);

        sendToUart( m_logger_outputBuffer, msgSize );
    }
}

/*********************************************************************************
* PRIVATE FUNTCTION DEFINITIONS
***********************************************************************************/
static void sendToUart(const char *msg, size_t msgSize)
{
    if(HAL_OK == HAL_UART_Transmit_IT(m_logger_log.uartHandle, (uint8_t *)msg, msgSize))
    {
        xSemaphoreTake(m_logger_log.uartSemaphore, portMAX_DELAY);
    }
}