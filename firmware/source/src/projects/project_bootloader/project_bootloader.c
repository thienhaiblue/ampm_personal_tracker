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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "fmc.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "hiduser.h"
#include "cdcuser.h"
#include "spi.h"
#include "sst25.h"
#ifdef CMSIS
#include "start.h"
#endif
#include "flashprog.h"
#include "sysinit.h"
#include "hw_config.h"
#include "flash_FTFA.h"
#include "llwu.h"

#define USER_FLASH_START		0x00004000
#define PAGE_SIZE         1024

uint8_t USB_rxBuff[64];


#define LPTMR_USE_IRCLK 0 
#define LPTMR_USE_LPOCLK 1
#define LPTMR_USE_ERCLK32 2
#define LPTMR_USE_OSCERCLK 3

uint8_t buff[1024];
uint8_t flashBuff[4096];
uint32_t fileSize;
uint32_t packetNo;
uint32_t firmwareFileOffSet;
uint32_t firmwareFileSize;
uint8_t USB_txBuff[64];
uint8_t usbRecv = 0;

// uint32_t  DbgCfgPrintf(const uint8_t *format, ...)
// {
// 	static  uint8_t  buffer[512];
// 	uint32_t len,i;
// 	__va_list     vArgs;
// 	va_start(vArgs, format);
// 	len = vsprintf((char *)&buffer[0], (char const *)format, vArgs);
// 	va_end(vArgs);
// 	if(len >= 255) len = 255;
// 	for(i = 0;i < len; i++)
// 	{
// 			RINGBUF_Put(&VCOM_TxRingBuff,  buffer[i]);
// 	}
// 	return 0;
// }
void HardFault_Handler(void)
{
	NVIC_SystemReset();
	//while(1);
}

void SetOutReport (void)
{
	usbRecv = 1;
}

void GetInReport  (void)
{

}

uint8_t CfgCalcCheckSum(uint8_t *buff, uint32_t length)
{
	uint32_t i;
	uint8_t crc = 0;
	for(i = 0;i < length; i++)
	{
		crc += buff[i];
	}
	return crc;
}

typedef  void (*pFunction)(void);
pFunction Jump_To_Application;
uint32_t JumpAddress;

void execute_user_code(void)
{
	uint32_t i = 15000000;
	
	if (((*(uint32_t*)USER_FLASH_START) != 0xFFFFFFFF ))
	{ 
		USB_Connect(FALSE);    // USB Connect
		while(i--);
		
		/* Jump to user application */
		SCB->VTOR = USER_FLASH_START;
		JumpAddress = *(uint32_t*) (USER_FLASH_START + 4);
		Jump_To_Application = (pFunction) JumpAddress;
		/* Initialize user application's Stack Pointer */
		__set_MSP(*(uint32_t*) USER_FLASH_START);
		Jump_To_Application();
	}
}

void load_boot_code(void)
{
			uint32_t bootAddr = 0;
			SCB->VTOR = 0;
      JumpAddress = *(uint32_t*) (bootAddr + 4);
      Jump_To_Application = (pFunction) JumpAddress;
      /* Initialize user application's Stack Pointer */
      __set_MSP(*(uint32_t*) bootAddr);
      Jump_To_Application();  
}

void LPTMR_init(int count, int clock_source)
{
    SIM_SCGC5 |= SIM_SCGC5_LPTMR_MASK;
		NVIC_SetPriority (LPTimer_IRQn, (1<< 0) - 1);
    enable_irq(28);

    LPTMR0_PSR = ( LPTMR_PSR_PRESCALE(0) // 0000 is div 2
                 | LPTMR_PSR_PBYP_MASK  // LPO feeds directly to LPT
                 | LPTMR_PSR_PCS(clock_source)) ; // use the choice of clock
             
    LPTMR0_CMR = LPTMR_CMR_COMPARE(count);  //Set compare value

    LPTMR0_CSR =(  LPTMR_CSR_TCF_MASK   // Clear any pending interrupt
                 | LPTMR_CSR_TIE_MASK   // LPT interrupt enabled
                 | LPTMR_CSR_TPS(0)     //TMR pin select
                 |!LPTMR_CSR_TPP_MASK   //TMR Pin polarity
                 |!LPTMR_CSR_TFC_MASK   // Timer Free running counter is reset whenever TMR counter equals compare
                 |!LPTMR_CSR_TMS_MASK   //LPTMR0 as Timer
                );
    LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;   //Turn on LPT and start counting
}

uint32_t pwrCnt =0;
void LPTimerTask(void)
{
	uint32_t u32Temp;
	SIM_SCGC5 |= SIM_SCGC5_LPTMR_MASK;
  LPTMR0_CSR |=  LPTMR_CSR_TCF_MASK;   // write 1 to TCF to clear the LPT timer compare flag
  LPTMR0_CSR = ( LPTMR_CSR_TEN_MASK | LPTMR_CSR_TIE_MASK | LPTMR_CSR_TCF_MASK  );
	if(PWR_KEY_IN)
		pwrCnt = 0;
	else if(SOS_KEY_IN)
	{
		pwrCnt++;
		if(pwrCnt >= 4)
		{
			NVIC_SystemReset();
		}
	}
	
	
}

void LPTimer_IRQHandler (void)
{ 
		LPTimerTask();
}



void SysInit(void)
{
	sysinit();
	SST25_Init();
	LPTMR_init(1000,LPTMR_USE_LPOCLK);
/*USB Init*/
 	USB_Init();        	  // USB Initialization
  USB_Connect(TRUE);    // USB Connect
	__enable_irq();
}


void Delay(uint32_t i)
{
	while(i--);
}


/********************************************************************/
int main (void)
{   
	uint32_t *u32Pt = (uint32_t *)buff;
	uint8_t *u8pt,u8temp,c;
	int32_t i = 10000000;
	uint32_t u32temp;
	uint8_t mainBuf[32];
	RINGBUF_Init(&VCOM_TxRingBuff,VCOM_TxBuff,64);
	RINGBUF_Init(&VCOM_RxRingBuff,VCOM_RxBuff,64);
	
	SysInit();
	
 	PWR_EN_INIT;
 	PWR_EN_CLR;
 	SOS_KEY_INIT;
 	PWR_KEY_INIT;
 	if(SOS_KEY_IN || PWR_KEY_IN)
 		execute_user_code();
	PWR_EN_SET;
	while(i && (USB_Configuration == 0))
	{
		i--;
	}
	Flash_Init(59);
	while(USB_Configuration)
	{	
			PWR_EN_SET;
			VCOM_CheckSerialState();
			if (CDC_DepInEmpty)
				if(RINGBUF_Get(&VCOM_TxRingBuff,&c) == 0)
				{
					VCOM_Serial2Usb(&c);                      // read serial port and initiate USB event
				}
		 if((usbRecv))
		 {
					usbRecv = 0;
					USB_ReadEP(0x03,USB_rxBuff);
					u8temp = CfgCalcCheckSum((uint8_t *)&USB_rxBuff[4],USB_rxBuff[1]);
					if((USB_rxBuff[0] == 0xCA) && (u8temp == USB_rxBuff[USB_rxBuff[1] + 4]))
					{
						for(i = 0;i < 64;i++)
							USB_txBuff[i] = 0;
						switch(USB_rxBuff[3])
						{
							case 0x12:
								u32Pt = (uint32_t *)&USB_rxBuff[4];
								packetNo = *u32Pt;
								if(packetNo == 0xA5A5A5A5)
								{
									firmwareFileOffSet = 0;
									u32Pt = (uint32_t *)&USB_rxBuff[8];
									firmwareFileSize = *u32Pt;
									USB_txBuff[1] = 12; //length
									USB_txBuff[3] = 0x12; //opcode
									
									USB_txBuff[4] = USB_rxBuff[4];
									USB_txBuff[5] = USB_rxBuff[5];
									USB_txBuff[6] = USB_rxBuff[6];
									USB_txBuff[7] = USB_rxBuff[7];
									//Max file size
									USB_txBuff[8] = USB_rxBuff[8];
									USB_txBuff[9] = USB_rxBuff[9];
									USB_txBuff[10] = USB_rxBuff[10];
									USB_txBuff[11] = USB_rxBuff[11];
									//Max packet size
									USB_txBuff[12] = 32; //max packet size
								}
								else
								{
									if(packetNo == firmwareFileOffSet)
									{
										firmwareFileOffSet += (USB_rxBuff[1] - 4);
										u32temp = firmwareFileOffSet % PAGE_SIZE;
										for(i = 0;i < (USB_rxBuff[1] - 4);i++)
										{
											flashBuff[(packetNo % PAGE_SIZE) + i] = USB_rxBuff[i + 8];
										}						
										if(u32temp == 0)
										{
											myFMC_Erase(USER_FLASH_START + firmwareFileOffSet - PAGE_SIZE);
											u32Pt = (uint32_t *)flashBuff;
											FMC_ProgramPage(USER_FLASH_START + firmwareFileOffSet - PAGE_SIZE,u32Pt);
											for(i = 0;i < PAGE_SIZE; i++)
												flashBuff[i] = 0xff;
										}
										else if(firmwareFileOffSet >= firmwareFileSize)
										{
											i = (firmwareFileOffSet / PAGE_SIZE)*PAGE_SIZE;
											myFMC_Erase(USER_FLASH_START + i);
											u32Pt = (uint32_t *)flashBuff;
											FMC_ProgramPage(USER_FLASH_START + i,u32Pt);
										}
									}
									packetNo = firmwareFileOffSet;
									if(firmwareFileOffSet >= firmwareFileSize)
									{
										firmwareFileOffSet = 0;
										packetNo = 0x5A5A5A5A;
									}
									u8pt = (uint8_t *)&packetNo;
									USB_txBuff[1] = 4; //length
									USB_txBuff[3] = 0x12; //opcode
									USB_txBuff[4] = u8pt[0];
									USB_txBuff[5] = u8pt[1];
									USB_txBuff[6] = u8pt[2];
									USB_txBuff[7] = u8pt[3];
								}
							break;
							case 0xA5:
								execute_user_code();
							break;
						default:
							USB_txBuff[1] = 4; //length
							USB_txBuff[3] = 0x33; //opcode
							USB_txBuff[4] = 0xA5;
							USB_txBuff[5] = 0xA5;
							USB_txBuff[6] = 0xA5;
							USB_txBuff[7] = 0xA5;
							break;
					}
					USB_txBuff[0] = 0xCA;
					USB_txBuff[USB_txBuff[1] + 4] = CfgCalcCheckSum((uint8_t *)&USB_txBuff[4],USB_txBuff[1]);
					USB_WriteEP (0x83,USB_txBuff,64);
					}
				USB_rxBuff[0] = 0;
		 }
	}
	while(1)
	{
		execute_user_code();
	}
}
/********************************************************************/
