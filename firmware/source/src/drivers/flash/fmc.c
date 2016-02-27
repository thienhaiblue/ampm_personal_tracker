

/**
* \file
*		Flash Memory Controler
* \author
*		Hai Nguyen Van <thienhaiblue@gmail.com>
*/

#include "common.h"
#include "fmc.h"


#define PROGRAM_TIMEOUT ((uint32_t)0x00002000)

FLASH_Status FLASH_WaitForLastOperation(uint32_t Timeout)
{ 
  FLASH_Status status = FLASH_COMPLETE;
	uint8_t u8temp;
	/* Check for the Flash Status */
	u8temp = FTFA->FSTAT;
  status = (FLASH_Status)((u8temp & FTFA_FSTAT_CCIF_MASK) >> FTFA_FSTAT_CCIF_SHIFT);
  /* Wait for a Flash operation to complete or a TIMEOUT to occur */
  while((status == FLASH_BUSY) && (Timeout != 0x00))
  {
    /* Check for the Flash Status */
		u8temp = FTFA->FSTAT;
		status = (FLASH_Status)((u8temp & FTFA_FSTAT_CCIF_MASK) >> FTFA_FSTAT_CCIF_SHIFT);
    Timeout--;
  }
	
	if(FTFA->FSTAT & FTFA_FSTAT_FPVIOL_MASK) 
		FTFA->FSTAT = FTFA_FSTAT_FPVIOL_MASK; //Clear Flash Protection Violation Flag
	
	if(FTFA->FSTAT & FTFA_FSTAT_ACCERR_MASK)
		FTFA->FSTAT = FTFA_FSTAT_ACCERR_MASK; //Clear Flash Access Error Flag
	
  if(Timeout == 0x00 )
  {
    status = FLASH_TIMEOUT;
  }
  /* Return the operation status */
  return status;
}

FLASH_Status FLASH_ErasePage(uint32_t Page_Address)
{
  FLASH_Status status = FLASH_COMPLETE;
	uint8_t *u8pt;
	uint16_t i = 0xffff;
	/* Wait for last operation to be completed */
	while(i--);
  status = FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);
	if(status == FLASH_COMPLETE)
  {
		FTFA->FCCOB0 = 0x09; //Program longword command
		u8pt = (uint8_t *)&Page_Address;
		FTFA->FCCOB1 = u8pt[2];
		FTFA->FCCOB2 = u8pt[1];
		FTFA->FCCOB3 = u8pt[0];
		
		FTFA->FSTAT = FTFA_FSTAT_CCIF_MASK; //Run command
		i = 0xffff;
		while(i--);
		status = FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);
	}
	return status;
}


FLASH_Status FLASH_ProgramWord(uint32_t Address, uint32_t Data)
{
	FLASH_Status status = FLASH_COMPLETE;
	uint8_t *u8pt;
	uint32_t i;
	i = 1000;
	while(i--);
	/* Wait for last operation to be completed */
  status = FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);
	if(status == FLASH_COMPLETE)
  {
		
		FTFA->FCCOB0 = 0x06; //Program longword command
		u8pt = (uint8_t *)&Address;
		FTFA->FCCOB1 = u8pt[2];
		FTFA->FCCOB2 = u8pt[1];
		FTFA->FCCOB3 = u8pt[0];
		
		u8pt = (uint8_t *)&Data;
		FTFA->FCCOB4 = u8pt[3];
		FTFA->FCCOB5 = u8pt[2];
		FTFA->FCCOB6 = u8pt[1];
		FTFA->FCCOB7 = u8pt[0];
		
		FTFA->FSTAT = FTFA_FSTAT_CCIF_MASK; //Run command
		i = 1000;
		while(i--);
		status = FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);
	}
	return status;
}
	



