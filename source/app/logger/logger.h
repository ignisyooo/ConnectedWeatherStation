#ifndef LOGGER_H
#define LOGGER_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "usart.h"

#include <string.h>

typedef enum
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} tLogLevel;

void logger_init(UART_HandleTypeDef *huart);
void logger_setLogLevel(tLogLevel level);
void logger_txCompleteCallback(void);
void logger_print(tLogLevel level, const char *file, int line, const char *format, ...);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_PRINT(LOG_LEVEL, format, ...)   logger_print(LOG_LEVEL, __FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  LOG_PRINT(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG_PRINT(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_PRINT(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#endif // LOGGER_H
