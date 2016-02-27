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
#include <MKL25Z4.H>
#include "common.h"
#include "start.h"
#include "DTMF.h"                 // global definitions
#include "stdio.h"
#include "stdint.h"
#include "ringbuf.h"
#include "sysinit.h"

RINGBUF DTMF_ringBuff;
uint8_t DTMF_buf[32];

/*  DTMF Digit encoding */
static char DTMFchar[16] = {
  '1', '2', '3', 'A', 
  '4', '5', '6', 'B', 
  '7', '8', '9', 'C', 
  '*', '0', '#', 'D', 
};

struct DTMF dail1 = { 0, DTMF_DATA_DEEP, 0 };  // DTMF info of one input

uint8_t DEMF_Enable = 0;

void TPM2_IRQHandler(void)
{
	int val;
	TPM2_SC |= 1 << 7; // clear interrup
	if ((ADC0_SC1A & ADC_SC1_COCO_MASK) != 0)
 	{
		val = ADC0_R(0);
		ADC0_SC1A = 0x00; // init ADC conversion
 	}
	dail1.AInput[dail1.AIindex & (DTMFsz-1)] = (val);
	dail1.AIindex++;
}

void ADC0_Init(void)
{
	SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
	PORTE_PCR20 = PORT_PCR_MUX(0);
	//PORTE_PCR21 = PORT_PCR_MUX(0);
	ADC0_CFG1 = ADC_CFG1_MODE(1);
	ADC0_SC2 = 0;
	ADC0_SC1A = 0x00; // init ADC conversion
}

void ADC0_DeInit(void)
{
	SIM_SCGC6 &= ~SIM_SCGC6_ADC0_MASK;
}

void TPM2_Init(uint32_t clock/*Hz*/,uint32_t freq/*Hz*/)
{
	SIM_SCGC6 |= SIM_SCGC6_TPM2_MASK;
	TPM2_SC = 0; // disable timer
	TPM2_SC |= 1 << 7; // clear interrupt, prescaler divided by 1;
	TPM2_CNT = 0;
	TPM2_MOD = clock/freq; // 8000Hz
	TPM2_SC |= 1 << 6; //enable timer overflow interrupt
	TPM2_CONF = TPM_CONF_TRGSEL(8);
	NVIC_SetPriority (TPM2_IRQn, (1<< 0) - 0);
	NVIC_EnableIRQ(TPM2_IRQn);
	TPM2_SC |= TPM_SC_CMOD(1); // enable timer
}

void TPM2_DeInit(void)
{
	SIM_SCGC6 &= ~SIM_SCGC6_TPM2_MASK;
	NVIC_DisableIRQ(TPM2_IRQn);
}


void DTMF_DeInit(void)
{
	DEMF_Enable = 0;
	TPM2_DeInit();
	ADC0_DeInit();
}

void DTMF_Init(void)
{
	RINGBUF_Init(&DTMF_ringBuff,DTMF_buf,sizeof(DTMF_buf));
	ADC0_Init();
	TPM2_Init(mcg_clk_hz,8000);
	DEMF_Enable = 1;
}


char DTMF_Task(void)
{
	char c;
	if(DEMF_Enable)
	{
		if (dail1.AIindex >= dail1.AIcheck)  
		{
			DTMF_Detect (&dail1);
		}
		if (dail1.new)  
		{
			c = DTMFchar[dail1.digit & 0x0F];
			RINGBUF_Put(&DTMF_ringBuff,c);
			dail1.new = 0;                              // digit taken
			return c;		
		}
	}
	return 0;
}
