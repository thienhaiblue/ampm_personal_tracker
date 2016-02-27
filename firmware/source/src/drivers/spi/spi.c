
/**
* \file
*         spi driver
* \author
*         Nguyen Van Hai <blue@ambo.com.vn>
*/
#include "spi.h"



void SPI_Init(void)
{
	/* SIM_SCGC4: SPI0=1 */
  SIM_SCGC4 |= SIM_SCGC4_SPI1_MASK;       
	SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK;	
	
	PORTA_PCR2 = PORT_PCR_MUX(0x01) | PORT_PCR_DSE_MASK | PORT_PCR_PE_MASK;                                   
  PORTD_PCR2 = PORT_PCR_MUX(0x01) | PORT_PCR_DSE_MASK | PORT_PCR_PE_MASK;                                   
  PORTD_PCR5 = PORT_PCR_MUX(0x02) | PORT_PCR_DSE_MASK | PORT_PCR_PE_MASK;                                   
  PORTB_PCR17 = PORT_PCR_MUX(0x05) | PORT_PCR_DSE_MASK | PORT_PCR_PE_MASK;                                   
  PORTD_PCR7 = PORT_PCR_MUX(0x02) | PORT_PCR_DSE_MASK | PORT_PCR_PE_MASK;  

	GPIOD_PDDR |= 1 << 2;	
	GPIOA_PDDR |= 1 << 2;	
	
	HAL_SPI_CS_DEASSERT;
	HAL_SPI_ACCEL_CS_DEASSERT;
	
  /* SPI1_C1: SPIE=0,SPE=0,SPTIE=0,MSTR=1,CPOL=1,CPHA=1,SSOE=1,LSBFE=0 */
	SPI1_C1 &= ~SPI_C1_SPE_MASK; 
  SPI1_C1 = SPI_C1_MSTR_MASK;          /* Set configuration register */
  /* SPI1_BR: ??=0,SPPR=2,SPR=5 */
  SPI1_BR = (SPI_BR_SPPR(0x02) | SPI_BR_SPR(0x01)); /* Set baud rate register */
  /* SPI1_C1: SPE=1 */
  SPI1_C1 |= SPI_C1_SPE_MASK;          /* Enable SPI module */
}

uint8_t halSpiWriteByte(uint8_t data)
{
	uint8_t rc;
	uint32_t timeOut = SPI_TIMEOUT;
	while(!(SPI1_S & SPI_S_SPTEF_MASK))
	{
			if(timeOut-- == 0) return 0xff;
	}
	/* Write in the DR register the data to be sent */
	SPI1_D = data;
	while(!(SPI1_S & SPI_S_SPRF_MASK))
	{
			if(timeOut-- == 0) return 0xff;
	}
	rc = SPI1_D;;
	return rc;
}



