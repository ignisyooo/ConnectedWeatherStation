
#ifndef __SPI_H__
#define __SPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "error.h"

    extern SPI_HandleTypeDef hspi1;

    void SPI1_Init( void );

    void DMA_Init( void );

#ifdef __cplusplus
}
#endif

#endif /* __SPI_H__ */
