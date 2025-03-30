#include "stm32f7xx_it.h"

#include "usart.h"

extern ETH_HandleTypeDef heth;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;

void NMI_Handler( void )
{
    while( 1 )
    {
    }
}

void HardFault_Handler( void )
{
    while( 1 )
    {
    }
}

void MemManage_Handler( void )
{
    while( 1 )
    {
    }
}

void BusFault_Handler( void )
{
    while( 1 )
    {
    }
}

void UsageFault_Handler( void )
{
    while( 1 )
    {
    }
}

void DebugMon_Handler( void )
{
}

void TIM6_DAC_IRQHandler( void )
{
    HAL_TIM_IRQHandler( &htim6 );
}

void ETH_IRQHandler( void )
{
    HAL_ETH_IRQHandler( &heth );
}

void USART3_IRQHandler( void )
{
    HAL_UART_IRQHandler( &huart3 );
}

void SPI1_IRQHandler( void )
{
    HAL_SPI_IRQHandler( &hspi1 );
}

void DMA2_Stream3_IRQHandler( void )
{
    HAL_DMA_IRQHandler( &hdma_spi1_tx );
}

void I2C1_EV_IRQHandler( void )
{
    HAL_I2C_EV_IRQHandler( &hi2c1 );
}

void I2C1_ER_IRQHandler( void )
{
    HAL_I2C_ER_IRQHandler( &hi2c1 );
}
