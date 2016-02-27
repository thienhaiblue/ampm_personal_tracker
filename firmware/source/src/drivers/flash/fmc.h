


#ifndef __FMC_H__
#define __FMC_H__
#include <stdint.h>

typedef enum
{ 
  FLASH_BUSY = 0,
	FLASH_COMPLETE,
  FLASH_ERROR_PG,
  FLASH_ERROR_WRP,
  FLASH_TIMEOUT
}FLASH_Status;

FLASH_Status FLASH_ErasePage(uint32_t Page_Address);
FLASH_Status FLASH_ProgramWord(uint32_t Address, uint32_t Data);

#endif
