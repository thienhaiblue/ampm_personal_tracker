#ifndef __SPI_H__
#define __SPI_H__
#include "common.h"
#include <stdint.h>


#define SPI_TIMEOUT 100000



#define HAL_SPI_CS_DEASSERT  GPIOD_PSOR |= 1 << 2
#define HAL_SPI_CS_ASSERT    GPIOD_PCOR |= 1 << 2

#define HAL_SPI_ACCEL_CS_DEASSERT  GPIOA_PSOR |= 1 << 2
#define HAL_SPI_ACCEL_CS_ASSERT    GPIOA_PCOR |= 1 << 2

	
uint8_t halSpiWriteByte(uint8_t data);
void SPI_Init(void);
#endif

