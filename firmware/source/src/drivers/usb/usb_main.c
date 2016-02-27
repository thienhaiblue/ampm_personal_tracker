/*
 * File:		usb_main.c
 * Purpose:		Main process
 *
 */

/* Includes */
#include "start.h"
#include "common.h"
#include "uart.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "cdc.h"
#include "cdcuser.h"
#include "tick.h"
#include "dbg_cfg_app.h"
#include "system_config.h"
#include "spi.h"
#include "sst25.h"



uint8_t UART2_2_USB_Buff[128];
RINGBUF UART2_2_USB_RingBuff;

uint8_t USB_2_UART2_Buff[128];
RINGBUF USB_2_UART2_RingBuff;

uint8_t wBuf[128] = "123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 1234567";
uint8_t rBuf[128];

/********************************************************************/
int main (void)
{ uint32_t i;
		uint8_t temp;
		#ifdef CMSIS  // If we are conforming to CMSIS, we need to call start here
		start();
		#endif
		CFG_Load();
		while(1);
		uart0_init(UART0_BASE_PTR,SystemCoreClock / 2 / 1000,9600);
		uart_init(UART2_BASE_PTR,SystemCoreClock,115200);
	
		SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;
		PORTC_PCR4 = PORT_PCR_MUX(1);
		PORTC_PCR5 = PORT_PCR_MUX(1);
		PORTC_PCR6 = PORT_PCR_MUX(1);
		GPIOC_PDDR = (1 << 4) | (1 << 5) | (1 << 6);
		GPIOC_PCOR = (1 << 4) | (1 << 5) | (1 << 6);
	
		SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;
		PORTB_PCR2 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x1);
		GPIOB_PDDR |= (1 << 2);
		GPIOB_PSOR = (1 << 2);
	
		PORTB_PCR3 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x1);
		GPIOB_PDDR |= (1 << 3);
		GPIOB_PSOR = (1 << 3);
	
		
		CFG_Load();
		TICK_Init(1);
		TPM0_Init();
		SST25_Init();

		SST25_Write(0,wBuf,sizeof(wBuf));
		SST25_Read(0,rBuf,sizeof(rBuf));

		RINGBUF_Init(&UART2_2_USB_RingBuff,UART2_2_USB_Buff,sizeof(UART2_2_USB_Buff));
		RINGBUF_Init(&USB_2_UART2_RingBuff,USB_2_UART2_Buff,sizeof(USB_2_UART2_Buff));

		USB_Init();                               // USB Initialization
		USB_Connect(TRUE);                        // USB Connect
	
		while(1);
}

/*******************************************************************/

