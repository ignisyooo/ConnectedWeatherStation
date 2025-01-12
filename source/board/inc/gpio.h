#ifndef __GPIO_H__
#define __GPIO_H__

#include "error.h"

#define LED_RED_Pin GPIO_PIN_14
#define LED_RED_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_7
#define LED_BLUE_GPIO_Port GPIOB
#define STLK_RX_Pin GPIO_PIN_8
#define STLK_RX_GPIO_Port GPIOD
#define STLK_TX_Pin GPIO_PIN_9
#define STLK_TX_GPIO_Port GPIOD
#define SPI1_CS_Pin GPIO_PIN_4
#define SPI1_CS_GPIO_Port GPIOA
#define DC_Pin GPIO_PIN_12
#define DC_GPIO_Port GPIOF
#define RESET_Pin GPIO_PIN_15
#define RESET_GPIO_Port GPIOD

void MX_GPIO_Init(void);

#endif /*__ GPIO_H__ */
