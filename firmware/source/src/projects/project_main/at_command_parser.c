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
#include "at_command_parser.h"
#include "string_parser.h"
#include "rtc.h"
#include "db.h"
#include "gps.h"
#include "db.h"
#include "sst25.h"
#include "tracker.h"
#include "audio.h"
#include "power_management.h"
#include "project_common.h"
#include "gsm_gprs_tasks.h"
#include "gps_task.h"
#include "ambo_pedometer.h"
#include "pedometer_task.h"
#define INFO(...)	//DbgCfgPrintf(__VA_ARGS__)

#define smsScanf	sscanf
extern int nmea_scanf(const char *buff, int buff_sz, const char *format, ...);


extern uint8_t incomingCallPhoneNo[16];
extern uint8_t addressData[110];

const uint8_t terminateStr[7] = "\r\nOK\r\n";
uint8_t cellLocationBuf[128];
uint8_t cellLocationFlag = 0;

uint8_t smsBuf[255];
uint8_t smsSender[18];
uint32_t smsCnt = 0,smsNewMessageFlag = 0,ringFlag = 0;
uint8_t pickUpCallFlag = 0;
uint8_t smsSentFlag = 0;
uint8_t smsModemAccept = 0;
uint8_t inCalling = 0;
uint8_t busyFlag = 0;
uint8_t noAnswerFlag = 0;
uint8_t callUnsuccess = 0;
uint8_t incomingCall = 0;
uint8_t getIncomingNumber = 0;
uint8_t farHangUp = 0;
uint16_t extendedErrorCode = 0;

uint8_t smsUnreadBuff[32] = {0};
RINGBUF smsUnreadRingBuff;
uint8_t *gprsRecvPt;
uint32_t gprsRecvDataLen;
uint8_t gprsLengthBuff[SOCKET_NUM][GPRS_KEEP_DATA_INFO_LENGTH] = {0};
RINGBUF gprsRingLengthBuff[SOCKET_NUM];
uint8_t gprsDeactivationFlag = 0;

uint32_t gprsDataOffset = 0,tcpSocketStatus[SOCKET_NUM] = {SOCKET_CLOSE};
uint32_t GPRS_dataUnackLength[SOCKET_NUM];
uint8_t socketMask[SOCKET_NUM];
uint8_t createSocketNow = 0;

uint32_t socketRecvEnable = 0;
uint32_t socketNo = 0;
uint32_t tcpTimeReCheckData[SOCKET_NUM] = {0xffffffff};
uint8_t DNS_serverIp[16];
char *commandSeparator = " ,:\r\n";

DATE_TIME sysGsmTime;
uint8_t gsmGetTimeFlag = 0;

static const STR_INFO_Type AT_ProcessCmd[] = {
	"+CLIP: \"", GetCallerNumber,
	"RING" , RingCallProcess,
  "+CMGL: ", SMS_Process,
	"+CMTI:", SMS_NewMessage,
	"+UUSORD: ", GPRS_GetLength,
	"+UUPSDD: ",	GPRS_Deactivation,
	"+USORD: ", GPRS_GetData,
	"+UUSOCL: ", GPRS_CloseSocket1,
	"+USOWR: ", GPRS_CloseSocket,
	"+USOCTL: ",GPRS_SocketControl,
	"+CMGR: ",SMS_ReadMsg,
	"+USOCR: ",GPRS_CreateSocket,
	"BUSY",CallBusy,
	"NO ANSWER",CallNoAnswer,
	"NO CARRIER",CallCancel,
	"+CMTI: \"SM\",",SMS_Process,
	"+CMTI: \"ME\",",SMS_Process,
	"+UDNSRN: \"",GPRS_GetDNS,
	"+UULOC: ",CellLocateParser,
	"+CBC: ",GetBatteryStatus,
	"+CEER: ",CallErrorReport,
	"\"\r\r\n>", SMS_ModemOk,
	"ATD",CallOutProcess,
	"+CCLK: \"",GsmGetTime
};

uint8_t cmdCnt[STR_PARSER_COUNT(AT_ProcessCmd)];
STR_PARSER_Type AT_CmdParser;

void AT_CmdProcessInit(void)
{
	uint32_t i;
	RINGBUF_Init(&smsUnreadRingBuff,smsUnreadBuff,sizeof(smsUnreadBuff));
	StrParserInit(&AT_CmdParser,cmdCnt,STR_PARSER_COUNT(AT_ProcessCmd),GPRS_DATA_MAX_LENGTH);
	for(i = 0;i < SOCKET_NUM; i++)
	{
		RINGBUF_Init(&gprsRingLengthBuff[i],gprsLengthBuff[i],GPRS_KEEP_DATA_INFO_LENGTH);
	}
}

void AT_ComnandParser(char c)
{
	static uint32_t RingBuffInit = 0;
	if(RingBuffInit == 0)// int ring buff
	{
		RingBuffInit = 1;
		AT_CmdProcessInit();
	}
	StrComnandParser(AT_ProcessCmd,&AT_CmdParser,cmdCnt,c);
}

#define CALL_PICKUP_BUF_LEN	6
const char callPickUpBuf[6] = ";\r\r\nOK";
char pickUpPos = 0;


//+CCLK: "13/12/11,11:15:41+00
char GsmGetTime(char c)
{
	static uint16_t number;
	if(c < '0' && c > '9' && c != '/' && c != ':' && c != ',' && c != '+')
	{
		return 0;
	}
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
			number = c - '0';
		break;
		case 1:
			number *= 10;
			number += c - '0';
			sysGsmTime.year = number + 2000;
			if(sysGsmTime.year < 2013)
				return 0;
		break;
		case 3:
			number = c - '0';
		break;
		case 4:
			number *= 10;
			number += c - '0';
			sysGsmTime.month = number;
			if(sysGsmTime.month > 12)
					return 0;
		break;
		case 6:
			number = c - '0';
		break;
		case 7:
			number *= 10;
			number += c - '0';
			sysGsmTime.mday = number;
			if(sysGsmTime.mday > 31)
					return 0;
		break;

		case 9:
			number = c - '0';
		break;
		case 10:
			number *= 10;
			number += c - '0';
			sysGsmTime.hour = number;
			if(sysGsmTime.hour > 24)
				return 0;
		break;
		case 12:
			number = c - '0';
		break;
		case 13:
			number *= 10;
			number += c - '0';
			sysGsmTime.min = number;
			if(sysGsmTime.min > 59)
					return 0;
		break;

		case 15:
			number = c - '0';
		break;
		case 16:
			number *= 10;
			number += c - '0';
			sysGsmTime.sec = number;
			if(sysGsmTime.sec > 59)
					return 0;
		break;
		case 17:
			if(c == '+')
			{
				gsmGetTimeFlag = 1;
			}
			return 0;
		break;
			
		default:
		break;
	}
	return 0xff;
}


char CallOutProcess(char c)
{
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
			pickUpPos = 0;
			pickUpCallFlag = 0;
			AT_CmdParser.timeoutSet = 120;// 2min
		break;
		default:
			if(pickUpPos)
			{
				if(c == callPickUpBuf[pickUpPos])
				{
					pickUpPos++;
					if(pickUpPos >= CALL_PICKUP_BUF_LEN)
					{
						pickUpCallFlag = 1;
						return 0;
					}
				}
				else
				{
					pickUpCallFlag = 0;
					pickUpPos = 0;
					//CallCancel(c);
					return 0;
				}
			}
			else
			{
				if(c == ';')
					pickUpPos = 1;
				else if(c < '0' || c > '9')
				{
					pickUpCallFlag = 0;
					//CallCancel(c);
					return 0;
				}
			}
		break;
	}
	return 0xff;
}

char CallCancel(char c)
{
// 	if(pickUpCallFlag == 0)
// 	{
// 		callUnsuccess = 1;
// 	}
	farHangUp = 1;
	return 0;
}


char CallErrorReport(char c)
{
	static uint16_t errorCode = 0;
	
	if(AT_CmdParser.dataRecvLength >= 4) 
	{
		errorCode = 0;
		callUnsuccess = 1;
		return 0;
	}
	else if (AT_CmdParser.dataRecvLength == 0)
	{
		errorCode = 0;
	}
	if((c >= '0') && (c <= '9'))
	{
		errorCode *= 10;
		errorCode += c - '0';
	}
	else if (c == ',')
	{
		if (errorCode == 0)
		{
			errorCode = 9999;
		}
		if (extendedErrorCode == 0)		// check only the first received code
		{
			extendedErrorCode = errorCode;
			if (errorCode != 16)
			{
				callUnsuccess = 1;
			}
		}
		return 0;
	}
	else
	{
		errorCode = 0;
		callUnsuccess = 1;
		return 0;
	}
	
	return 0xff;
}

char CallNoAnswer(char c)
{
	noAnswerFlag = 1;
	return 0;
}

char CallBusy(char c)
{
	busyFlag = 1;
 return 0;
}


/*

+CBC: 0,46

*/

char GetBatteryStatus(char c)
{
	static uint32_t level = 0;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
		case 1:
			break;
		default:
			if(AT_CmdParser.dataRecvLength >= 6) 
			{
				level = 0;
				return 0;
			}
			if((c >= '0') && (c <= '9'))
			{
				level *= 10;
				level += c - '0';
			}
			else
			{
				if(level > 0 && level < 100)
					batteryPercent = level;
				level = 0;
				return 0;
			}
			break;
	}
	return 0xff;
}

/*
	+UULOC: 27/09/2012,18:26:13.363,21.0213870,105.8091050,0,127,0,0 
*/


char CellLocateParser(char c)
{
	
	cellLocationBuf[AT_CmdParser.dataRecvLength] = c;
	if(c == '\r')
	{
		cellLocationBuf[AT_CmdParser.dataRecvLength + 1] = 0;
		cellLocationFlag = 1;
		return 0;
	}
	return 0xff;
}
/*
+CLIP: "0978779222",161,,,,0
*/
char GetCallerNumber(char c)
{
	if((c >= '0') && (c <= '9'))
	{
		incomingCallPhoneNo[AT_CmdParser.dataRecvLength] = c;
		incomingCallPhoneNo[AT_CmdParser.dataRecvLength+1] = '\0';
	}
	else 
	{
		if(c == '"' && AT_CmdParser.dataRecvLength >= 2)
		{
			getIncomingNumber = 1;
		}
		return 0;
	}
 return 0xff;
}

char RingCallProcess(char c)
{
	if(ringFlag == 0)
	{
		ringFlag = 1;
		//COMM_Puts("AT+CLIP=1\r");
	}
	incomingCall = 1;
	return 0;
}
char SMS_ModemOk(char c)
{
	smsModemAccept = 1;
	return 0;
}
// char SMS_Sent(char c)
// {
// 	smsSentFlag = 1;
// 	return 0;
// }

char SMS_NewMessage(char c)
{
	smsNewMessageFlag = 1;
	return 0;
}
/*
AT+USOCTL=0,11 +USOCTL: 0,11,2501
*/
char GPRS_SocketControl(char c)
{
	static uint32_t length = 0;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				socketNo = RemaskSocket((c - '0'));
				length = 0;
				if(socketNo >= SOCKET_NUM)
				{
					return 0;
				}
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			break;
		default:
			if(AT_CmdParser.dataRecvLength > 9) 
			{
				GPRS_dataUnackLength[socketNo] = length;
				return 0;
			}
			if((c >= '0') && (c <= '9'))
			{
				length *= 10;
				length += c - '0';
			}
			else
			{
				GPRS_dataUnackLength[socketNo] = length;
				return 0;
			}
			break;
	}
	return 0xff;
}

char GPRS_CreateSocket(char c)
{
	if((c >= '0') && (c <= '9'))
	{
		socketMask[createSocketNow] = c - '0';
	}
	return 0;
}
/*+UUSOCL: */
char GPRS_CloseSocket1(char c)
{ uint8_t socketNo;
	socketNo = RemaskSocket((c - '0'));
	if(socketNo < SOCKET_NUM)
	{
		tcpSocketStatus[socketNo] = SOCKET_CLOSE;
	}
	return 0;
}
/*+USOWR: 0,0*/
char GPRS_CloseSocket(char c)
{
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				socketNo = RemaskSocket((c - '0'));
				if(socketNo >= SOCKET_NUM)
				{
					return 0;
				}
			break;
		case 2:
				if(c == '0')
				{
					tcpSocketStatus[socketNo] = SOCKET_CLOSE;
				}
				return 0;
		default:
			break;
	}
	return 0xff;
}


/*   14,"REC READ","+84972046096","","12/07/26,11:10:17+28"   */
char SMS_Process(char c)
{ 
	static uint32_t length = 0;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				if((c >= '0') && (c <= '9'))
				{
					length = c - '0';
					break;
				}
				return 0;
		case 1:
		case 2:
		case 3:
				if((c >= '0') && (c <= '9'))
				{
					length *= 10;
					length += c - '0';
					break;
				}
				else
				{
					RINGBUF_Put(&smsUnreadRingBuff,(uint8_t)(length & 0x00ff));
					RINGBUF_Put(&smsUnreadRingBuff,(uint8_t)((length >> 8) & 0x00ff));
					smsCnt++;
					smsNewMessageFlag = 1;
					return 0;
				}	
		default:
			smsNewMessageFlag = 1;
			return 0;
	}
	return 0xff;
}


uint8_t RemaskSocket(uint8_t socket)
{
	uint8_t i;
	for(i = 0;i < SOCKET_NUM;i++)
	{
		if(tcpSocketStatus[i] == SOCKET_OPEN)
			if(socketMask[i] == socket)
				return i;
	}
	return 0xff;
}
//+UUPSDD: 0
char GPRS_Deactivation(char c)
{
	gprsDeactivationFlag = 1;
	return 0;
}

/*				
				  \/
+UUSORD: 0,14
AT_cmdRecvBuff = 0,14
*/
char GPRS_GetLength(char c)
{
	static uint32_t length = 0;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				socketNo = RemaskSocket((c - '0'));
				if(socketNo >= SOCKET_NUM)
				{
					return 0;
				}
			break;
		case 2:
			if((c >= '0') && (c <= '9'))
				length = c - '0';
			else
				return 0;
			break;
		case 3:
			if((c >= '0') && (c <= '9'))
			{
				length *= 10;
				length += c - '0';
				RINGBUF_Put(&gprsRingLengthBuff[socketNo],(uint8_t)(length & 0x00ff));
				RINGBUF_Put(&gprsRingLengthBuff[socketNo],(uint8_t)((length >> 8) & 0x00ff));
				length = 0;
			}
			return 0;
		default:
			break;
	}
	return 0xff;
}


enum{
	GPRS_START_RECV_DATA,
	GPRS_GET_LENGTH,
	GPRS_PREPARE_GET_DATA,
	GPRS_GET_DATA,
	GPRS_FINISH
}dataPhase = GPRS_FINISH;

// uint32_t GPRS_SendCmdRecvData(uint8_t socketNum,uint8_t *gprsBuff,uint16_t gprsBuffLen,uint32_t *dataLen)
// {
// 	uint8_t buf[20];
// 	Timeout_Type tout;
// 	MODEM_Info("\r\nGSM->GPRS READ DATA\r\n");
// 	DelayMs(100);
// 	dataPhase = GPRS_START_RECV_DATA;
// 	gprsRecvDataLen = 0;
// 	gprsRecvPt = gprsBuff;
// 	socketRecvEnable = 1;
// 	sprintf((char *)buf, "AT+USORD=%d,%d\r",socketMask[socketNum],(GPRS_DATA_MAX_LENGTH-32));
// 	COMM_Puts(buf);
// 	InitTimeout(&tout,TIME_MS(1000));
// 	while(dataPhase != GPRS_FINISH)
// 	{
// 		if(CheckTimeout(&tout) == TIMEOUT)
// 		{
// 			return 1;
// 		}
// 	}
// 	socketRecvEnable = 0;
// 	*dataLen = gprsRecvDataLen;
// 	return 0;
// }

uint32_t GPRS_CmdRecvData(uint8_t *gprsBuff,uint16_t gprsBuffLen,uint32_t *dataLen)
{
	static uint32_t timeRead[SOCKET_NUM] = {0};
	uint8_t c,buf[20];
	uint16_t len = 0;
	Timeout_Type tout;
	uint32_t socketNum;
	gprsBuff[0] = 0;
	*dataLen = 0;
	for(socketNum = 0;socketNum < SOCKET_NUM;socketNum++)
	{
		if(tcpSocketStatus[socketNum] == SOCKET_OPEN)
		{
			if((TICK_Get() - timeRead[socketNum]) >= tcpTimeReCheckData[socketNum]) //5SEC
			{
				len = (GPRS_DATA_MAX_LENGTH-32);
			}
			else if((RINGBUF_GetFill(&gprsRingLengthBuff[socketNum]) >= 2))
			{
				RINGBUF_Get(&gprsRingLengthBuff[socketNum],&c);
				len = c;
				RINGBUF_Get(&gprsRingLengthBuff[socketNum],&c);
				len |= (((uint16_t)c) << 8 & 0xff00);
			}	
			while(len)
			{
				timeRead[socketNum] = TICK_Get();
				DelayMs(100);
				if(len > (GPRS_DATA_MAX_LENGTH-32))
				{
					len -= (GPRS_DATA_MAX_LENGTH-32);
				}
				else
				{
					len = 0;
				}
				dataPhase = GPRS_START_RECV_DATA;
				gprsRecvDataLen = 0;
				gprsRecvPt = gprsBuff;
				socketRecvEnable = 1;
				MODEM_Info("\r\nGSM->GPRS READ DATA\r\n");
				sprintf((char *)buf, "AT+USORD=%d,%d\r",socketMask[socketNum],(GPRS_DATA_MAX_LENGTH-32));
				COMM_Puts(buf);
				InitTimeout(&tout,TIME_MS(3000));
				while(dataPhase != GPRS_FINISH)
				{
					if(CheckTimeout(&tout) == TIMEOUT)
					{
						 socketRecvEnable = 0;
						 return 0xff;
					}
				}
				socketRecvEnable = 0;
				*dataLen = gprsRecvDataLen;
				return socketNum;
			}
		}
		else
		{
			timeRead[socketNum] = TICK_Get();
		}
	}
	socketRecvEnable = 0;
	return 0xff;
}

char GPRS_GetDNS(char c)
{
	static uint8_t *pt;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				pt = DNS_serverIp;
				*pt = c;
				pt++;
			break;
		default:
				if((c == '"') || (pt >= DNS_serverIp + sizeof(DNS_serverIp)))
				{
					return 0;
				}
				else
				{
					*pt = c;
					pt++;
					*pt = 0;
				}
			break;
	}
	
	return 0xff;
}

/*
+USORD: 0,12,"thienhaiblue"
									 1,"
AT_cmdRecvBuff = 0,12,"thienhaiblue"
									 123,"
                 0,0,""
*/
char GPRS_GetData(char c)
{
	static uint32_t dataLen = 0;
	switch(AT_CmdParser.dataRecvLength)
	{
		case 0:
				socketNo = RemaskSocket((c - '0'));
				if(socketNo >= SOCKET_NUM)
				{
					return 0;
				}
				dataPhase = GPRS_GET_LENGTH;
			break;
		case 1:
				if(c != ',') return 0;
			break;
		case 2:
			if((c > '0') && (c <= '9'))
				dataLen = c - '0';
			else
				return 0;
			break;
		default:
			switch(dataPhase)
			{
				case GPRS_GET_LENGTH:
					if((c >= '0') && (c <= '9'))
					{
						dataLen *= 10;
						dataLen += c - '0';
					}
					else if(c == ',')
						dataPhase = GPRS_PREPARE_GET_DATA;
					else 
						return 0;
					break;
				case GPRS_PREPARE_GET_DATA:
					if(c == '"')
					{
						dataPhase = GPRS_GET_DATA;
						gprsRecvDataLen = 0;
					}
					else 
						return 0;
					break;
				case GPRS_GET_DATA:
					if(socketRecvEnable)
					{
						gprsRecvPt[gprsRecvDataLen] = c;
						gprsRecvDataLen++;
					}
					dataLen--;
					if(dataLen == 0)
					{
						dataPhase = GPRS_FINISH;
						return 0;
					}
					break;
			}
			break;
	}
	return 0xff;
}

// char CLOSE_TCP_Process(char c)
// {
// 	//tcpStatus = TCP_CLOSED;
// 	return 0;
// }


/*+CMGR: "REC READ","+841645715282","","12/07/26,20:50:07+28"
"thienhailue"
*/
uint8_t flagSmsReadFinish = 0;
char SMS_ReadMsg(char c)
{ 
	static uint8_t comma = 0,getSmsDataFlag = 0;
	static uint8_t *u8pt;
	static uint8_t *u8pt1;
	static uint8_t *u8pt2;
	if(AT_CmdParser.dataRecvLength == 0)
	{
		comma = 0;
		getSmsDataFlag = 0;
		u8pt = smsSender;
		u8pt2 = smsBuf;
		u8pt1 = (uint8_t *)terminateStr;
		return 0xff;
	}
	if(c == ',') 
	{
		comma++;
	}
	
	if(getSmsDataFlag)
	{
		if(c == *u8pt1)
		{
			u8pt1++;
			if(*u8pt1 == '\0')
			{
				*u8pt2 = 0;
				flagSmsReadFinish = 1;
				return 0;
			}
		}
		else
		{
			u8pt1 = (uint8_t *)terminateStr;
			if(c == *u8pt1) u8pt1++;
		}
		if((u8pt2 - smsBuf) >= sizeof(smsBuf))
		{		
			*u8pt2 = 0;
			flagSmsReadFinish = 1;
			return 0;
		}
		*u8pt2 = c;
		 u8pt2++;
	}
	else
	{
		switch(comma)
		{
			case 0:
				break;
			case 1:
				if((u8pt - smsSender) >= sizeof(smsSender))
				{
					smsSender[0] = 0;
					return 0;
				}
				if(((c >= '0') && (c <= '9')) || (c == '+'))
				{
					*u8pt = c;
					u8pt++;
					*u8pt = 0;
				}
				break;
			default:
				break;
		}
	}
	if(c == '\n')
	{
		getSmsDataFlag = 1;
	}
	return 0xff;
}

// untested
void SMS_Manage(uint8_t *buff,uint32_t lengBuf)
{
	Timeout_Type t;
	uint8_t tmpBuf[32],c;	
	uint16_t smsLoc,i,res;
	uint32_t len;
	
	// read the newly received SMS
	DelayMs(500);
	INFO("\r\nSMS->CHECK ALL\r\n");
	COMM_Puts("AT+CMGL=\"ALL\"\r");
	DelayMs(2000);
	while(RINGBUF_GetFill(&smsUnreadRingBuff) >=2)
	{
		//SetLowPowerModes(LPW_NOMAL);
		//watchdogFeed[WTD_MAIN_LOOP] = 0;
		RINGBUF_Get(&smsUnreadRingBuff,&c);
		smsLoc = c;
		RINGBUF_Get(&smsUnreadRingBuff,&c);
		smsLoc |= (((uint16_t)c) << 8 & 0xff00);
		// read the newly received SMS
		INFO("\r\nSMS->READ SMS\r\n");
		flagSmsReadFinish = 0;
		sprintf((char *)tmpBuf, "AT+CMGR=%d\r", smsLoc);
		COMM_Puts(tmpBuf);
		InitTimeout(&t,3000);//1s
		while(!flagSmsReadFinish)
		{
			if(CheckTimeout(&t) == TIMEOUT) 
			{
				break;
			}
		}
		smsBuf[sizeof(smsBuf) - 1] = 0;
		INFO("\n\rSMS->PHONE:%s\n\r", smsSender);	
		INFO("\r\nSMS->DATA:%s\r\n",smsBuf);
		// delete just received SMS
		INFO("\n\rSMS->DELETE:%d\n\r",smsLoc);
		sprintf((char *)tmpBuf, "AT+CMGD=%d\r", smsLoc);
		COMM_Puts(tmpBuf);
		MODEM_CheckResponse("OK", 1000);
		if(flagSmsReadFinish == 0)
		{
			continue;
		}
		len = 0;
		res = CMD_CfgParse((char *)smsBuf, buff, lengBuf, &len, 1);
		if(len >= 160)
		{
			for(i = 0;i < len; i += 160)
			{
				DelayMs(500);
				if (SendSMS(smsSender,&buff[i]))
				{
					// retry 1 time
					DelayMs(500);
					SendSMS(smsSender,&buff[i]);
				}
			}
		}
		else if(len)
		{
			if (SendSMS(smsSender,buff))
			{
				// retry 1 time
				DelayMs(1000);
				SendSMS(smsSender,buff);
			}
		}
		if(res == 0xa5)
		{
			AudioStopAll();
			AudioPlayFile("1_2", TOP_INSERT);
			DelayMs(3000);
			AudioStopAll();
			AudioPlayFile("1_1",TOP_INSERT);				// power on
			DelayMs(2000);
			NVIC_SystemReset();
		}
	}
}

uint16_t CMD_CfgParse(char *cmdBuff, uint8_t *smsSendBuff, uint32_t smsLenBuf, uint32_t *dataOutLen, uint8_t pwdCheck)
{
	char *buff, *pt, *u8pt, tempBuff0[34], tempBuff1[18], tempBuff2[18], flagCfgSave = 0;
	uint32_t i, t1, t2, t3, t4, t5, t6, t7, len, flagResetDevice = 0;
	double fTemp1, fTemp2;
	float fTemp3;
	uint8_t cmdOkFlag = 0;
	DATE_TIME sysTime;
	char c;
	len = 0;
	
	buff = cmdBuff;
// 	pt = strstr(buff, "ABCFG,115");
// 	if (pt != NULL)
// 	{
// 		sysDownloadConfig = 2;
// 		cmdOkFlag = 1;
// 		return 0;
// 	}
	pt = strstr(buff, "MYSTEP");
	if (pt != NULL)
	{
		len += sprintf((char *)&smsSendBuff[len], "YOUR STEPS IS:%d\n",Pedometer.stepCount);
		*dataOutLen = len;
		cmdOkFlag = 1;
		return 0;
	}
	pt = strstr(buff, "ABCFG,116");
	if (pt != NULL)
	{
		// start send gps data to received web command
		GprsTask_StartTask(SEND_GPS_DATA_TASK);
		cmdOkFlag = 1;
		return 0;
	}
	
	//check password
	if(pwdCheck)
	{	
		pt = strstr(buff,(char *)sysCfg.smsPwd);
		if (pt != NULL)
		{
			t1 = strlen((char*)sysCfg.smsPwd);
		}
		if (pt == NULL)
		{
			pt = strstr(buff, DEFAULT_SMS_MASTER_PWD);
			if (pt != NULL)
			{
				t1 = strlen(DEFAULT_SMS_MASTER_PWD);
			}
		}
		if ((pt != NULL) && (pt == buff) && (strlen(pt) > t1) && (pt[t1] == ',')) // password has to be sent at the start of the message
		{
			INFO("\n\rSMS->PASSWORD OK\n\r");
		}
		else
		{
			INFO("\n\rSMS->PASSWORD FAILS\n\r");
			pt = strstr(buff, "ABCFG");
			if (pt != NULL)
			{
				len += sprintf((char*)&smsSendBuff[len], "WRONG PASSWORD\n");
				smsSendBuff[len] = 0;
				*dataOutLen = len;
			}
			return 1;
		}
	
// 		pt = strstr(buff, "ABCFG");
// 		if (pt != NULL)
// 		{
// 			// remove all white space characters
// 			for (i = 0; i < strlen(pt); i++)
// 			{
// 				if ((pt[i] == '\n') || (pt[i] == '\r') || (pt[i] == ' ') || (pt[i] == '\t'))
// 				{
// 					pt[i] = 0;
// 					break;
// 				}
// 			}
// 			
// 			u8pt = strstr(pt, ",,");
// 			if (u8pt != NULL)
// 			{
// 				// avoid empty argument
// 				*u8pt = 0;
// 			}
// 		}

		pt = strstr(buff, "ABFND");
		if (pt != NULL)
		{
			GpsTask_EnableGps();
			GprsTask_StartTask(SEND_GPS_DATA_TASK);
			INFO("\n\rFIND_NOW->OK\n\r");
			//len += sprintf((char*)&smsSendBuff[len], "ABFND->OK\n");
			cmdOkFlag = 1;
			return 0;
		}
		
		//change password
		pt = strstr(buff,"ABCFG,0,");
		if(pt != NULL)
		{
			*tempBuff0 = 0;
			smsScanf(pt,"ABCFG,0,%[^, \t\n\r]",tempBuff0);
			if ((*tempBuff0 != 0) && (strlen(tempBuff0) <= CONFIG_SIZE_SMS_PWD))
			{
				memcpy((char *)sysCfg.smsPwd,tempBuff0,sizeof(sysCfg.smsPwd));
				INFO("\n\rSMS->NEW PASSWORD:%s\n\r",sysCfg.smsPwd);
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,0: OK\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,0: FAIL\n");
			}
		}
		
		//change gprs settings
		pt = strstr(buff,"ABCFG,1,");
		if(pt != NULL)
		{
			*tempBuff0 = 0;
			*tempBuff1 = 0;
			*tempBuff2 = 0;
			smsScanf(pt, "ABCFG,1,%[^,\t\n\r],%[^,\t\n\r],%[^,\t\n\r]", tempBuff1, tempBuff0, tempBuff2);
			if ((*tempBuff1 != 0) && (*tempBuff0 != 0) && (*tempBuff2 != 0) &&
					(strlen(tempBuff1) <= CONFIG_SIZE_GPRS_APN) && (strlen(tempBuff0) <= CONFIG_SIZE_GPRS_USR) && (strlen(tempBuff2) <= CONFIG_SIZE_GPRS_PWD))
			{
				memcpy((char*)sysCfg.gprsApn, tempBuff1, sizeof(sysCfg.gprsApn));
				memcpy((char*)sysCfg.gprsUsr, tempBuff0, sizeof(sysCfg.gprsUsr));
				memcpy((char*)sysCfg.gprsPwd, tempBuff2, sizeof(sysCfg.gprsPwd));
				INFO("\n\rSMS->CFG GPRS APN:%s, USER:%s, PWD:%s\n\r", sysCfg.gprsApn, sysCfg.gprsUsr, sysCfg.gprsPwd);
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,1->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,1->FAILED\n");
			}
		}
		
		//change Server Name
		pt = strstr(buff,"ABCFG,4,");
		if(pt != NULL)
		{
			*tempBuff0 = 0;
			t1 = 2;
			t2 = 0;
			smsScanf(pt, "ABCFG,4,%d,%[^, \t\n\r],%d", &t1, tempBuff0, &t2);
			if ((*tempBuff0 != 0) && (strlen(tempBuff0) <= CONFIG_SIZE_SERVER_NAME) && (t1 < 2) && (t2 > 0) && (t2 <= 65535))
			{
				if (t1 == 0)
				{
					memcpy((char*)sysCfg.priDserverName, tempBuff0, sizeof(sysCfg.priDserverName));
					sysCfg.priDserverPort = t2;
					sysCfg.dServerUseIp = 0;
					INFO("\n\rSMS->CFG PRI DATA SERVER DOMAIN:%s:%d\n\r", sysCfg.priDserverName, sysCfg.priDserverPort);
				}
				else
				{
					memcpy((char*)sysCfg.secDserverName, tempBuff0, sizeof(sysCfg.secDserverName));
					sysCfg.secDserverPort = t2;
					sysCfg.dServerUseIp = 0;
					INFO("\n\rSMS->CFG SEC DATA SERVER DOMAIN:%s:%d\n\r", sysCfg.secDserverName, sysCfg.secDserverPort);
				}
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,4->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
				tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,4->FAILED\n");
			}
		}

		//change Server IP
		pt = strstr(buff,"ABCFG,5,");
		if(pt != NULL)
		{
			t1 = 2;
			t2 = 0;
			t3 = 0;
			t4 = 0;
			t5 = 0;
			t6 = 0;
			smsScanf(pt, "ABCFG,5,%d,%d.%d.%d.%d,%d", &t1, &t2, &t3, &t4, &t5, &t6);
			if ((t1 < 2) && (t2 > 0) && (t2 <= 255) && (t3 <= 255) && (t4 <= 255) && (t5 <= 255) && (t6 > 0) && (t6 <= 65535))
			{
				if (t1 == 0)
				{
					u8pt = (char *)sysCfg.priDserverIp;
					u8pt[0] = t2;
					u8pt[1] = t3;
					u8pt[2] = t4;
					u8pt[3] = t5;
					sysCfg.priDserverPort = t6;
					sysCfg.dServerUseIp = 1;
					INFO("\n\rSMS->CFG PRI DATA SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.priDserverPort);
				}
				else
				{
					u8pt = (char *)sysCfg.secDserverIp;
					u8pt[0] = t2;
					u8pt[1] = t3;
					u8pt[2] = t4;
					u8pt[3] = t5;
					sysCfg.secDserverPort = t6;
					sysCfg.dServerUseIp = 1;
					INFO("\n\rSMS->NEW SEC DATA SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.secDserverPort);

				}
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,5->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
				tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,5->FAILED\n");
			}
		}
		
		//Clear log
		pt = strstr(buff,"ABCFG,12,0");
		if(pt != NULL)
		{
			__disable_irq();
			DB_LogRingReset(&trackerLog);
			cmdOkFlag = 1;
			INFO("\n\rSMS->CLEAR LOG\n\r");
			len += sprintf((char *)&smsSendBuff[len], "ABCFG,12->SUCCESS\n");
			__enable_irq();
		}

		//set default config
		pt = strstr(buff,"ABCFG,13");
		if(pt != NULL)
		{
			__disable_irq();
			DB_LogRingReset(&trackerLog);
			SST25_Erase(MILEAGE_ADDR,block4k);
			SST25_Erase(RESET_CNT_ADDR,block4k);
			DB_ResetCntLoad();
			DB_ResetCntSave();
			DB_MileageLoad();
			DB_MileageSave();
			memset((void*)&sysCfg, 0xFF, sizeof sysCfg);
			sysUploadConfig = 2;
			CFG_Save();
			CFG_Load();
			for(i = 0; i < SOCKET_NUM; i++)
			{
				tcpSocketStatus[i] = SOCKET_CLOSE;
			}
			INFO("\n\rSMS->SET DEFAULT CONFIGURATION\n\r");
			len += sprintf((char *)&smsSendBuff[len], "ABCFG,13->SUCCESS\n");
			cmdOkFlag = 1;
			__enable_irq();
		}
		
		//change device id
		pt = strstr(buff,"ABCFG,14,");
		if(pt != NULL)
		{
			*tempBuff0 = 0;
			smsScanf(pt,"ABCFG,14,%[^,\t\n\r]",tempBuff0);
			if ((*tempBuff0 != 0) && (strlen(tempBuff0) <= 18))
			{
				memcpy((char *)sysCfg.id,tempBuff0,sizeof(sysCfg.id));
				INFO("\n\rSMS->CHANGE DEVICE ID:%s\n\r",sysCfg.id);
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,14->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,14->FAILED\n");
			}
		}
		
// 		//get gps location
// 		pt = strstr(buff,"ABCFG,25");
// 		if(pt != NULL)
// 		{
// 			INFO("\n\rSMS->GET GPS LOCATION\n\r");
// 			len += sprintf((char *)&smsSendBuff[len], "TIME:%d:%d:%d\n",sysTime.hour,sysTime.min,sysTime.sec);
// 			len += sprintf((char *)&smsSendBuff[len], "SPEED:%f\n", lastGpsInfo.speed);
// 			len += sprintf((char *)&smsSendBuff[len], "HDOP:%f\n", lastGpsInfo.HDOP);
// 			len += sprintf((char *)&smsSendBuff[len],	"http://maps.google.com/maps?q=%.8f,%.8f\n",formatLatLng(lastGpsInfo.lat),formatLatLng(lastGpsInfo.lon));
// 			len += sprintf((char *)&smsSendBuff[len], "ABCFG,25->SUCCESS\n");
// 			cmdOkFlag = 1;
// 		}
			
		//change interval time run
		pt = strstr(buff,"ABCFG,16,");
		if(pt != NULL)
		{
			t1 = 0; // error number
			c = 0;
			t2 = 0;
			smsScanf(pt, "ABCFG,16,%d%c", &t1, &c);
			if (c == 'm')
			{
				if ((t1 > 0) && (t1 <= 30))
				{
					sysCfg.reportInterval = t1;
					t2 = 1;
				}
			}
			else if (c == 'h')
			{
				if ((t1 > 0) && (t1 <= 24))
				{
					sysCfg.reportInterval = t1 * 60;
					t2 = 1;
				}
			}
			
			if (t2)
			{
				INFO("\n\rSMS->CHANGE INTERVAL TIME RUN:%d/%d\n\r",sysCfg.reportInterval);
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,16->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,16->FAILED\n");
			}
		}
		
			//reset device
		flagResetDevice = 0;
		pt = strstr(buff,"ABCFG,40");
		if(pt != NULL)
		{
			flagResetDevice = 1;
			INFO("\n\rSMS->RESET DEVICE\\n\r");
			len += sprintf((char *)&smsSendBuff[len], "ABCFG,40->SUCCESS\n");
			cmdOkFlag = 1;
		}
		
		// DTMF on
		pt = strstr(buff,"ABCFG,66,");
		if(pt != NULL)
		{
			t1 = 2; // error number
			smsScanf(pt, "ABCFG,66,%d", &t1);
			if (t1 == 1)
			{
				sysCfg.featureSet |= FEATURE_DTMF_ON;
				INFO("\n\rSMS->DTMF ON\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,66->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else if (t1 == 0)
			{
				sysCfg.featureSet &= ~FEATURE_DTMF_ON;
				INFO("\n\rSMS->DTMF OFF\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,66->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,66->FAILED\n");
			}
		}
		
		//No movement on
		pt = strstr(buff,"ABCFG,77,");
		if(pt != NULL)
		{
			t1 = 721; // error number
			smsScanf(pt, "ABCFG,77,%d", &t1);
			if ((t1 <= 720) && ((sysCfg.featureSet & FEATURE_MAN_DOWN_SET) == 0))
			{
				if (t1 > 0)
				{
					sysCfg.noMovementInterval = t1;
					sysCfg.featureSet |= FEATURE_NO_MOVEMENT_WARNING_SET;
					INFO("\n\rSMS->NO MOVEMENT ON, INTERVAL: %d\n\r", sysCfg.noMovementInterval);
				}
				else
				{
					// disable no movement alarm
					sysCfg.featureSet &= ~FEATURE_NO_MOVEMENT_WARNING_SET;
					INFO("\n\rSMS->NO MOVEMENT OFF\n\r");
				}
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,77->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,77->FAILED\n");
			}
		}
		
		// man down enable
		pt = strstr(buff,"ABCFG,80,");
		if(pt != NULL)
		{
			t1 = 2; // error number
			smsScanf(pt, "ABCFG,80,%d", &t1);
			if (t1 == 1)
			{
				sysCfg.featureSet |= FEATURE_MAN_DOWN_SET;
				sysCfg.featureSet &= ~FEATURE_NO_MOVEMENT_WARNING_SET;
				INFO("\n\rSMS->MAN DOWN ON\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,80->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else if (t1 == 0)
			{
				sysCfg.featureSet &= ~FEATURE_MAN_DOWN_SET;
				INFO("\n\rSMS->MAN DOWN OFF\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,80->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,80->FAILED\n");
			}
		}
		
		//Lone worker enable
		pt = strstr(buff,"ABCFG,88,");
		if(pt != NULL)
		{
			t1 = 721; // error number
			smsScanf(pt,"ABCFG,88,%d", &t1);
			if (t1 <= 720)
			{
				if (t1 > 0)
				{
					sysCfg.loneWorkerInterval = t1;
					sysCfg.featureSet |= FEATURE_LONE_WORKER_SET;
					INFO("\n\rSMS->LONE WORKER ON, INTERVAL: %d\n\r", sysCfg.loneWorkerInterval);
				}
				else
				{
					// disable lone worker alarm
					sysCfg.featureSet &= ~FEATURE_LONE_WORKER_SET;
					INFO("\n\rSMS->LONE WORKER OFF\n\r");
				}
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,88->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,88->FAILED\n");
			}
		}

		//Auto answer on
		pt = strstr(buff,"ABCFG,90,");
		if(pt != NULL)
		{
			t1 = 2; // error number
			smsScanf(pt, "ABCFG,90,%d", &t1);
			if (t1 == 1)
			{
				sysCfg.featureSet |= FEATURE_AUTO_ANSWER;
				INFO("\n\rSMS->AUTO ANSWER ON\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,90->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else if (t1 == 0)
			{
				sysCfg.featureSet &= ~FEATURE_AUTO_ANSWER;
				INFO("\n\rSMS->AUTO ANSWER OFF\n\r");
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,90->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,90->FAILED\n");
			}
		}
		
		//change language
		pt = strstr(buff,"ABCFG,92,");
		if(pt != NULL)
		{
			t1 = 9; // error number
			smsScanf(pt, "ABCFG,92,%d", &t1);
			if(t1 <= 8)
			{
				sysCfg.language = t1;
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,92->SUCCESS\n");
				INFO("\n\rSMS->LANGUAGE:%d\n\r",sysCfg.language);
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,92->FAILED\n");
			}
		}
		
		//Set phone number for phone book
		pt = strstr(buff,"ABCFG,93,");
		if(pt != NULL)
		{
			t1 = 6; // error number
			*tempBuff0 = 0;
			*tempBuff1 = 0;
			smsScanf(pt, "ABCFG,93,%d,%[^,\t\n\r],%[^, \t\n\r]", &t1, tempBuff0, tempBuff1);
			if ((t1 < 6) && (*tempBuff0 != 0) && (*tempBuff1 != 0) &&
					(strlen(tempBuff0) <= CONFIG_SIZE_USER_NAME) && (strlen(tempBuff1) <= CONFIG_SIZE_PHONE_NUMBER))
			{
				memcpy((char*)(sysCfg.userList[t1].userName), tempBuff0, sizeof(sysCfg.userList[t1].userName));
				memcpy((char*)(sysCfg.userList[t1].phoneNo), tempBuff1, sizeof(sysCfg.userList[t1].phoneNo));
				INFO("\n\rSMS->Contact:%d, Name:%s, PhoneNo:%s\n\r", t1, tempBuff0, tempBuff1);
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,93->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,93->FAILED\n");
			}
		}
		
		// delete contact in phone book
		pt = strstr(buff, "ABCFG,94,");
		if (pt != NULL)
		{
			t1 = 6; // error number
			smsScanf(pt, "ABCFG,94,%d", &t1);
			if (t1 < 6)
			{
				*((char*)sysCfg.userList[t1].userName) = 0;
				*((char*)sysCfg.userList[t1].phoneNo) = 0;
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,94->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
				INFO("\n\rSMS->DELETE CONTACT:%d\n\r", t1);
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,94->FAILED\n");
			}
		}
		
		//geofence setup
		pt = strstr(buff,"ABCFG,95,");
		if(pt != NULL)
		{
			t1 = 10; // error number
			fTemp1 = 0;
			fTemp2 = 0;
			fTemp3 = 0;
			t2 = 0;
			smsScanf(pt, "ABCFG,95,%d,%lf,%lf,%f,%d", &t1, &fTemp1, &fTemp2, &fTemp3, &t2);
			if((t1 < 10) && (fTemp1 != 0) && (fTemp2 != 0) && (fTemp3 > 0) && (fTemp3 <= 100000) && (t2 > 0) && (t2 <= 720))
			{
				
				sysCfg.geoFence[t1].lat = fTemp1;
				sysCfg.geoFence[t1].lon = fTemp2;
				sysCfg.geoFence[t1].radius = fTemp3;
				sysCfg.geoFence[t1].time = t2;
				sysCfg.geoFence[t1].geoFenceEnable = 1;
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,95->SUCCESS\n");
				cmdOkFlag = 1;
				flagCfgSave = 1;
				INFO("\n\rSMS->ADD GEOFENCE:%d,%f,%f,%d,%d\n\r",sysCfg.geoFence[t1].lat
																								,sysCfg.geoFence[t1].lon
																								,sysCfg.geoFence[t1].radius
																								,sysCfg.geoFence[t1].time
																								);
			}
			else
			{
				len += sprintf((char *)&smsSendBuff[len], "ABCFG,95->FAILED\n");
			}
		}
		
		// disable geo fence
		pt = strstr(buff, "ABCFG,96,");
		if (pt != NULL)
		{
			t1 = 10; // error number
			smsScanf(pt, "ABCFG,96,%d", &t1);
			if (t1 < 10)
			{
				sysCfg.geoFence[t1].geoFenceEnable = 0;
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,96->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
				INFO("\n\rSMS->DISABLE GEOFENCE:%d\n\r", t1);
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,96->FAILED\n");
			}
		}
		
		// config geo fence type
		pt = strstr(buff, "ABCFG,97,");
		if (pt != NULL)
		{
			t1 = 3; // error number
			smsScanf(pt, "ABCFG,97,%d", &t1);
			if (t1 < 3)
			{
				sysCfg.geoFenceType = t1;
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,97->SUCCESS\n");
				flagCfgSave = 1;
				cmdOkFlag = 1;
				INFO("\n\rSMS->GEOFENCE TYPE:%d\n\r", t1);
			}
			else
			{
				len += sprintf((char*)&smsSendBuff[len], "ABCFG,97->FAILED\n");
			}
		}
	
		if ((cmdOkFlag == 0) && (len == 0))
		{
			len += sprintf((char*)&smsSendBuff[len], "CMD NOT SUPPORTED\n");
		}
		
		if(len >= smsLenBuf)
		{
			len = smsLenBuf;
		}
		smsSendBuff[len] = 0;
		*dataOutLen = len;
		if(flagCfgSave)
		{
			sysUploadConfig = 2;			// new config available
			CFG_Save();
		}
		if(flagResetDevice)
		{
			return 0xa5;
		}
		return 0;
	}
	
/*--------------------------------------------------------------------------------------------
	end of SMS configuration command handling
---------------------------------------------------------------------------------------------*/
	
/*--------------------------------------------------------------------------------------------
	start of Web configuration command handling
---------------------------------------------------------------------------------------------*/
	
	else
	{
		// Find Now cmd
		pt = strstr(buff, "ABFND");
		if (pt != NULL)
		{
			GprsTask_StartTask(SEND_GPS_DATA_TASK);
			INFO("\n\rFIND_NOW->OK\n\r");
			len += sprintf((char*)&smsSendBuff[len], "ABFND->OK\n");
			cmdOkFlag = 1;
			*dataOutLen = len;
			smsSendBuff[len] = 0;
			return 0;
		}
		
		pt = strstr(buff, "ABADR,");
		if (pt != NULL)
		{
			memset(addressData, 0, sizeof(addressData));
			smsScanf(pt, "ABADR,%[^\t\r\n]", addressData);
			if (*addressData == 0)
			{
				sprintf((char*)addressData, "No address available");
			}
			cmdOkFlag = 1;
			len = 0;
			*dataOutLen = 0;
		//	return 0;
		}
		
		// config commands
		pt = NULL;
		while (1)
		{
			if (pt != NULL)
			{
				buff = pt + 10;
			}
			buff = strstr(buff, "ABCFG,");
			if (buff == NULL)
			{
				break;
			}
			
			// configure gprs settings
			pt = strstr(buff, "ABCFG,101,");
			if (pt == buff)
			{
				*tempBuff0 = 0;
				*tempBuff1 = 0;
				*tempBuff2 = 0;
				t1 = 0;
				smsScanf(pt, "ABCFG,101,%[^,\t\n\r],%[^,\t\n\r],%[^,\t\n\r],%d", tempBuff0, tempBuff1, tempBuff2, &t1);
				if ((*tempBuff0 != 0) && (*tempBuff1 != 0) && (*tempBuff2 != 0) && (t1 > 0) && (t1 <= 1440))
				{
					memcpy((char*)sysCfg.gprsApn, tempBuff0, sizeof(sysCfg.gprsApn));
					memcpy((char*)sysCfg.gprsUsr, tempBuff1, sizeof(sysCfg.gprsUsr));
					memcpy((char*)sysCfg.gprsPwd, tempBuff2, sizeof(sysCfg.gprsPwd));
					sysCfg.reportInterval = t1;
					INFO("\n\rWEB->CFG GPRS APN:%s, USER:%s, PWD:%s, RepInterval:%d\n\r", sysCfg.gprsApn, sysCfg.gprsUsr, sysCfg.gprsPwd, sysCfg.reportInterval);
					len += sprintf((char*)&smsSendBuff[len], "ABCFG101 SET GPRS->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG101 SET GPRS->FAILED\n");
				}
				continue;
			}
			
			// configure emergency settings
			pt = strstr(buff, "ABCFG,102,");
			if (pt == buff)
			{
				t1 = 3;
				*tempBuff0 = 0;
				t2 = 9;
				smsScanf(pt, "ABCFG,102,%d,%[^, \t\n\r],%d", &t1, tempBuff0, &t2);
				if ((t1 < 3) && (*tempBuff0 != 0) && (t2 < 9))
				{
					sysCfg.familyPalMode = t1;
					memcpy((char*)sysCfg.whiteCallerNo, tempBuff0, sizeof(sysCfg.whiteCallerNo));
					sysCfg.language = t2;
					INFO("\n\rWEB->CFG EMER FamilyMode:%d, CallCenterNo:%s, Lang:%d\n\r", sysCfg.familyPalMode, sysCfg.whiteCallerNo, sysCfg.language);
					len += sprintf((char*)&smsSendBuff[len], "ABCFG102 SET SOS MODE->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG102 SET SOS MODE->FAILED\n");
				}
				continue;
			}
			
			// configure device user name
			pt = strstr(buff, "ABCFG,103,");
			if (pt == buff)
			{
				*tempBuff0 = 0;
				smsScanf(pt, "ABCFG,103,%[^,\t\n\r]", tempBuff0);
				if (*tempBuff0 != 0)
				{
					memcpy((char*)sysCfg.id, tempBuff0, sizeof(sysCfg.id));
					INFO("\n\rWEB->CFG User Name:%s\n\r", sysCfg.id);
					len += sprintf((char*)&smsSendBuff[len], "ABCFG103 SET DEVICE NAME->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char*)&smsSendBuff[len], "ABCFG103 SET DEVICE NAME->FAILED\n");
				}
				continue;
			}
			
			// configure contact list
			pt = strstr(buff, "ABCFG,104,");
			if (pt == buff)
			{
				t1 = 6;
				*tempBuff0 = 0;
				*tempBuff1 = 0;
				smsScanf(pt, "ABCFG,104,%d,%[^,\t\n\r],%[^,\t\n\r]", &t1, tempBuff0, tempBuff1);
				if ((t1 < 6) && (tempBuff0 != 0) && (tempBuff1 != 0))
				{
					memcpy((char*)(sysCfg.userList[t1].userName), tempBuff0, sizeof(sysCfg.userList[t1].userName));
					memcpy((char*)(sysCfg.userList[t1].phoneNo), tempBuff1, sizeof(sysCfg.userList[t1].phoneNo));
					INFO("\n\rWEB->CFG Contact:%d, Name:%s, PhoneNo:%s\n\r", t1, sysCfg.userList[t1].userName, sysCfg.userList[t1].phoneNo);
					len += sprintf((char*)&smsSendBuff[len], "ABCFG104 SET CONTACT %d->OK\n", t1);
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG104 SET CONTACT %d->FAILED\n", t1);
				}
				continue;
			}
			
			// delete contact
			pt = strstr(buff, "ABCFG,105,");
			if (pt == buff)
			{
				smsScanf(pt, "ABCFG,105,%d", &t1);
				if (t1 < 6)
				{
					*((char*)sysCfg.userList[t1].userName) = 0;
					*((char*)sysCfg.userList[t1].phoneNo) = 0;
					len += sprintf((char*)&smsSendBuff[len], "ABCFG105 DELETE CONTACT %d->OK\n", t1);
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				
				if (cmdOkFlag == 0)
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG105 DETELE CONTACT %d->FAILED\n", t1);
				}
				continue;
			}
			
			// configure server settings
			// ip
			pt = strstr(buff, "ABCFG,106,");
			if (pt == buff)
			{
				t1 = 2;
				t2 = 2;
				t3 = 0;
				t4 = 0;
				t5 = 0;
				t6 = 0;
				t7 = 0;
				smsScanf(pt, "ABCFG,106,%d,%d,%d.%d.%d.%d,%d", &t1, &t2, &t3, &t4, &t5, &t6, &t7);
				if ((t1 < 2) && (t2 < 2) && (t3 > 0) && (t3 <= 255) && (t4 <= 255) && (t5 <= 255) && (t6 <= 255) && (t7 > 0) && (t7 <= 65535))
				{
					if (t1 == 0) // data server
					{
						if (t2 == 0) // primary server
						{
							u8pt = (char *)sysCfg.priDserverIp;
							u8pt[0] = t3;
							u8pt[1] = t4;
							u8pt[2] = t5;
							u8pt[3] = t6;
							sysCfg.priDserverPort = t7;
							sysCfg.dServerUseIp = 1;
							INFO("\n\rWEB->CFG GPRS PRI DATA SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.priDserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG106 SET PRI DATA SERVER IP->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
						else // secondary server
						{
							u8pt = (char *)sysCfg.secDserverIp;
							u8pt[0] = t3;
							u8pt[1] = t4;
							u8pt[2] = t5;
							u8pt[3] = t6;
							sysCfg.secDserverPort = t7;
							sysCfg.dServerUseIp = 1;
							INFO("\n\rWEB->CFG GPRS SEC DATA SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.secDserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG106 SET SEC DATA SERVER IP->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
					}
					else // firmware server
					{
						if (t2 == 0) // primary server
						{
							u8pt = (char *)sysCfg.priFserverIp;
							u8pt[0] = t3;
							u8pt[1] = t4;
							u8pt[2] = t5;
							u8pt[3] = t6;
							sysCfg.priFserverPort = t7;
							sysCfg.fServerUseIp = 1;
							INFO("\n\rWEB->CFG GPRS PRI FIRMWARE SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.priFserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG106 SET PRI FIRMWARE SERVER IP->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
						else // secondary server
						{
							u8pt = (char *)sysCfg.secFserverIp;
							u8pt[0] = t3;
							u8pt[1] = t4;
							u8pt[2] = t5;
							u8pt[3] = t6;
							sysCfg.secFserverPort = t7;
							sysCfg.fServerUseIp = 1;
							INFO("\n\rWEB->CFG GPRS SEC FIRMWARE SERVER IP:%d.%d.%d.%d:%d\n\r",u8pt[0],u8pt[1],u8pt[2],u8pt[3],sysCfg.secFserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG106 SET SEC FIRMWARE SERVER IP->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
					}
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG106 SET SERVER IP->FAILED\n");
				}
				continue;
			}
			// domain
			pt = strstr(buff, "ABCFG,107,");
			if (pt == buff)
			{
				t1 = 2;
				t2 = 2;
				*tempBuff0 = 0;
				t3 = 0;
				smsScanf(pt, "ABCFG,107,%d,%d,%[^, \t\n\r],%d", &t1, &t2, tempBuff0, &t3);
				if ((t1 < 2) && (t2 < 2) && (*tempBuff0 != 0) && (t3 > 0) && (t3 <= 65535))
				{
					if (t1 == 0) // data server
					{
						if (t2 == 0) // primary server
						{
							memcpy((char*)sysCfg.priDserverName, tempBuff0, sizeof(sysCfg.priDserverName));
							sysCfg.priDserverPort = t3;
							sysCfg.dServerUseIp = 0;
							INFO("\n\rWEB->CFG GPRS PRI DATA SERVER DOMAIN:%s:%d\n\r", sysCfg.priDserverName, sysCfg.priDserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG107 SET PRI DATA SERVER DOMAIN->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
						else // secondary server
						{
							memcpy((char*)sysCfg.secDserverName, tempBuff0, sizeof(sysCfg.secDserverName));
							sysCfg.secDserverPort = t3;
							sysCfg.dServerUseIp = 0;
							INFO("\n\rWEB->CFG GPRS SEC DATA SERVER DOMAIN:%s:%d\n\r", sysCfg.secDserverName, sysCfg.secDserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG107 SET SEC DATA SERVER DOMAIN->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
					}
					else // firmware server
					{
						if (t2 == 0) // primary server
						{
							memcpy((char*)sysCfg.priFserverName, tempBuff0, sizeof(sysCfg.priFserverName));
							sysCfg.priFserverPort = t3;
							sysCfg.fServerUseIp = 0;
							INFO("\n\rWEB->CFG GPRS PRI FIRMWARE SERVER DOMAIN:%s:%d\n\r", sysCfg.priFserverName, sysCfg.priFserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG107 SET PRI FIRMWARE SERVER DOMAIN->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
						else // secondary server
						{
							memcpy((char*)sysCfg.secFserverName, tempBuff0, sizeof(sysCfg.secFserverName));
							sysCfg.secFserverPort = t3;
							sysCfg.fServerUseIp = 0;
							INFO("\n\rWEB->CFG GPRS SEC FIRMWARE SERVER DOMAIN:%s:%d\n\r", sysCfg.secFserverName, sysCfg.secFserverPort);
							len += sprintf((char *)&smsSendBuff[len], "ABCFG107 SET SEC FIRMWARE SERVER DOMAIN->OK\n");
							flagCfgSave = 1;
							cmdOkFlag = 1;
							tcpSocketStatus[DATA_SOCKET_NUM] = SOCKET_CLOSE;
						}
					}
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG107 SET SERVER DOMAIN->FAILED\n");
				}
				continue;
			}
			
			// configure feature set
			// settings on/off
			pt = strstr(buff, "ABCFG,108,");
			if (pt == buff)
			{
				smsScanf(pt, "ABCFG,108,%d", &t1);
				{
					t1 &= ((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 11));
					sysCfg.featureSet &= ~((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 11));
					sysCfg.featureSet |= t1;
					INFO("\n\rWEB->CFG FEATURESET:%d\n\r", sysCfg.featureSet);
					len += sprintf((char *)&smsSendBuff[len], "ABCFG108 SET FEATURE SET->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				
				if (cmdOkFlag == 0)
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG108 SET FEATURE SET->FAILED\n");
				}
				continue;
			}
			// lone worker interval
			pt = strstr(buff, "ABCFG,109,");
			if (pt == buff)
			{
				t1 = 0;
				smsScanf(pt, "ABCFG,109,%d", &t1);
				if ((t1 > 0) && (t1 <= 720))
				{
					sysCfg.loneWorkerInterval = t1;
					INFO("\n\rWEB->CFG LoneWorkerInterval:%d\n\r", sysCfg.loneWorkerInterval);
					len += sprintf((char *)&smsSendBuff[len], "ABCFG109 SET LONE WORKER INTERVAL->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG109 SET LONE WORKER INTERVAL->FAILED\n");
				}
				continue;
			}
			// lone worker countdown time
// 			pt = strstr(buff, "ABCFG,110,");
// 			if (pt == buff)
// 			{
// 				t1 = 0;
// 				smsScanf(pt, "ABCFG,110,%d", &t1);
// 				if ((t1 > 5) && (t1 <= 30))
// 				{
// 					sysCfg.alarmCountdownTime = t1;
// 					INFO("\n\rWEB->CFG LoneWorkerInterval:%d\n\r", sysCfg.alarmCountdownTime);
// 					len += sprintf((char *)&smsSendBuff[len], "ABCFG110 SET LONE WORKER COUNTDOWN->OK\n");
// 					flagCfgSave = 1;
// 					cmdOkFlag = 1;
// 				}
// 				else
// 				{
// 					len += sprintf((char *)&smsSendBuff[len], "ABCFG110 SET LONE WORKER COUNTDOWN->FAILED\n");
// 				}
// 				continue;
// 			}
			
			// no movement interval
			pt = strstr(buff, "ABCFG,111,");
			if (pt == buff)
			{
				t1 = 0;
				smsScanf(pt, "ABCFG,111,%d", &t1);
				if ((t1 > 0) && (t1 <= 720))
				{
					sysCfg.noMovementInterval = (unsigned short)t1;
					INFO("\n\rWEB->CFG NoMovementInterval:%d\n\r", sysCfg.noMovementInterval);
					len += sprintf((char *)&smsSendBuff[len], "ABCFG111 SET NO MOVEMENT INTERVAL->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG111 SET NO MOVEMENT INTERVAL->FAILED\n");
				}
				continue;
			}
			
			// configure GEO fence settings
			// geo fence type
			pt = strstr(buff, "ABCFG,112,");
			if (pt == buff)
			{
				t1 = 3;
				smsScanf(pt, "ABCFG,112,%d", &t1);
				if (t1 < 3)
				{
					sysCfg.geoFenceType = t1;
					INFO("\n\rWEB->CFG GeoFenceType:%d\n\r", sysCfg.geoFenceType);
					len += sprintf((char *)&smsSendBuff[len], "ABCFG112 SET GEO FENCE TYPE->OK\n");
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG112 SET GEO FENCE TYPE->FAILED\n");
				}
				continue;
			}
			
			// geo fence parameters
			pt = strstr(buff, "ABCFG,113,");
			if (pt == buff)
			{
				t1 = 10;
				t2 = 2;
				fTemp1 = 0;
				fTemp2 = 0;
				fTemp3 = 0;
				t3 = 0;
				smsScanf(pt, "ABCFG,113,%d,%d,%lf,%lf,%f,%d", &t1, &t2, &fTemp1, &fTemp2, &fTemp3, &t3);
				if ((t1 < 10) && (t2 < 2) && (fTemp1 != 0) && (fTemp2 != 0) && (fTemp3 > 0) && (fTemp3 <= 100000) && (t3 > 0) && (t3 <= 720))
				{
					sysCfg.geoFence[t1].geoFenceEnable = t2;
					sysCfg.geoFence[t1].lat = fTemp1;
					sysCfg.geoFence[t1].lon = fTemp2;
					sysCfg.geoFence[t1].radius = fTemp3;
					sysCfg.geoFence[t1].time = t3;
					INFO("\n\rWEB->CFG GeoFence:%d, Enable:%d, Lat:%f, Lon:%f, Radius:%d, Time:%d\n\r", t1, sysCfg.geoFence[t1].geoFenceEnable, sysCfg.geoFence[t1].lat, sysCfg.geoFence[t1].lon, sysCfg.geoFence[t1].radius, sysCfg.geoFence[t1].time);
					len += sprintf((char *)&smsSendBuff[len], "ABCFG113 SET GEO FENCE %d->OK\n", t1);
					flagCfgSave = 1;
					cmdOkFlag = 1;
				}
				else
				{
					len += sprintf((char *)&smsSendBuff[len], "ABCFG113 SET GEO FENCE %d->FAILED\n", t1);
				}
				continue;
			}
			//ABCFG,117,16,12,13,01,51,05
			// Set date time parameters
			pt = strstr(buff, "ABCFG,117,");
			if (pt == buff)
			{
				smsScanf(pt, "ABCFG,117,%d,%d,%d,%d,%d,%d,%d", &t1, &t2, &t3, &t4, &t5, &t6,&t7);
				sysTime.mday = t1;
				sysTime.month = t2;
				sysTime.year = t3 + 2000;
				sysTime.hour = t4;
				sysTime.min = t5;
				sysTime.sec = t6;
				timeZone = t7;
				dbTimeZone.value = timeZone;
				if (sysTime.year >= 2013 && sysTime.mday <= 31 && sysTime.month <= 12 && sysTime.hour <= 23 && sysTime.min <= 59 && sysTime.sec <= 59)
				{
					t1 = TIME_GetSec(&sysTime);
					UpdateRtcTime(t1);
					//len += sprintf((char *)&smsSendBuff[len], "TIME PARSER:%02d/%02d/%02d,%02d:%02d:%02d -->OK\n",sysTime.mday,sysTime.month,sysTime.year,sysTime.hour % 100,sysTime.min,sysTime.sec);
					cmdOkFlag = 1;
				}
//				else
//				{
//					len += sprintf((char *)&smsSendBuff[len], "TIME PARSER:%02d/%02d/%02d,%02d:%02d:%02d -->FAILED\n",sysTime.mday,sysTime.month,sysTime.year,sysTime.hour % 100,sysTime.min,sysTime.sec);
//				}
				continue;
			}
			
			
			if (pt == NULL)
			{
				break;
			}
		}
		
		if(len >= smsLenBuf)
		{		
			len = smsLenBuf;
		}
		smsSendBuff[len] = 0;
		*dataOutLen = len;
		if(flagCfgSave)
		{
			// config is update from web
			// --> stop upload config
			// --> stop download config
			sysUploadConfig = 0;
			CFG_Save();
		}
		if(flagResetDevice)
		{
			return 0xa5;
		}
		return 0;
	}
}

// char* mstrtok (char **str, const char *delimiters)
// {
// 	char *result = NULL;
// 	
// 	if (str != NULL)
// 	{
// 		result = strtok(*str, delimiters);
// 	}
// 	
// 	if (result != NULL)
// 	{
// 		*str += strlen(result) + 1;
// 	}
// 	
// 	return result;
// }
