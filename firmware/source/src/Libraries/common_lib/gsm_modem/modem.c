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
#include "modem.h"
#include "gps.h"
#include "uart.h"
#include "at_command_parser.h"
#include "hw_config.h"
#include "sysinit.h"
#include "power_management.h"
#include "string_parser.h"
#include "audio.h"
#include "gsm_gprs_tasks.h"

#define GSM_RING_PIN		1<<8
#define GSM_RING_PORT		2


#define MODEM_RI()				

#define MODEM_DCD()					//((GPIO_ReadValue(2) >> 3) & (1))

#define MODEM_Status()		

#define CRITICAL_CMD_TIME_OUT 120		// 120s

const uint8_t *modemOk = "OK";
const uint8_t *modemError = "ERROR";

uint8_t modemId[16];
volatile uint8_t modemRinging = 0;
volatile int8_t  modemStarted = 0;
uint8_t csqBuff[16];
float csqValue = 0;
uint8_t gsmSignal = 0;
uint32_t batteryPercent = 100;
uint32_t agpsRestartCnt = 0;

void MODEM_FlushBuffer(void);
extern uint8_t mainBuf[];
uint8_t flagGsmStatus = GSM_NO_REG_STATUS;
uint8_t criticalCmd = 0;

uint8_t ModemCmdTask_IsCriticalCmd(void)
{
	return criticalCmd;
}

uint8_t MODEM_PowerOff(void)
{
	while(MODEM_Init());
	if(MODEM_SendCommand(modemOk,modemError,1000,3,"AT+CPWROFF\r")) return 0xff; //Power off modem
	
	return 0;
	
}
uint8_t MODEM_Init(void)
{
//	uint8_t lpwModeTemp;
	GSM_POWER_PIN_SET_OUTPUT;
	GSM_RESET_PIN_SET_OUTPUT;
	GSM_RTS_PIN_SET_OUTPUT;
	GSM_RTS_PIN_CLR;			
	GSM_RESET_PIN_SET; 
	GSM_POWER_PIN_SET;    // Turn on GSM
	DelayMs(5);           // Delay 5ms
	GSM_POWER_PIN_CLR;    // Turn off GSM
	DelayMs(200);         // Delay 200ms
	GSM_POWER_PIN_SET;    // Turn on GSM
	// ---------- Turn on GSM module ----------
	GSM_RESET_PIN_CLR;    // Reset GSM
	DelayMs(200);         // Delay 200ms
	GSM_RESET_PIN_SET;    // Start GSM module (Release reset)  
//	if(lpwMode != LPW_NOMAL)
//	{
//		SetLowPowerModes(LPW_NOMAL);
//	}
	COMM_Open(UART1_BASE_PTR, periph_clk_khz, 115200);
	MODEM_FlushBuffer();
	if(MODEM_SendCommand(modemOk,modemError,1000,2,"AT\r")) return 0xff;
	MODEM_SendCommand(modemOk,modemError,1000,2,"AT+CBC\r"); //Batery check status
	return 0;
}

uint8_t MODEM_SendCommand(const uint8_t *resOk,const uint8_t *resFails,uint32_t timeout,uint8_t tryAgainNum,const uint8_t *format, ...)
{
	static  uint8_t  buffer[128];
	COMPARE_TYPE cmp1,cmp2;
	Timeout_Type tOut;
	uint8_t c;
	__va_list     vArgs;
	
	va_start(vArgs, format);
	vsprintf((char *)&buffer[0], (char const *)format, vArgs);
	va_end(vArgs);
	COMM_Putc(0x1A);
	COMM_Putc(0x1A);
	if (tryAgainNum == 0)
	{
		tryAgainNum = 1;
	}
	while (tryAgainNum)
	{
		DelayMs(100);
		InitFindData(&cmp1, (uint8_t *)resOk);
		InitFindData(&cmp2, (uint8_t *)resFails);
		InitTimeout(&tOut, TIME_MS(timeout));
		MODEM_FlushBuffer();
		MODEM_Info("\r\nGSM->%s\r\n", buffer);
		COMM_Puts(buffer);
		while(CheckTimeout(&tOut))
		{
			if(RINGBUF_Get(&commBuf, &c) == 0)
			{
				if(FindData(&cmp1, c) == 0)
				{
					MODEM_Info("\r\nGSM->%s OK\r\n",buffer);
					return 0;
				}
				if(FindData(&cmp2, c) == 0)
				{
					break;
				}
			}
		}
		tryAgainNum--;
	}
	MODEM_Info("\r\nGSM->%s FAILS\r\n", buffer);

	return 0xff;
}

MODEM_CMD_PHASE_TYPE modemCmd_TaskPhase = MODEM_CMD_FINISH;
uint8_t  modemAtCmdBuf[128];
COMPARE_TYPE modemCmpOk, modemCmpFails;
Timeout_Type tModemAtTimeout;
Timeout_Type tModemCancelTimeout;
char modemResOkBuf[32], modemResFailsBuf[32];
uint8_t cmdRetry = 0;
uint32_t cmdTimeout = 0;
uint32_t cmdDelay = 0;
uint8_t cmdTimeoutCount = 0;
uint8_t modemCancel = 0;

void ModemCmdTask_SetCriticalCmd(void)
{
	if (modemCmd_TaskPhase == MODEM_CMD_SEND)
	{
		criticalCmd = 1;
	}
}

uint8_t ModemCmdTask_SendCmd(uint32_t delay, const uint8_t *resOk, const uint8_t *resFails, uint32_t timeout, uint8_t tryAgainNum, const uint8_t *format, ...)
{
	__va_list     vArgs;
	
	if(ModemCmdTask_IsIdle())
	{
		if(format != NULL)
		{
			va_start(vArgs, format);
			vsprintf((char *)&modemAtCmdBuf[0], (char const *)format, vArgs);
			va_end(vArgs);
		}
		else
		{
			modemAtCmdBuf[0] = 0;
		}
		strcpy(modemResOkBuf, (const char  *)resOk);
		strcpy(modemResFailsBuf, (const char  *)resFails);
		InitFindData(&modemCmpOk, (uint8_t *)modemResOkBuf);
		InitFindData(&modemCmpFails, (uint8_t *)modemResFailsBuf);
		cmdDelay = delay;
		InitTimeout(&tModemAtTimeout, TIME_MS(cmdDelay));
		cmdTimeout = timeout;
		cmdRetry  = tryAgainNum;
		modemCmd_TaskPhase = MODEM_CMD_SEND;
		MODEM_FlushBuffer();
		
		return 0;
	}
	
	return 0xff;
}

MODEM_CMD_PHASE_TYPE ModemCmdTask_GetPhase(void)
{
	return modemCmd_TaskPhase;
}

uint8_t ModemCmdTask_IsIdle(void)
{
	switch(modemCmd_TaskPhase)
	{
		case MODEM_CMD_ERROR:
		case MODEM_CMD_OK:
		case MODEM_CMD_FINISH:
			return 1;
		default:
			return 0;
	}
}

void ModemCmdTask_Reset(void)
{
	modemCmd_TaskPhase = MODEM_CMD_FINISH;
}

void ModemCmdTask_SetWait(const uint8_t *resOk, const uint8_t *resFails, uint32_t timeout)
{
	if(ModemCmdTask_IsIdle())
	{
		strcpy(modemResOkBuf, (const char  *)resOk);
		strcpy(modemResFailsBuf, (const char  *)resFails);
		InitFindData(&modemCmpOk, (uint8_t *)modemResOkBuf);
		InitFindData(&modemCmpFails, (uint8_t *)modemResFailsBuf);
		cmdRetry = 0;
		InitTimeout(&tModemAtTimeout, TIME_MS(timeout));
		modemCmd_TaskPhase = MODEM_CMD_WAIT;
	}
}

void ModemCmdTask_Cancel(void)
{
	modemCancel = 1;
	InitTimeout(&tModemCancelTimeout, TIME_SEC(10));
	while ((modemCmd_TaskPhase == MODEM_CMD_WAIT) && (CheckTimeout(&tModemCancelTimeout) != TIMEOUT))
	{
		COMM_Putc(0x1B);
		ModemCmd_Task();
	}
	modemCancel = 0;
}

void ModemCmd_Task(void)
{
	uint8_t c;
	
// 	if ((cmdRetry >= 30) || (CheckTimeout(&tModemAtTimeout) >= 60000))
// 	{
// 		cmdRetry = 0;
// 		modemCmd_TaskPhase = MODEM_CMD_ERROR;
// 	}
	
	switch(modemCmd_TaskPhase)
	{
		case MODEM_CMD_SEND:
			if ((criticalCmd == 0) || (ringingDetectFlag == 0) && (ringingFlag == 0))
			{
				if(CheckTimeout(&tModemAtTimeout) == TIMEOUT)
				{
					if (modemAtCmdBuf[0])
					{
						MODEM_FlushBuffer();
						COMM_Puts(modemAtCmdBuf);
					}
					if (criticalCmd)
					{
						InitTimeout(&tModemAtTimeout, TIME_SEC(CRITICAL_CMD_TIME_OUT));
					}
					else
					{
						InitTimeout(&tModemAtTimeout, TIME_MS(cmdTimeout));
					}
					modemCmd_TaskPhase = MODEM_CMD_WAIT;
				}
			}
			else
			{
				modemCmd_TaskPhase = MODEM_CMD_ERROR;
			}
			break;
			
		case MODEM_CMD_WAIT:
			if(CheckTimeout(&tModemAtTimeout) == TIMEOUT)
			{
				if (cmdRetry)
				{
					cmdRetry--;
					InitTimeout(&tModemAtTimeout, TIME_MS(cmdDelay));
					modemCmd_TaskPhase = MODEM_CMD_SEND;
				}
				else
				{
					criticalCmd = 0;
					modemCmd_TaskPhase = MODEM_CMD_ERROR;
					if (cmdTimeoutCount++ > 10)
					{
						cmdTimeoutCount = 0;
						GsmTask_Init();
					}
				}
			}
			else
			{			
				while (RINGBUF_Get(&commBuf, &c) == 0)
				{
					if ((c != 0x1A) && (c != 0x1B))
					{
						if (FindData(&modemCmpOk, c) == 0)
						{
							criticalCmd = 0;
							cmdTimeoutCount = 0;
							modemCmd_TaskPhase = MODEM_CMD_OK;
						}
						else if (FindData(&modemCmpFails, c) == 0)
						{
							cmdTimeoutCount = 0;
							if (cmdRetry)
							{
								cmdRetry--;
								InitTimeout(&tModemAtTimeout, TIME_MS(cmdDelay));
								modemCmd_TaskPhase = MODEM_CMD_SEND;
							}
							else
							{
								criticalCmd = 0;
								modemCmd_TaskPhase = MODEM_CMD_ERROR;
							}
						}
					}
					else if (modemCancel)
					{
						modemCancel = 0;
						modemCmd_TaskPhase = MODEM_CMD_FINISH;
						break;
					}						
				}
			}
			break;
			
		case MODEM_CMD_ERROR:		
		case MODEM_CMD_OK:
		case MODEM_CMD_FINISH:
			criticalCmd = 0;
			break;
		
		default:
			modemCmd_TaskPhase = MODEM_CMD_FINISH;
			break;
	}
}

// uint8_t MODEM_GprsSendData(uint8_t socket,uint8_t *data,uint32_t length)
// {
// 	COMPARE_TYPE cmp1;
// 	Timeout_Type tOut;
// 	uint8_t buf[32],c;
// 	uint32_t i;
// 	uint32_t tryNum = 1;
// 	MODEM_FlushBuffer();
// 	sprintf((char *)buf, "AT+USOWR=%d,%d\r",socketMask[socket],length);
// 	
// 	for(;;)
// 	{
// 		DelayMs(100);
// 		COMM_Puts(buf);
// 		InitTimeout(&tOut,TIME_MS(5000));
// 		InitFindData(&cmp1,"@");
// 		while(CheckTimeout(&tOut))
// 		{
// 			if(RINGBUF_Get(&commBuf,&c) == 0)
// 			{
// 				if(FindData(&cmp1,c) == 0)
// 				{
// 					break;
// 				}
// 			}
// 		}
// 		if(CheckTimeout(&tOut) == TIMEOUT)
// 		{
// 			tryNum --;
// 			if(tryNum == 0) 
// 			{
// 				tcpSocketStatus[socket] = SOCKET_CLOSE;
// 				return 0xff;
// 			}
// 			continue;
// 		}
// 		i = length;
// 		for(i = 0; i < length ;i++)
// 		{
// 			COMM_Putc(data[i]);
// 		}
// 		InitTimeout(&tOut,TIME_MS(5000));
// 		InitFindData(&cmp1,"OK");
// 		while(CheckTimeout(&tOut))
// 		{
// 			if(RINGBUF_Get(&commBuf,&c) == 0)
// 			{
// 				if(FindData(&cmp1,c) == 0)
// 				{
// 					MODEM_Info("\r\nGSM->%s OK\r\n",buf);
// 					DelayMs(100);
// 					return 0;
// 				}
// 			}
// 		}
// 		MODEM_Info("\r\nGSM->%s FAILS\r\n",buf);
// 		tryNum --;
// 		if(tryNum == 0) 
// 		{
// 			tcpSocketStatus[socket] = SOCKET_CLOSE;
// 			return 0xff;
// 		}
// 	}
// }

// uint8_t MODEM_CreateSocket(uint8_t socket)
// {
// 	COMPARE_TYPE cmp1,cmp2;
// 	Timeout_Type tOut;
// 	uint8_t c;
// 	DelayMs(100);
// 	MODEM_FlushBuffer();
// 	socketMask[socket] = 0xff;
// 	createSocketNow = socket;
// 	MODEM_Info("\r\nGSM->CREATE SOCKET\r\n");
// 	COMM_Puts("AT+USOCR=6\r");
// 	InitFindData(&cmp1,"OK");
// 	InitFindData(&cmp2,"ERROR");
// 	InitTimeout(&tOut,TIME_MS(3000));
// 	while(CheckTimeout(&tOut))
// 	{
// 		if(RINGBUF_Get(&commBuf,&c) == 0)
// 		{
// 			if(FindData(&cmp1,c) == 0)
// 			{
// 				if((socketMask[socket] == 0xff)) 
// 				{
// 					break;
// 				}	
// 				MODEM_Info("\r\nGSM->CREATE SOCKET OK\r\n");
// 				return 0;
// 			}
// 			if(FindData(&cmp2,c) == 0)
// 			{
// 				break;
// 			}
// 		}
// 	}
// 	MODEM_Info("\r\nGSM->CREATE SOCKET FAILS\r\n");
// 	return 0xff;
// }

uint16_t MODEM_CheckGPRSDataOut(uint8_t socket)
{
	COMPARE_TYPE cmp1,cmp2;
	Timeout_Type tOut;
	uint8_t buf[32],c;
	DelayMs(100);
	MODEM_FlushBuffer();
	MODEM_Info("\r\nGSM->CHECK OUTCOMING UNACK SOCKET:%d\r\n",socket);
	sprintf((char *)buf, "AT+USOCTL=%d,11\r",socketMask[socket]);
	COMM_Puts(buf);
	InitFindData(&cmp1,"OK");
	InitFindData(&cmp2,"ERROR");
	InitTimeout(&tOut,TIME_MS(1000));
	while(CheckTimeout(&tOut))
	{
		if(RINGBUF_Get(&commBuf,&c) == 0)
		{
			if(FindData(&cmp1,c) == 0)
			{
				MODEM_Info("\r\nGPRS->UNACK:%dBytes\r\n",GPRS_dataUnackLength[socket]);
				return GPRS_dataUnackLength[socket];
			}
			if(FindData(&cmp2,c) == 0)
			{
				MODEM_Info("\r\nGPRS->ERROR\r\n");
				gprsDeactivationFlag = 1;
				break;
			}
		}
	}
	//num of data will check in at_command_parser.c 
	// return GPRS_dataUnackLength
	MODEM_Info("\r\nGPRS->UNACK:%dBytes\r\n",GPRS_dataUnackLength[socket]);
	return 0xffff;
}
uint8_t MODEM_CloseSocket(uint8_t socket)
{
	COMPARE_TYPE cmp1,cmp2;
	Timeout_Type tOut;
	uint8_t c;
	uint8_t buf[32];
	DelayMs(100);
	MODEM_FlushBuffer();
	MODEM_Info("\r\nGSM->CLOSE SOCKET:%d\r\n",socket);
	sprintf((char *)buf, "AT+USOCL=%d\r",socket);
	COMM_Puts(buf);
	InitFindData(&cmp1,"OK");
	InitFindData(&cmp2,"ERROR");
	InitTimeout(&tOut,TIME_MS(3000));
	while(CheckTimeout(&tOut))
	{
		if(RINGBUF_Get(&commBuf,&c) == 0)
		{
			if(FindData(&cmp1,c) == 0)
			{
				//MODEM_Info("\r\nGSM->CLOSE SOCKET OK\r\n");
				return 0;
			}
			if(FindData(&cmp2,c) == 0)
			{
				break;
			}
		}
	}
	//MODEM_Info("\r\nGSM->CLOSE SOCKET FAILS\r\n");
	return 0xff;
}
/*
Connect socket
AT+USOCO=0,"220.128.220.133",2020
*/
// uint8_t cellBuf[128];
// uint8_t GetCellLocate(uint8_t *buff)
// {
// 	Timeout_Type tOut;
// 	uint8_t c,i;
// 	uint8_t buf[32];
// 	DelayMs(100);
// 	MODEM_FlushBuffer();
// 	MODEM_Info("\r\nGSM->GET CELL LOCATE\r\n");
// 	sprintf((char *)buff, "AT+ULOC=2,2,1,120,1\r");
// 	COMM_Puts(buff);
// 	InitTimeout(&tOut,TIME_SEC(3));
// 	i = 0;
// 	while(CheckTimeout(&tOut))
// 	{
// 		if(RINGBUF_Get(&commBuf,&c) == 0)
// 		{
// 			buff[i++] = c;		
// 		}
// 	}
// 	MODEM_Info("\r\nGSM->%s\r\n",cellBuf);
// 	return 0xff;
// }

cellLocateType cellLocate;

/*
+UULOC: 27/09/2012,18:26:13.363,21.0213870,105.8091050,0,127,0,0 
nmea_scanf((char *)buff,128,"%d/%d/%d,%d:%d:%d.%d,%f,%f,%f,%f,%f,%f",&t1,&t2,&t3,&t4,&t5,&t6,&t7,
						cellLocateType.lat,cellLocateType.lon,cellLocateType.alt,cellLocateType.uncertainty,
						cellLocateType.speed,cellLocateType.dir;
					);
*/
uint32_t cellPass = 0;
//uint8_t GetCellLocate(uint8_t *buff,uint8_t waitEn)
//{
//	Timeout_Type tOut;
//	DelayMs(100);
//	MODEM_FlushBuffer();
//	MODEM_Info("\r\nGSM->GET CELL LOCATE\r\n");
//	sprintf((char *)buff, "AT+ULOC=2,2,1,500,1\r");
//	COMM_Puts(buff);
//	InitTimeout(&tOut,TIME_SEC(10));
//	cellPass = 0;
//	if(waitEn)
//		while(CheckTimeout(&tOut) && !cellPass);
//	return 0xff;
//}

uint8_t ParserCellLocate(void)
{
	cellLocateType cell;
	uint32_t t1,t2,t3,t4,t5,t6,t7;
	if(cellLocationFlag)
	{
		nmea_scanf((char *)cellLocationBuf,128,"%d/%d/%d,%d:%d:%d.%d,%f,%f,%f,%f",&t1,&t2,&t3,&t4,&t5,&t6,&t7,
		&cell.lat,&cell.lon,&cell.alt,&cell.uncertainty);
		cell.ew = 'E';
		cell.ns = 'N';
		if(cell.lat < 0) cell.ns = 'S';
		if(cell.lon < 0) cell.ew = 'W';
		cell.time.mday = t1;
		cell.time.month = t2;
		cell.time.year = t3;
		cell.time.hour = t4;
		cell.time.min = t5;
		cell.time.sec = t6;	
		cellLocationFlag = 0;
		cellPass = 1;
		if( (TIME_GetSec(&cell.time) >= rtcTimeSec)
			&& (cell.time.year >= 2013) 
			&& (cell.lat <= 90 || cell.lat >= -90) 
			&& (cell.lon <= 180  || cell.lon >= -180)
		)
		{
			if(cell.uncertainty <= 3000)
			{
				cellLocate = cell;
				return 1;
			}
			return 2;
		}
		return 0xff;
	}
	return 0;
}

uint8_t MODEM_DisconectSocket(uint8_t socket)
{
	return MODEM_CloseSocket(socketMask[socket]);
}

uint8_t MODEM_GetSocket(uint8_t socket)
{
	uint8_t i;
	socketMask[socket] = 0;
	for(i = 0; i < SOCKET_NUM;)
	{
		if(tcpSocketStatus[i] == SOCKET_OPEN)
		{
			if((socketMask[i] == socketMask[socket]) && (i != socket))
			{
				socketMask[socket]++;
				i = 0;
				continue;
			}
		}
		i++;
	}
	return socketMask[socket];
}

// uint8_t MODEM_ConnectSocket(uint8_t socket,uint8_t *ip,uint16_t port,uint8_t useIp)
// {
// 	COMPARE_TYPE cmp1,cmp2;
// 	Timeout_Type tOut;
// 	uint8_t c,i;
// 	uint8_t buf[32];
// 	DelayMs(100);
// 	MODEM_FlushBuffer();
// 	socketMask[socket] = 0;
// 	for(i = 0; i < SOCKET_NUM;)
// 	{
// 		if(tcpSocketStatus[i] == SOCKET_OPEN)
// 		{
// 			if((socketMask[i] == socketMask[socket]) && (i != socket))
// 			{
// 				socketMask[socket]++;
// 				i = 0;
// 				continue;
// 			}
// 		}
// 		i++;
// 	}
// 	MODEM_CloseSocket(socketMask[socket]);
// 	if(MODEM_CreateSocket(socket)) return 0xff;
// 	DelayMs(500);
// 	if(useIp == 0)
// 	{
// 		MODEM_Info("\r\nGSM->GET DOMAIN\r\n");
// 		sprintf((char *)buf, "AT+UDNSRN=0,\"%s\"\r",ip);
// 		COMM_Puts(buf);
// 		if(MODEM_CheckResponse("OK", 5000))
// 		{
// 			MODEM_Info("\r\nGSM->GET DOMAIN FAILS\r\n");
// 			return 0xff;
// 		}
// 		ip = DNS_serverIp;
// 	}
// 	MODEM_Info("\r\nGSM->IP:%s\r\n",ip);
// 	MODEM_Info("\r\nGSM->CONNECT SOCKET:%d:%s:%d\r\n",socketMask[socket],ip,port);
// 	sprintf((char *)buf, "AT+USOCO=%d,\"%s\",%d\r",socketMask[socket],ip,port);
// 	COMM_Puts(buf);
// 	InitFindData(&cmp1,"OK");
// 	InitFindData(&cmp2,"ERROR");
// 	InitTimeout(&tOut,TIME_SEC(10));
// 	while(CheckTimeout(&tOut))
// 	{
// 		if(RINGBUF_Get(&commBuf,&c) == 0)
// 		{
// 			if(FindData(&cmp1,c) == 0)
// 			{
// 				MODEM_Info("\r\nGSM->CONNECT SOCKET OK\r\n");
// 				return 0;
// 			}
// 			if(FindData(&cmp2,c) == 0)
// 			{
// 				MODEM_Info("\r\nGSM->CONNECT SOCKET ERROR\r\n");
// 				break;
// 			}
// 		}
// 	}
// 	MODEM_Info("\r\nGSM->CONNECT SOCKET FAILS\r\n");
// 	return 0xff;
// }

uint8_t VoiceSetup(void)
{
	AudioEnable();
	
	DelayMs(500);
	MODEM_SendCommand(modemOk,modemError,500,3,"AT\r");
	if(MODEM_SendCommand(modemOk,modemError,1000,1,"AT+USPM=0,3,0,0\r"))	
	{
		return 0xff;
	}
	if(MODEM_SendCommand(modemOk,modemError,1000,1,"AT+UMGC=0,6,8192\r"))	
	{
		return 0xff;
	}
	if(MODEM_SendCommand(modemOk,modemError,1000,1,"AT+CRSL=5\r"))	
	{
		return 0xff;
	}
	DIS_AU_CMU_EN_SET;
	if(MODEM_SendCommand(modemOk,modemError,1000,1,"AT+USGC=3,1,1,32767,32767,32767\r"))	
	{
		return 0xff;
	}
	
	return 0;
}

// uint8_t CallingProcess(uint8_t callAction)
// {
// 	if(callAction)
// 	{
// 		if(VoiceSetup())
// 		{
// 			return 0xff;
// 		}
// 		if(MODEM_SendCommand(modemOk,modemError,3000,3,"ATA\r"))	
// 		{
// 			return 0xff;
// 		}
// 	}
// 	else
// 	{
// 		if(MODEM_SendCommand(modemOk,modemError,3000,3,"ATH\r"))	
// 		{
// 			return 0xff;
// 		}
// 		MODEM_SendCommand(modemOk,modemError,1000,3,"AT+CLIP=1\r");
// 	}
// 	return 0;
// }

// uint8_t InternetSetProfiles(uint8_t *apn,uint8_t *usr,uint8_t *pwd)
// {
// 	DelayMs(100);
// 	MODEM_FlushBuffer();
// 	MODEM_Info("\r\nGSM->CLEAR APN PROFILE\r\n");
// 	if(MODEM_SendCommand(modemOk,modemError,3000,3,"AT+UPSD=0,0\r"))	return 0xff;
// 	
// 	MODEM_Info("\r\nGSM->SET APN PROFILE\r\n");
// 	if(apn[0] != NULL)
// 	{
// 		if(MODEM_SendCommand(modemOk,modemError,3000,3,"AT+UPSD=0,1,\"%s\"\r", apn))	return 0xff;
// 	}
// 	if(usr[0] != NULL)
// 	{
// 		MODEM_Info("\r\nGSM->SET USE NAME\r\n");
// 		if(MODEM_SendCommand(modemOk,modemError,3000,3,"AT+UPSD=0,2,\"%s\"\r", usr))	return 0xff;
// 	}
// 	if(pwd[0] != NULL)	
// 	{
// 		MODEM_Info("\r\nGSM->SET PASSWORD\r\n");
// 		if(MODEM_SendCommand(modemOk,modemError,3000,3,"AT+UPSD=0,2,\"%s\"\r", usr))	return 0xff;
// 	}
// 	return 0;
// }

// void MODEM_GetIMEI(uint8_t *imei)
// {
// 	strcpy((char *)imei, (char *)modemId);
// }

// void MODEM_Hangup(void)
// {
// 	//MODEM_CommandMode();
// 	DelayMs(100);
// 	COMM_Puts("ATH\r");
// 	MODEM_CheckResponse("OK", 2000);
// }

void MODEM_FlushBuffer(void)
{
	uint8_t c;
	while(RINGBUF_Get(&commBuf, &c)==0);
}

// void MODEM_DataMode(void)
// {	
// 	DelayMs(100);
// 	COMM_Puts("ATO\r");
// 	MODEM_CheckResponse("CONNECT", 10000);
// 	MODEM_FlushBuffer();
// }

// void MODEM_EndRing(void)
// {
// 	modemRinging = 0;
// }

//+CSQ: 8,99
//+CSQ: 11,99
uint8_t MODEM_GetCSQ(void)
{
	uint8_t i,c;
	float fTemp;
	DelayMs(100);
	MODEM_Info("\r\nGSM->CSQ Checking...\r\n");
	MODEM_FlushBuffer();
	COMM_Puts("AT+CSQ\r");				// cancel calling if busy.
	if(!MODEM_CheckResponse("+CSQ:", 2000))
	{
		DelayMs(500);
		i = 0;
		while(RINGBUF_Get(&commBuf,&c) == 0)
		{
				if((c == '\r') || (c == '\n'))
					break;
				csqBuff[i] = c;
				i++;
				if(i >= sizeof(csqBuff)) 
				{
					break;		
				}
		}
		csqBuff[i] = 0;
		//MODEM_Info("\r\nGSM->CSQ Buf:%s\r\n",csqBuff);
		sscanf((char *)csqBuff,"%f",&fTemp);
		//MODEM_Info("\r\nGSM->CSQ Buf:%f\r\n",fTemp);
		csqValue = (uint8_t)fTemp;
		//MODEM_Info("\r\nGSM->CSQ Value:%f\r\n",csqValue);
		if((csqValue < 4) || (csqValue == 99))
			gsmSignal = 0;
		else if(csqValue < 10)
			gsmSignal = 1;
		else if(csqValue < 16)
			gsmSignal = 2;
		else if(csqValue < 22)
			gsmSignal = 3;
		else if(csqValue < 28)
			gsmSignal = 4;
		else if(csqValue < 51)
			gsmSignal = 5;
		else 
			gsmSignal = 0;
		return 0;
	}
	return 0xff;
}

// uint8_t MODEM_Started(void)
// {
// 	return modemStarted;
// }

// is Carrier Detected
// uint8_t MODEM_CarrierDetected(void)
// {
// 	return 0;
// }

uint8_t SendSMS(const uint8_t *recipient, const uint8_t *data)
{
	uint8_t buf[32];
	uint8_t i;
	DelayMs(100);
	MODEM_FlushBuffer();
	MODEM_Info("\r\nGSM->SMS SEND TO %s...\r\n", recipient);
	MODEM_Info("\r\nGSM->DATA:");
	sprintf((char *)buf, "AT+CMGS=\"%s\"\r", recipient);
	COMM_Puts(buf);
	if(MODEM_CheckResponse("> ", 20000))
	{
		MODEM_Info("\r\nGSM->SMS SEND FAILS\r\n");
		return 0xff;
	}
	
	for(i = 0;i < 160;i++)
	{
		if(data[i] == '\0')
		{
			break;
		}
		COMM_Putc(data[i]);
	}
	COMM_Putc(0x1A);
	
	if(MODEM_CheckResponse("OK", 10000)){
		MODEM_Info("\r\nGSM->SMS SEND FAILS\r\n");
		return 0xff;
	}
	MODEM_Info("\r\n");
	MODEM_Info("\r\nOK\r\n");
	
	return 0;
}

// uint8_t MODEM_SendSMS(const uint8_t *recipient, uint8_t *data,uint16_t sizeMax)
// {
// 	uint16_t len,i;
// 	len = strlen((char *)data);
// 	if((len >= 160) && (len <= sizeMax))
// 	{
// 		for(i = 0;i < len; i += 160)
// 		{
// 			DelayMs(5000);
// 			if(SendSMS(recipient,&data[i]))
// 				return 0xff;
// 		}
// 	}
// 	else
// 	{
// 		if(SendSMS(recipient,data))
// 			return 0xff;
// 	}
// 	return 0;
// }

uint8_t MODEM_CheckResponse(uint8_t *str,uint32_t t)
{
	COMPARE_TYPE cmp;
	uint8_t c;
	InitFindData(&cmp,str);
	while(t--)
	{
		DelayMs(1);
		if(RINGBUF_Get(&commBuf,&c) == 0)
		{
			if(FindData(&cmp,c) == 0)
			{
				return 0;
			}
		}
	}
	return 0xff;
}

// wait for reponse from modem within specific time out
// maximum waiting period = (time / 10) seconds
// uint8_t MODEM_Wait(const uint8_t *res, uint32_t time)
// {
// 	uint8_t c, i=0;
// 	time *= 100;
// 	while(time--){
// 		DelayMs(1);
// 		while(RINGBUF_Get(&commBuf, &c)==0){
// 			if(c==res[i]){
// 				if(res[++i]==0)return 1;
// 			}else if(c==res[0]) i = 1;
// 			else i = 0;
// 		}
// 	}
// 	
// 	return 0;
// }

uint8_t MODEM_Gets(uint8_t *buf, uint16_t bufSize, uint8_t match)
{
	uint8_t c;
	uint16_t i = 0;
	
	if(bufSize <= 1 || buf == NULL) return 0;
	
	while(RINGBUF_Get(&commBuf, &c)==0){
		if(c == match) break;
		if(buf != NULL)
			buf[i++] = c;
		if(i>=bufSize-1) break;
	}
	
	buf[i] = 0;
	
	return i;
}


void GetIMEI(uint8_t *buf, uint16_t bufSize)
{	
	MODEM_CheckResponse("OK", 1000);
	MODEM_FlushBuffer();
	
	COMM_Puts("AT+CGSN\r");
	MODEM_CheckResponse("\n", 1000);
	DelayMs(100);
	
	MODEM_Gets(buf, bufSize, '\r');
	
	MODEM_CheckResponse("OK", 1000);
}
