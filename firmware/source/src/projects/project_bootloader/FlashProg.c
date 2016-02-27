/* 
 *
 * Copyright (C) AMPM ELECTRONICS EQUIPMENT TRADING COMPANY LIMITED.,
 * Email: thienhaiblue@ampm.com.vn or thienhaiblue@mail.com
 *
 * This file is part of ampm_open_lib
 *
 * ampm_open_lib is free software; you can redistribute it and/or modify.
 *
 * Add: 143/36/10 , Lien khu 5-6 street, Binh Hung Hoa B Ward, Binh Tan District , HCM City,Vietnam
 */
/*---------------------------------------------------------------------------------------------------------*/
/* Includes of system headers                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#include "common.h"
#include "fmc.h"
#include "flashprog.h"
#include "flash_FTFA.h"
/*---------------------------------------------------------------------------------------------------------*/
/* Macro, type and constant definitions                                                                    */
/*---------------------------------------------------------------------------------------------------------*/

void myFMC_Erase(uint32_t u32addr)
{
	__disable_irq();
	if(Flash_SectorErase(u32addr) != Flash_OK)
		NVIC_SystemReset();
	__enable_irq();
}

void FMC_ProgramPage(uint32_t u32startAddr, uint32_t * u32buff)
{
	uint32_t i;
		__disable_irq();
		if(Flash_ByteProgram(u32startAddr,u32buff,PAGE_SIZE) != Flash_OK)
			NVIC_SystemReset();
		for (i = 0; i < PAGE_SIZE/4; i++)
    {
			if(u32buff[i] != *(uint32_t*)(u32startAddr + i*4)) //check wrote data
			{
				NVIC_SystemReset();
			}
    }
		__enable_irq();
}



// void myFMC_Erase(uint32_t u32addr)
// {
// 	uint16_t  FlashStatus;
// 	__disable_irq();
// 	FlashStatus = FLASH_ErasePage(u32addr);
// 	if(FLASH_COMPLETE != FlashStatus) 
// 	{
// 		NVIC_SystemReset();
// 	}
// 	__enable_irq();
// }

// void FMC_ProgramPage(uint32_t u32startAddr, uint32_t * u32buff)
// {
// 		uint16_t  FlashStatus;
//     uint32_t i;
// 		__disable_irq();
//     for (i = 0; i < PAGE_SIZE/4; i++)
//     {
// 			FlashStatus = FLASH_ProgramWord(u32startAddr + i*4, u32buff[i]);
// 			if(FLASH_COMPLETE != FlashStatus) 
// 			{
// 				NVIC_SystemReset();
// 			}
// 			if(u32buff[i] != *(uint32_t*)(u32startAddr + i*4)) //check wrote data
// 			{
// 				NVIC_SystemReset();
// 			}
//     }
// 		__enable_irq();
// }


