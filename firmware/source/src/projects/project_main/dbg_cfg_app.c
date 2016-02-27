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
#include "project_common.h"
#include "dbg_cfg_app.h"
#include "gps.h"
#include "uart.h"
#include "sys_time.h"
#include "sst25.h"
#include "db.h"
#include "tracker.h"
#include "rtc.h"
#include "cdc.h"
#include "cdcuser.h"
#include "audio.h"
#include "hw_config.h"
#include "DTMF_app.h"
#include "accelerometer.h"
#include "gsm_gprs_tasks.h"
#include "pedometer_task.h"

#define PACKET_NACK	1
#define PACKET_ACK	2
#define PACKET_NON	0
#define receivedBuf	HID_receivedBuf

extern uint8_t movementToCheckGps;
extern uint8_t hasMoved;

CFG_PROTOCOL_TYPE USB_ProtoRecv;
CFG_PROTOCOL_TYPE USB_ProtoSend;
uint8_t USB_DataBuff[USB_PACKET_SIZE];
PARSER_PACKET_TYPE USB_parserPacket;

uint8_t comBuf[256];
RINGBUF comRingBuf;
uint32_t timer1Cnt = 0;

uint8_t request = 0;
uint32_t packetLen;
uint32_t vpacketNo;
uint32_t oldPacketNo = 0;
uint8_t* packetPtr;
uint32_t packetStatus = PACKET_NON;

uint8_t logEnable = 0;

uint8_t usbRecv = 0;
uint8_t PrevXferComplete = 0;
uint32_t enableUSB_Manager = 0;

// uint32_t DbgCfgPutChar(uint8_t c)
// {
// 	if(logEnable == 4)
// 	{
// 		return RINGBUF_Put(&comRingBuf,  c);
// 	}
// 	return 0;
// }

uint32_t  DbgCfgPrintf(const uint8_t *format, ...)
{
	static  uint8_t  buffer[256];
	uint32_t len,i;
	__va_list     vArgs;
	#ifndef DEVICE_TESTING
	if(CHG_EN_IN)
	{
//		len = sprintf((char *)buffer, "\r\nTIME:%02d:%02d:%02d->",sysTime.hour,sysTime.min,sysTime.sec);
//		for(i = 0;i < len; i++)
//		{
//			if(logEnable == 4)
//			{
//				RINGBUF_Put(&comRingBuf,  buffer[i]);
//			}
//		}
		va_start(vArgs, format);
		len = vsprintf((char *)&buffer[0], (char const *)format, vArgs);
		va_end(vArgs);
		
		if(len >= 255) len = 255;
		for(i = 0;i < len; i++)
		{
			if(logEnable == 4)
			{
				RINGBUF_Put(&comRingBuf,  buffer[i]);
			}
		}
	}
	#else
	va_start(vArgs, format);
	len = vsprintf((char *)&buffer[0], (char const *)format, vArgs);
	va_end(vArgs);
	uart_putstr(UART1_BASE_PTR,buffer);
	#endif
	return 0;
}

uint8_t CfgParserPacket(PARSER_PACKET_TYPE *parserPacket,CFG_PROTOCOL_TYPE *cfgProtoRecv,uint8_t c)
{
	switch(parserPacket->state)
	{
		case CFG_CMD_WAITING_SATRT_CODE:
			if(c == 0xCA)
			{
				parserPacket->state = CFG_CMD_GET_LENGTH;
				parserPacket->len = 0;
				parserPacket->crc = 0;
				parserPacket->cnt = 0;
			}
			break;
		case CFG_CMD_GET_LENGTH:
				parserPacket->len |= (uint16_t)c<<parserPacket->cnt;
				parserPacket->cnt += 8;
				if(parserPacket->cnt >= 16)
				{
					parserPacket->state = CFG_CMD_GET_OPCODE;
					parserPacket->cnt = 0;
				}
			break;
		case CFG_CMD_GET_OPCODE:		
					parserPacket->opcode = c;
					parserPacket->state = CFG_CMD_GET_DATA;
			break;
		case CFG_CMD_GET_DATA:
				if((parserPacket->cnt >= parserPacket->len) || (parserPacket->len > parserPacket->lenMax))
				{
					parserPacket->state = CFG_CMD_WAITING_SATRT_CODE;
				}
				else
				{
					parserPacket->crc += c;
					cfgProtoRecv->dataPt[parserPacket->cnt]= c;
					parserPacket->cnt++;
					if(parserPacket->cnt == parserPacket->len)
					{
						parserPacket->state = CFG_CMD_CRC_CHECK;
					}
				}
			break;
		case CFG_CMD_CRC_CHECK:
				parserPacket->state= CFG_CMD_WAITING_SATRT_CODE;
				if(parserPacket->crc  == c)
				{	
					cfgProtoRecv->length = parserPacket->len;
					cfgProtoRecv->opcode = parserPacket->opcode;
					return 0;
				}
			break;
		default:
			parserPacket->state = CFG_CMD_WAITING_SATRT_CODE;
			break;				
	}
	return 0xff;
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

CFG_DATA_TYPE	cfgSendData;


uint32_t recordFileSize;
uint32_t recordFileOfset;
// uint32_t firmwareFileCrc;
// uint32_t firmwareFileSize;
// uint32_t firmwareFileOffSet;
// uint32_t firmwareFileOffSetOld;
// uint32_t firmwareSaveFileOffsetCnt = 0;
// uint32_t firmwareStatus = 0xFFFFFFFF;


// void LoadFirmwareFile(void)
// {
// 	uint8_t buff[8];
// 	int32_t i;
// 	uint32_t *u32Pt = (uint32_t *)buff;
// 	SST25_Read((FIRMWARE_INFO_ADDR + FIRMWARE_STATUS_OFFSET)*4,buff,4);
// 	firmwareStatus = *u32Pt;
// 	if(firmwareStatus == 0xA5A5A5A5)
// 	{
// 		SST25_Read((FIRMWARE_INFO_ADDR + FIRMWARE_FILE_SIZE_OFFSET)*4,buff, 4);
// 		firmwareFileSize = *u32Pt;
// 		if(firmwareFileSize >= 128*1024*1024)
// 		{
// 			firmwareFileOffSet = 0;
// 			firmwareFileSize = 0;
// 			firmwareSaveFileOffsetCnt =0;
// 		}
// 		else
// 		{
// 			SST25_Read(FIRMWARE_INFO_ADDR+FIRMWARE_CRC_OFFSET*4,buff,4);
// 			firmwareFileCrc = *u32Pt;
// 			if(firmwareFileCrc == 0xffffffff)
// 			{
// 				firmwareStatus   = 0;
// 				firmwareFileOffSet = 0;
// 				firmwareFileOffSetOld = 0;
// 				firmwareFileSize = 0;
// 				firmwareSaveFileOffsetCnt =0;
// 				return;
// 			}
// 			for(i = 1023 ;i >= 0;i--)
// 			{
// 				SST25_Read(FIRMWARE_OFFSET_COUNTER_BASE_ADDR + i*4,buff, 4);
// 				if(*u32Pt != 0xffffffff)
// 				{
// 					firmwareFileOffSet = *u32Pt;
// 					firmwareSaveFileOffsetCnt = i;
// 					if(firmwareFileOffSet > firmwareFileSize)
// 					{
// 							firmwareFileOffSet = 0;
// 							firmwareSaveFileOffsetCnt = 0;
// 					}	
// 					firmwareFileOffSetOld = firmwareFileOffSet;
// 					break;
// 				}
// 			}
// 		}
// 	}
// 	else
// 	{
// 		firmwareFileOffSet = 0;
// 		firmwareFileOffSetOld = 0;
// 		firmwareFileSize = 0;
// 		firmwareSaveFileOffsetCnt =0;
// 	}
// }

void CfgUSBSendData(CFG_PROTOCOL_TYPE *data)
{
	uint8_t i;
	if(data->length)
	{
		RINGBUF_Put(&HID_TxRingBuff,data->start);
		RINGBUF_Put(&HID_TxRingBuff,((uint8_t *)&data->length)[0]);
		RINGBUF_Put(&HID_TxRingBuff,((uint8_t *)&data->length)[1]);
		RINGBUF_Put(&HID_TxRingBuff,data->opcode);
		for(i = 0;i < data->length;i++)
			RINGBUF_Put(&HID_TxRingBuff,data->dataPt[i]);
		RINGBUF_Put(&HID_TxRingBuff,data->crc);
		data->length = 0;
	}
}


CFG_DATA_TYPE	pt;
uint8_t CfgProcessData(CFG_PROTOCOL_TYPE *cfgProtoRecv,CFG_PROTOCOL_TYPE *cfgProtoSend,uint8_t *cfgSendDataBuff,uint32_t maxPacketSize,uint8_t logSendEnable)
{
	static uint8_t flagCfgProcessInUse = 0;
	uint8_t *u8pt;
	//uint8_t buf[FLASH_PACKET_SIZE];
	uint32_t i,u32Temp;
	if(flagCfgProcessInUse) 
	{
		cfgProtoRecv->opcode = 0;
		return 0;
	}
	flagCfgProcessInUse = 1;
	
// 	if(cfgProtoRecv->opcode && logSendEnable)
// 	{
// 		logEnable = 0;
// 	}
	
	switch(cfgProtoRecv->opcode)
	{
		case 0x00:
						if(logEnable && logSendEnable)
						{
							i = 0;
							while(RINGBUF_Get(&comRingBuf, &cfgSendDataBuff[i]) == 0)
							{
								i++;
								if(i >= maxPacketSize) break;
							}
							if(i)
							{
								cfgProtoSend->opcode = 0x03;
								cfgProtoSend->start = 0xCA;
								cfgProtoSend->length = i;
								cfgProtoSend->dataPt = cfgSendDataBuff;
								cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
							}
						}		
			break;
		case 0x01:
						logEnable = 0;
			break;
		case 0x02:
						logEnable  = 1;
			break;
		case 0x04:
						logEnable  = 2;
			break;
		case 0x05:
						logEnable  = 3;
			break;
		case 0x06:
						logEnable  = 4;
			break;
		case 0x0C: //read config
			pt.packetNo = cfgProtoRecv->dataPt[0];
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
			pt.packetLengthToSend = cfgProtoRecv->dataPt[4];
			pt.packetLengthToSend |= (uint32_t)cfgProtoRecv->dataPt[5]<<8;
			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
			if(pt.packetNo == 0xA5A5A5A5)
			{
				//logEnable = 4;
				i = sizeof(CONFIG_POOL);
				cfgSendData.dataPt = (uint8_t *)&sysCfg;	
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0C;
				cfgProtoSend->length = 12;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				//Max file size
				i = sizeof(CONFIG_POOL);
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[4] = u8pt[0];
				cfgSendDataBuff[5] = u8pt[1];
				cfgSendDataBuff[6] = u8pt[2];
				cfgSendDataBuff[7] = u8pt[3];
				//Max packet size
				i = maxPacketSize;
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[8] = u8pt[0];
				cfgSendDataBuff[9] = u8pt[1];
				cfgSendDataBuff[10] = u8pt[2];
				cfgSendDataBuff[11] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else if(pt.packetNo == 0x5A5A5A5A)
			{
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0C;
				cfgProtoSend->length = 4;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else if(((pt.packetNo + pt.packetLengthToSend) <= sizeof(CONFIG_POOL)) && (pt.packetLengthToSend <= maxPacketSize))
			{
				u8pt = (uint8_t *)&sysCfg + pt.packetNo;
				for(i = 0;i < pt.packetLengthToSend;i++)
				{
					cfgSendDataBuff[i+4] = u8pt[i];
				}
				cfgProtoSend->length = i + 4;
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0C;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else
			{
				
				pt.packetNo = 0xFFFFFFFF;
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0C;
				cfgProtoSend->length = 4;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
		break;
		case 0x0D:
			pt.packetNo = cfgProtoRecv->dataPt[0];
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
			if(pt.packetNo == 0xA5A5A5A5)
			{		
				cfgSendData.dataPt = (uint8_t *)&sysCfg;	
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0D;
				cfgProtoSend->length = 12;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				//Max file size
				i = sizeof(CONFIG_POOL);
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[4] = u8pt[0];
				cfgSendDataBuff[5] = u8pt[1];
				cfgSendDataBuff[6] = u8pt[2];
				cfgSendDataBuff[7] = u8pt[3];
				//Max packet size
				i = maxPacketSize;
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[8] = u8pt[0];
				cfgSendDataBuff[9] = u8pt[1];
				cfgSendDataBuff[10] = u8pt[2];
				cfgSendDataBuff[11] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else if(((pt.packetNo + cfgProtoRecv->length - 4) <= sizeof(CONFIG_POOL)))
			{
				u8pt = (uint8_t *)&sysCfg + pt.packetNo;
				for(i = 0;i < cfgProtoRecv->length - 4;i++)
				{
					u8pt[i] = pt.dataPt[i];
				}
				
				 pt.packetNo += i;
				if(pt.packetNo >= sizeof(CONFIG_POOL))
				{
					sysUploadConfig = 2;
					CFG_Save();
					cfgProtoSend->start = 0xCA;
					cfgProtoSend->opcode = 0x0D;
					cfgProtoSend->length = 4;
					u8pt = (uint8_t *)&pt.packetNo;
					cfgSendDataBuff[0] = u8pt[0];
					cfgSendDataBuff[1] = u8pt[1];
					cfgSendDataBuff[2] = u8pt[2];
					cfgSendDataBuff[3] = u8pt[3];
					cfgProtoSend->dataPt = cfgSendDataBuff;
					cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
					break;
				}
				if(pt.packetNo > (sizeof(CONFIG_POOL) - maxPacketSize))
				{
					i = (sizeof(CONFIG_POOL) -  pt.packetNo);
				}
				else
				{
					i = maxPacketSize;
				}
				cfgProtoSend->length = 8;
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0D;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				//Max packet size
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[4] = u8pt[0];
				cfgSendDataBuff[5] = u8pt[1];
				cfgSendDataBuff[6] = u8pt[2];
				cfgSendDataBuff[7] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else
			{
				pt.packetNo = 0xFFFFFFFF;
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0D;
				cfgProtoSend->length = 4;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
		break;
		case 0x0F:
			pt.packetNo = cfgProtoRecv->dataPt[0];
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
			if(pt.packetNo == 0xA5A5A5A5)
			{
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0F;
				cfgProtoSend->length = 12;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				//Max file size
				u32Temp = trackerLog.ring.head;;
				u8pt = (uint8_t *)&u32Temp;
				cfgSendDataBuff[4] = u8pt[0];
				cfgSendDataBuff[5] = u8pt[1];
				cfgSendDataBuff[6] = u8pt[2];
				cfgSendDataBuff[7] = u8pt[3];
				//Max packet size
				i = maxPacketSize;
				u8pt = (uint8_t *)&i;
				cfgSendDataBuff[8] = u8pt[0];
				cfgSendDataBuff[9] = u8pt[1];
				cfgSendDataBuff[10] = u8pt[2];
				cfgSendDataBuff[11] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);

			}
			else if(pt.packetNo == 0x5A5A5A5A)
			{
				cfgProtoSend->start = 0xCA;
				cfgProtoSend->opcode = 0x0F;
				cfgProtoSend->length = 4;
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
			else
			{
				SST25_Read(TRACKER_DATA_ADDR + pt.packetNo,&cfgSendDataBuff[4],maxPacketSize);
				if((pt.packetNo >= TRACKER_DATA_ADDR) >= TRACKER_DATA_SIZE_MAX)
				{
					pt.packetNo = 0xffffffff;
					cfgProtoSend->length = 4;
				}
				else
				{
					SST25_Read(TRACKER_DATA_ADDR + pt.packetNo,&cfgSendDataBuff[4],maxPacketSize);
					cfgProtoSend->length = maxPacketSize + 4;
				}
				u8pt = (uint8_t *)&pt.packetNo;
				cfgSendDataBuff[0] = u8pt[0];
				cfgSendDataBuff[1] = u8pt[1];
				cfgSendDataBuff[2] = u8pt[2];
				cfgSendDataBuff[3] = u8pt[3];
				cfgProtoSend->dataPt = cfgSendDataBuff;
				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
			}
		break;
			
		case 0xFD:					// factory reset
			pt.packetNo = cfgProtoRecv->dataPt[0];
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
			if(pt.packetNo == 0xA5A5A5A5)
			{
				DB_LogRingReset(&trackerLog);
				SST25_Erase(MILEAGE_ADDR,block4k);
				SST25_Erase(FIRMWARE_INFO_ADDR,block4k);
				SST25_Erase(RESET_CNT_ADDR,block4k);
				DB_ResetCntLoad();
				DB_ResetCntSave();
				DB_MileageLoad();
				DB_MileageSave();
				memset((void*)&sysCfg, 0xFF, sizeof sysCfg);
				sysUploadConfig = 2;
				CFG_Save();
				NVIC_SystemReset();
			}
			break;
		
		case 0xFE:					// Reset device
			pt.packetNo = cfgProtoRecv->dataPt[0];
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
			if(pt.packetNo == 0xA5A5A5A5)
			{
// 				if(firmwareStatus == 0x5A5A5A5A)
// 				{
// 					DelayMs(1000);
// 					//JumpToBootloader();
// 				}
				NVIC_SystemReset();
			}
			
			break;
		case 0x11:
			cfgProtoSend->start = 0xCA;
			cfgProtoSend->opcode = 0x11;
			cfgProtoSend->length = strlen(FIRMWARE_VERSION);
			cfgProtoSend->dataPt = FIRMWARE_VERSION;
			cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);	
		break;
// 		case 0x12:
// 			pt.packetNo = cfgProtoRecv->dataPt[0];
// 			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[1]<<8;
// 			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[2]<<16;
// 			pt.packetNo |= (uint32_t)cfgProtoRecv->dataPt[3]<<24;
// 			pt.dataPt = (uint8_t *)cfgProtoRecv->dataPt + 4;
// 			if(pt.packetNo == 0xA5A5A5A5)
// 			{
// 				firmwareStatus = 0xA5A5A5A5;
// 				firmwareFileOffSet = 0;
// 				firmwareFileSize = pt.dataPt[0];
// 				firmwareFileSize |= pt.dataPt[1]<<8;
// 				firmwareFileSize |= pt.dataPt[2]<<16;
// 				firmwareFileSize |= pt.dataPt[3]<<24;
// 				 
// 				firmwareFileCrc = pt.dataPt[4];
// 				firmwareFileCrc |= pt.dataPt[5]<<8;
// 				firmwareFileCrc |= pt.dataPt[6]<<16;
// 				firmwareFileCrc |= pt.dataPt[7]<<24;
// 				
// 				SST25_Erase(FIRMWARE_INFO_ADDR,block4k);
// 				firmwareSaveFileOffsetCnt = 0;
// 				u8pt = (uint8_t *)&firmwareSaveFileOffsetCnt;
// 				SST25_Write(FIRMWARE_OFFSET_COUNTER_BASE_ADDR,u8pt,4);
// 				
// 				u8pt = (uint8_t *)&firmwareFileSize;
// 				SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_FILE_SIZE_OFFSET*4,u8pt,4);
// 				i = 0xA5A5A5A5;
// 				u8pt = (uint8_t *)&i;
// 				SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_STATUS_OFFSET*4,u8pt,4);
// 				
// 				//Write CRC
// 				u8pt = (uint8_t *)&firmwareFileCrc;
// 				SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_CRC_OFFSET*4,u8pt,4);
// 				
// 				cfgProtoSend->start = 0xCA;
// 				cfgProtoSend->opcode = 0x12;
// 				cfgProtoSend->length = 12;
// 				u8pt = (uint8_t *)&pt.packetNo;
// 				cfgSendDataBuff[0] = u8pt[0];
// 				cfgSendDataBuff[1] = u8pt[1];
// 				cfgSendDataBuff[2] = u8pt[2];
// 				cfgSendDataBuff[3] = u8pt[3];
// 				//Max file size
// 				i = firmwareFileSize;
// 				u8pt = (uint8_t *)&i;
// 				cfgSendDataBuff[4] = u8pt[0];
// 				cfgSendDataBuff[5] = u8pt[1];
// 				cfgSendDataBuff[6] = u8pt[2];
// 				cfgSendDataBuff[7] = u8pt[3];
// 				//Max packet size
// 				i = maxPacketSize;
// 				u8pt = (uint8_t *)&i;
// 				cfgSendDataBuff[8] = u8pt[0];
// 				cfgSendDataBuff[9] = u8pt[1];
// 				cfgSendDataBuff[10] = u8pt[2];
// 				cfgSendDataBuff[11] = u8pt[3];
// 				cfgProtoSend->dataPt = cfgSendDataBuff;
// 				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
// 			}
// 			else
// 			{
// 				if(pt.packetNo == firmwareFileOffSet)
// 				{
// 					i = pt.packetNo + (uint32_t)cfgProtoRecv->length  - 4;
// 					firmwareFileOffSet = i;
// 					u32Temp = cfgProtoRecv->length - 4;
// 					
// // 					u8pt = 0;
// // 					for(i = 0;i < u32Temp;i++)
// // 					{
// // 						if(u8pt[pt.packetNo + i] != pt.dataPt[i])
// // 							break;
// // 					}
// 					if((maxPacketSize >= FLASH_PACKET_SIZE) && (u32Temp >= FLASH_PACKET_SIZE))
// 					{		
// 							for( i = 0;i < u32Temp; i += FLASH_PACKET_SIZE)
// 							{
// 								SST25_Write(pt.packetNo + FIRMWARE_BASE_ADDR + i,pt.dataPt + i,FLASH_PACKET_SIZE);
// 								SST25_Read(pt.packetNo + FIRMWARE_BASE_ADDR + i,buf, FLASH_PACKET_SIZE);
// 								for(j = 0;j < FLASH_PACKET_SIZE;j++)
// 								{
// 									if(pt.dataPt[i + j] != buf[j])
// 									{
// 										firmwareFileOffSet = firmwareFileOffSetOld;
// 										i = u32Temp;
// 									}
// 								}
// 								//DrvUART_Write(UART_PORT0,pt.dataPt + i,FLASH_PACKET_SIZE);
// 							}
// 					}
// 					if((firmwareFileOffSet - firmwareFileOffSetOld) >= 4096)
// 					{		
// 						firmwareFileOffSetOld = (firmwareFileOffSet / 4096) * 4096;
// 						firmwareSaveFileOffsetCnt++;
// 						if(firmwareSaveFileOffsetCnt >= 1024) firmwareSaveFileOffsetCnt = 0;
// 						u8pt = (uint8_t *)&firmwareFileOffSetOld;
// 						DelayMs(1);
// 						SST25_Write(FIRMWARE_OFFSET_COUNTER_BASE_ADDR + firmwareSaveFileOffsetCnt*4,u8pt,4);
// 					}
// 				}
// 				pt.packetNo = firmwareFileOffSet;
// 				
// 				if(firmwareFileOffSet >= firmwareFileSize)
// 				{
// 					u32Temp = 0;
// 					for(i = 0; i < FLASH_PACKET_SIZE;i++)
// 					{
// 						cfgSendDataBuff[i] = 0xff;
// 					}
// 					for(i = 0; i < 1024;i += FLASH_PACKET_SIZE)
// 					{
// 						DelayMs(1);
// 						SST25_Write(firmwareFileSize + FIRMWARE_BASE_ADDR + i,cfgSendDataBuff,FLASH_PACKET_SIZE);
// 					}
// 					for(i = 0; i < firmwareFileSize;i += FLASH_PACKET_SIZE)
// 					{
// 						SST25_Read(i + FIRMWARE_BASE_ADDR,cfgSendDataBuff, FLASH_PACKET_SIZE);
// 						for(j = 0;j < FLASH_PACKET_SIZE;j++)
// 						{
// // 							u8pt = 0;
// // 							if(u8pt[j + i] != cfgSendDataBuff[j])
// // 								DelayMs(1);
// 							if((i + j) < firmwareFileSize)
// 								u32Temp += cfgSendDataBuff[j];
// 							else
// 								break;
// 						}
// 						if((i + j) >= firmwareFileSize)
// 							break;
// 					}
// 					if(u32Temp == firmwareFileCrc)
// 					{
// 						firmwareStatus = 0x5A5A5A5A;
// 						pt.packetNo = 0x5A5A5A5A;
// 						firmwareFileOffSet = 0;
// 						SST25_Erase(FIRMWARE_INFO_ADDR,block4k);
// 						
// 						//Write file size
// 						u8pt = (uint8_t *)&firmwareFileSize;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_FILE_SIZE_OFFSET*4,u8pt,4);
// 						//Write CRC
// 						u8pt = (uint8_t *)&firmwareFileCrc;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_CRC_OFFSET*4,u8pt,4);
// 						//Write Start addr
// 						i = FIRMWARE_BASE_ADDR;
// 						u8pt = (uint8_t *)&i;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_START_ADDR_OFFSET*4,u8pt,4);
// 						//Write status
// 						i = 0x5A5A5A5A;
// 						u8pt = (uint8_t *)&i;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_STATUS_OFFSET*4,u8pt,4);

// 						SST25_Erase(RESET_CNT_ADDR,block4k);
// 						DB_ResetCntLoad();
// 						DB_ResetCntSave();
// 					}
// 					else
// 					{
// 						firmwareStatus = 0xA5A5A5A5;
// 						pt.packetNo = 0;
// 						firmwareFileOffSet = 0;
// 						SST25_Erase(FIRMWARE_INFO_ADDR,block4k);
// 						firmwareSaveFileOffsetCnt = 0;
// 						u8pt = (uint8_t *)&firmwareSaveFileOffsetCnt;
// 						SST25_Write(FIRMWARE_OFFSET_COUNTER_BASE_ADDR,u8pt,4);
// 						
// 						u8pt = (uint8_t *)&firmwareFileSize;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_FILE_SIZE_OFFSET*4,u8pt,4);
// 						i = 0xA5A5A5A5;
// 						u8pt = (uint8_t *)&i;
// 						SST25_Write(FIRMWARE_INFO_ADDR+FIRMWARE_STATUS_OFFSET*4,u8pt,4);
// 					}
// 				}				
// 				cfgProtoSend->start = 0xCA;
// 				cfgProtoSend->opcode = 0x12;
// 				cfgProtoSend->length = 4;
// 				u8pt = (uint8_t *)&pt.packetNo;
// 				cfgSendDataBuff[0] = u8pt[0];
// 				cfgSendDataBuff[1] = u8pt[1];
// 				cfgSendDataBuff[2] = u8pt[2];
// 				cfgSendDataBuff[3] = u8pt[3];
// 				cfgProtoSend->dataPt = cfgSendDataBuff;
// 				cfgProtoSend->crc = CfgCalcCheckSum(cfgProtoSend->dataPt,cfgProtoSend->length);
// 			}
// 		break;
		default:
			break;
	}
	cfgProtoRecv->opcode = 0;
	flagCfgProcessInUse = 0;
	return 0;
}

void vHID_Task(void)
{
	uint8_t buff[65],len;
	uint8_t c;
	while(RINGBUF_Get(&HID_RxRingBuff,&c) == 0)
	{
		if(CfgParserPacket(&USB_parserPacket,&USB_ProtoRecv,c) == 0)
				break;
	}
	CfgProcessData(&USB_ProtoRecv,&USB_ProtoSend,USB_DataBuff,USB_parserPacket.lenMax - sizeof(cfgSendData.packetNo),1);
	CfgUSBSendData(&USB_ProtoSend);

	if(HID_DepInEmpty)
	{
		len = 0;
		while((RINGBUF_Get(&HID_TxRingBuff,&c) == 0))
		{
			buff[len] = c;
			len++;
			if(len >= 64) break;
		}
		if(len)
		{
			HID_DepInEmpty = 0;
			USB_WriteEP (0x81,buff,64);
		}
	}
}



uint32_t timer2Cnt;
//uint32_t fCnt = 0;

uint32_t testBitsUart0 = 0;
uint32_t testBitsUart1 = 0;

uint32_t flashAddr;
uint8_t *flashDataPt;
uint16_t flashDataLen;
uint8_t flashWriteFlag;
uint8_t flashReadFlag;

void TPM0_IRQHandler(void)
{
	uint8_t res;
	TPM0_SC |= 1 << 7; // clear interrup
	
	timer2Cnt++;
	#ifndef DEVICE_TESTING
	if((UART0_S1 & UART_S1_OR_MASK) || 
		(UART0_S1 & UART_S1_NF_MASK) ||
		(UART0_S1 & UART_S1_FE_MASK) ||
		(UART0_S1 & UART_S1_PF_MASK)
	)
	{
		testBitsUart0++;
		UART0_S1 |= UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK;
// 		c = UART0_D;
	}
	
	if((UART1_S1 & UART_S1_OR_MASK) || 
			(UART1_S1 & UART_S1_NF_MASK) ||
			(UART1_S1 & UART_S1_FE_MASK) ||
			(UART1_S1 & UART_S1_PF_MASK)
	)
	{
		testBitsUart1++;
		UART1_S1 |= UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK;
// 		c = UART1_D;
	}
	
	if((timer2Cnt % 1000) == 0)
	{
		TrackerTask(); 
	}
	
	if(trackerMsg.oldMsgIsEmpty)
	{
		while(DB_LoadDataLogTail((uint8_t *)&logRecord,&trackerLog) == 0)
		{
			if(!logRecord.serverSent)
			{
				if((TIME_GetSec(&logRecord.currentTime) + 3600*24*7/*7 day*/) >= TIME_GetSec(&sysTime))
				{
					trackerMsg.oldMsg = logRecord;
					trackerMsg.oldMsgIsEmpty = 0;
					break;
				}
			}
		}
	}
	if(pwrStatus.value == POWER_ON)
	{
		if (USB_Configuration) 
		{                  /* Check if USB is configured */
			//if(enableUSB_Manager)
			vHID_Task();
		}
	}
	AudioControler();
	DTMF_Task();
	
	FlashReadWriteCtrl();
	#else
	//if((timer2Cnt % 1000) == 0 && timer2Cnt)
	{ 
		AudioControler();
		//TrackerTask();
		//PedometerTask();
	}
	#endif
}

void FlashReadWriteCtrl(void)
{
	uint16_t i,u32Cnt;
	
	if(flashWriteFlag)
	{
		u32Cnt = flashDataLen - flashDataLen % 4;
		for(i = 0;i < u32Cnt;i += 4)
		{
			SST25_Write(flashAddr + i,flashDataPt + i,4);
		}
		if(flashDataLen - u32Cnt)
		{
			for(;i < flashDataLen;i++)
				SST25_Write(flashAddr + i,flashDataPt + i,1);
		}
		flashWriteFlag = 0;
	}
	if(flashReadFlag)
	{
		u32Cnt = flashDataLen - flashDataLen % 512;
		for(i = 0;i < u32Cnt;i += 512)
		{
			SST25_Read(flashAddr + i,flashDataPt + i,512);
		}
		if(flashDataLen - u32Cnt)
			SST25_Read(flashAddr + i,flashDataPt + i,flashDataLen - u32Cnt);
		flashReadFlag = 0;
	}
}

uint8_t APP_Write2FlashMem(uint32_t addr,uint8_t *buf, uint16_t len)
{
	uint32_t time = TICK_Get();
	flashDataLen = len;
	flashAddr = addr;
	flashDataPt = buf;
	flashWriteFlag = 1;
	flashReadFlag = 0;
	while(flashWriteFlag)
	{
		if(TICK_Get() - time >= 3000)
		{
			return 1;
		}
	}
	return 0;
}

uint8_t APP_Read2FlashMem(uint32_t addr,uint8_t *buf, uint16_t len)
{
	uint32_t time = TICK_Get();
	flashDataLen = len;
	flashAddr = addr;
	flashDataPt = buf;
	flashReadFlag = 1;
	flashWriteFlag = 0;
	while(flashReadFlag)
	{
		if(TICK_Get() - time >= 3000)
		{
			return 1;
		}
	}
	return 0;
}

void TPM0_DeInit(void)
{
	timer2Cnt = 0;
	disable_irq(17);
	TPM0_SC = 0; // disable timer
	TPM0_SC |= 1 << 7; // clear interrupt, prescaler divided by 1;
	SIM_SCGC6 &= SIM_SCGC6_TPM0_MASK;
	
}

void TPM0_Init(uint32_t clock/*Hz*/,uint32_t freq/*Hz*/)
{
	timer2Cnt = 0;
	SIM_SCGC6 |= SIM_SCGC6_TPM0_MASK;
	TPM0_SC = 0; // disable timer
	TPM0_SC |= 1 << 7; // clear interrupt, prescaler divided by 1;
	TPM0_CNT = 0;
	TPM0_MOD = clock/freq - 1;
	TPM0_SC |= 1 << 6; //enable timer overflow interrupt
	TPM0_CONF = TPM_CONF_TRGSEL(8);
	NVIC_SetPriority (TPM0_IRQn, (1<< 2) - 1);
	enable_irq(17);
	TPM0_SC |= TPM_SC_CMOD(1); // enable timer
	
	RINGBUF_Init(&comRingBuf, comBuf, sizeof comBuf);
	RINGBUF_Init(&HID_TxRingBuff,HID_TxBuff,sizeof(HID_TxBuff));
	RINGBUF_Init(&HID_RxRingBuff,HID_RxBuff,sizeof(HID_RxBuff));
	USB_ProtoRecv.dataPt = USB_DataBuff;
	USB_parserPacket.state = CFG_CMD_WAITING_SATRT_CODE;
	USB_parserPacket.lenMax = USB_PACKET_SIZE;
	
}



