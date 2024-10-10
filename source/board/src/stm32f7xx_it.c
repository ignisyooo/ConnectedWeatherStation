/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include "stm32f7xx_it.h"

extern ETH_HandleTypeDef heth;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart3;


void NMI_Handler(void)
{

  while (1)
  {
  }
}


void HardFault_Handler(void)
{

  while (1)
  {
  }
}

void MemManage_Handler(void)
{

  while (1)
  {
  }
}


void BusFault_Handler(void)
{

  while (1)
  {

  }
}


void UsageFault_Handler(void)
{

  while (1)
  {
  }
}

void DebugMon_Handler(void)
{

}


void TIM6_DAC_IRQHandler(void)
{

  HAL_TIM_IRQHandler(&htim6);

}

void ETH_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&heth);
}


void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

