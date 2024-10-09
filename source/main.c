#include "error.h"
#include "clock.h"
#include "gpio.h"
#include "usart.h"
#include "cmsis_os.h"

#include "logger.h"


static void StartDefaultTask(void* argument);

int main(void)
{
  HAL_Init();

  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  osKernelInitialize();

  logger_init(&huart3);

  LOG_INFO("Initialization completed!");

  const osThreadAttr_t attributes = {
    .name = "defaultTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityBelowNormal,
  };

  osThreadNew(StartDefaultTask, NULL, &attributes);

  /* Start scheduler */
  osKernelStart();

  while (1)
  {
    /* We should never get here as control is now taken by the scheduler */
  }

}


static void StartDefaultTask(void *argument)
{

  while (1)
  {
    LOG_INFO("Hello from default task\n");
    HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
    osDelay(1000);
  }
}

/* FREERTOS HOOKS */
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName )
{
  // logger_sendMessage("Stack overflow detected on task %s\r\n", pcTaskName);
  while(1){}
}

void vApplicationMallocFailedHook( void )
{
  // logger_sendMessage("Memory allocation failed\r\n");
  while(1){}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if(USART3 == huart->Instance)
  {
    logger_txCompleteCallback();
  }
}
