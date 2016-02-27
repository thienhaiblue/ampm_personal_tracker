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
#include <stdio.h>
#include <string.h>
#include "tracker.h"
#include <math.h>
#include "ringbuf.h"
#include "system_config.h"
#include "gps.h"
#include "db.h"
#include "at_command_parser.h"
#include "modem.h"
#include "hw_config.h"
#include "db.h"
#include "sos_tasks.h"
#include "gps_task.h"

extern uint8_t sosStartTime[9];
extern uint8_t sosEndTime[9];
extern uint8_t sosStartLocation[22];
extern uint8_t sosEndLocation[22];
extern SOS_ACTION sosActionSms;

// uint16_t CFG_Add(uint8_t *header,uint8_t *buf,uint8_t *data,uint16_t dataLen,uint16_t cfgSize,uint16_t cfgOffset)
// {
// 	uint32_t len = 0;
// 	uint16_t i;
// 	len = sprintf((char *)buf,"%s,",header);
// 	len += sprintf((char *)&buf[len], "%s,", sysCfg.imei);
// 	if(dataLen)
// 		len += sprintf((char *)&buf[len],"%04X,",dataLen*2 + len + 17/*total byte*/);
// 	else
// 		len += sprintf((char *)&buf[len],"%04X,",dataLen + len + 16/*total byte*/);
// 	len += sprintf((char *)&buf[len],"%04X,",cfgSize/*total byte*/);
// 	len += sprintf((char *)&buf[len],"%04X",cfgOffset/*offset byte*/);
// 	if(dataLen)
// 		len += sprintf((char *)&buf[len],",");
// 	for(i = 0;i < dataLen;i++) //data
// 		len += sprintf((char *)&buf[len],"%02X",data[i]);
// 	len += sprintf((char *)&buf[len],"\r\n");
// 	return len;
// }

uint16_t DCFG_Add(uint8_t session, uint8_t *buf, uint8_t *cfgData, uint16_t dataLen, uint16_t cfgSize, uint16_t cfgOffset)
{
	uint32_t len = 0;
	uint16_t i;
	
	len = sprintf((char*)buf, "%s,", "@DCFG");
	len += sprintf((char*)&buf[len], "%s,", sysCfg.imei);
	len += sprintf((char*)&buf[len], "%03X,", session);
	len += sprintf((char*)&buf[len], "%04X,", dataLen * 2 + len + 17			/*total byte*/);
	len += sprintf((char*)&buf[len], "%04X,", cfgSize											/*total byte*/);
	len += sprintf((char*)&buf[len], "%04X,", cfgOffset										/*offset byte*/);
	
	for (i = 0; i < dataLen; i++) //data
	{
		len += sprintf((char*)&buf[len], "%02X", cfgData[i]);
	}
	len += sprintf((char *)&buf[len], "\r\n");
	
	return len;
}

uint16_t SCFG_Add(uint8_t session, uint8_t *buf, uint16_t cfgSize, uint16_t cfgOffset)
{
	uint32_t len = 0;
	
	len = sprintf((char*)buf, "%s,", "@SCFG");
	len += sprintf((char*)&buf[len], "%s,", sysCfg.imei);
	len += sprintf((char*)&buf[len], "%03X,", session);
	len += sprintf((char*)&buf[len], "%04X,", len + 17);									/*total byte*/
	len += sprintf((char*)&buf[len], "%04X,", cfgSize);										/*total byte*/
	len += sprintf((char*)&buf[len], "%04X,", cfgOffset);									/*offset byte*/
	len += sprintf((char *)&buf[len], "\r\n");
	
	return len;
}

uint8_t DCFG_Parser(uint8_t *buf, uint8_t *session, uint16_t *cfgSize, uint16_t *cfgOffset)
{
	uint32_t A, B, C, D;
	uint8_t imei[16];
	
	if ((buf[0] == '@') &&
			(buf[1] == 'D') &&
			(buf[2] == 'C') &&
			(buf[3] == 'F') &&
			(buf[4] == 'G') &&
			(buf[5] == ',') &&
			(buf[40] == '\r'))
	{
		buf[40] = 0;
		sscanf((char *)buf,"@DCFG,%[^,],%03X,%04X,%04X,%04X", (char *)imei, &A, &B, &C, &D);
		if (B == 42)
		{
			*session = A;
			*cfgSize = C;
			*cfgOffset = D;
			imei[15] = 0;
			if (strcmp((char*)&sysCfg.imei, (char*)imei) == 0)
			{
				return 1;
			}
		}
	}
	
	return 0;
}

// uint8_t SCFG_Parser(uint8_t *buf, uint8_t *data, uint8_t *session, uint16_t *dataLen, uint16_t *cfgSize, uint16_t *cfgOffset, uint32_t receivedLen)
// {
// 	uint32_t A, B, C, D;
// 	uint8_t imei[16];
// 	uint16_t i;
// 	uint8_t tempBuf[3];
// 	uint32_t tempData;
// 	uint16_t len;
// 	
// 	if ((buf[0] == '@') &&
// 			(buf[1] == 'S') &&
// 			(buf[2] == 'C') &&
// 			(buf[3] == 'F') &&
// 			(buf[4] == 'G') &&
// 			(buf[5] == ','))
// 	{
// 		buf[40] = 0;
// 		sscanf((char *)buf,"@SCFG,%[^,],%03X,%04X,%04X,%04X", (char *)imei, &A, &B, &C, &D);
// 		if (B == receivedLen)
// 		{
// 			*session = A;
// 			*cfgSize = C;
// 			*cfgOffset = D;
// 			imei[15] = 0;
// 			if (strcmp((char*)&sysCfg.imei, (char*)imei) == 0)
// 			{
// 				if (B > 43)
// 				{
// 					*dataLen = B - 2;
// 					len = 0;
// 					tempBuf[2] = 0;
// 					for (i = 41; i < *dataLen; i += 2)
// 					{
// 						tempBuf[0] = buf[i];
// 						tempBuf[1] = buf[i + 1];
// 						if ((((tempBuf[0] >= '0') && (tempBuf[0] <= '9')) || ((tempBuf[0] >= 'A') && (tempBuf[0] <= 'F'))) &&
// 								(((tempBuf[1] >= '0') && (tempBuf[1] <= '9')) || ((tempBuf[1] >= 'A') && (tempBuf[1] <= 'F'))))
// 						{
// 							sscanf((char*)tempBuf, "%02X", &tempData);
// 							data[len] = tempData;
// 						}
// 						else
// 						{
// 							data[len] = 0;
// 						}
// 						len++;
// 					}
// 					*dataLen = len;
// 				}
// 				else
// 				{
// 					data[0] = 0;
// 					*dataLen = 0;
// 				}
// 				
// 				return 1;
// 			}
// 		}
// 	}
// 	
// 	return 0;
// }

uint8_t IsGprsDataPacket(uint8_t *buf, char *header, uint8_t headerLen)
{
	uint8_t i;
	uint8_t result = 1;
	
	for (i = 0; i < headerLen; i++)
	{
		if (buf[i] != header[i])
		{
			result = 0;
			break;
		}
	}
	
	return result;
}

// uint16_t CFG_Parser(uint8_t *header,uint8_t *buf,uint8_t *data,uint16_t *dataLen,uint16_t *cfgSize,uint16_t *cfgOffset)
// {
// 	uint32_t len = 0;
// 	uint16_t i;
// 	uint32_t A,B,C,X;
// 	uint8_t bufTemp[3],u8t;
// 	uint8_t imei[16];
// 	
// 	if(buf[0] == '@' 
// 		&& (buf[36] == ',' || buf[36] == '\r')
// 		&& buf[2] == 'C'
// 		&& buf[3] == 'F'
// 		&& buf[4] == 'G'
// 		&& buf[5] == ','
// 	)
// 	{
// 		*header = buf[1];
// 		buf[36] = 0;
// 		sscanf((char *)&buf[2],"CFG,%[^,],%04X,%04X,%04X", (char *)imei, &A, &B, &C);
// 		*cfgSize = B;
// 		*cfgOffset = C;
// 		*dataLen = 0;
// 		imei[15] = 0;
// 		if (strcmp((char*)&sysCfg.imei, (char*)imei) == 0)
// 		{
// 			if(A > 39)
// 			{
// 				bufTemp[2] = '\0';
// 				for(i = 37;i < A - 2;)
// 				{
// 					memcpy(bufTemp,&buf[i],2);
// 					if(
// 						(
// 								(bufTemp[0] >= '0' && bufTemp[0] <= '9')
// 						|| 	(bufTemp[0] >= 'A' && bufTemp[0] <= 'F')
// 						)
// 						&&
// 						(
// 								(bufTemp[1] >= '0' && bufTemp[1] <= '9')
// 						|| 	(bufTemp[1] >= 'A' && bufTemp[1] <= 'F')
// 						)
// 					)
// 					{
// 						sscanf((char *)bufTemp,"%02X",&X);
// 						u8t = (uint8_t)X;
// 						data[len] = u8t;
// 						len++;
// 						i += 2;
// 					}
// 					else
// 						return 0;
// 				}
// 				*dataLen = len;
// 				return 1;
// 			}
// 			else if(A == 38)
// 			{
// 				return 1;
// 			}
// 		}
// 	}
// 	return 0;
// }

uint16_t DPWF_Add(char *data,uint32_t len)
{
	uint32_t u32Temp0;
	char tmpBuf[32],buff[128];
	buff[0] = 0;
	data[0] = 0;
	sprintf(tmpBuf,"%s",sysCfg.imei);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%s",FIRMWARE_VERSION);
	strcat(buff, tmpBuf);
	strcat(data, "@DPWF");
	strcat(data, "004");
	u32Temp0 = strlen(tmpBuf) + strlen(data) + 3;
	sprintf(tmpBuf,"%03d",u32Temp0);
	strcat(data, tmpBuf);
	strcat(data, buff);
	return strlen(data);
}

uint16_t DPWO_Add(char *data,uint32_t len)
{
	uint32_t u32Temp0;
	char tmpBuf[32],buff[128];
	buff[0] = 0;
	data[0] = 0;
	sprintf(tmpBuf,"%s",sysCfg.imei);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%s",FIRMWARE_VERSION);
	strcat(buff, tmpBuf);
	strcat(data, "@DPWO");
	strcat(data, "008");
	u32Temp0 = strlen(tmpBuf) + strlen(data) + 3;
	sprintf(tmpBuf,"%03d",u32Temp0);
	strcat(data, tmpBuf);
	strcat(data, buff);
	return strlen(data);
}


uint16_t DCLI_Add(MSG_STATUS_RECORD *logRecordSend,char *data,uint32_t *len)
{
	uint32_t u32Temp0,u32Temp1;
	char tmpBuf[32],buff[128];
	float fTemp;
	double lfTemp;
	buff[0] = 0;
	data[0] = 0;
	sprintf(tmpBuf,"%s",sysCfg.imei);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%02d%02d%02d",logRecordSend->currentTime.mday,logRecordSend->currentTime.month,logRecordSend->currentTime.year % 100);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%02d%02d%02d",logRecordSend->currentTime.hour,logRecordSend->currentTime.min,logRecordSend->currentTime.sec);
	strcat(buff, tmpBuf);
	// Add lat and long
	lfTemp = formatLatLng(logRecordSend->CELL_lat);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);
	if (logRecordSend->CELL_ns != 0)
	{
		sprintf(tmpBuf,"%c",logRecordSend->CELL_ns);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_CELL_NS);
	}
	strcat(buff,tmpBuf);
	
	lfTemp = formatLatLng(logRecordSend->CELL_lon);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);
	if (logRecordSend->CELL_ew != 0)
	{
		sprintf(tmpBuf,"%c",logRecordSend->CELL_ew);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_CELL_EW);
	}
	strcat(buff,tmpBuf);
	//add speed
	fTemp = logRecordSend->speed;
	u32Temp0 = (uint32_t)fTemp;
	u32Temp1 = (uint32_t)((fTemp - (float)u32Temp0)*10);
	sprintf(tmpBuf,"%03d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%01d",u32Temp1);
	strcat(buff, tmpBuf);
	//add direction
	sprintf(tmpBuf,"%03d",(uint32_t)logRecordSend->dir);
	strcat(buff, tmpBuf);
	strcat(buff, "00000");
	strcat(buff, "A");
	//15 bytes reserve
	//1
	//sprintf(tmpBuf,"%01d",(logRecordSend->SOS_action % 5));
	sprintf(tmpBuf,"%01d", 0);
	strcat(buff, tmpBuf);
	//2
	sprintf(tmpBuf,"%01d",(logRecordSend->geoFenceCheck % 3));
	strcat(buff, tmpBuf);
	//3/4
	sprintf(tmpBuf,"%02d",(logRecordSend->GSM_csq % 100));
	strcat(buff, tmpBuf);
	//5/6/7
	sprintf(tmpBuf,"%03d",(uint32_t)((((float)logRecordSend->batteryPercent)/100 * 4.15) * 100));
	strcat(buff, tmpBuf);
	//8
	if(logRecordSend->status & 1)
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//9
	if(logRecordSend->batteryPercent < 5)
		strcat(buff, "2");
	else if(logRecordSend->batteryPercent < 25)
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//10
	if (logRecordSend->status & (1 << 3))
	{
		strcat(buff, "1");
	}
	else
	{
		strcat(buff, "0");
	}
	//11 Man Down Check
	if(logRecordSend->status & (1 << 4))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//12 Lone worker Check
	if(logRecordSend->status & (1 << 5))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//13 No Movement Check
	if(logRecordSend->status & (1 << 6))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//14/15
		strcat(buff, "00");
		
	strcat(data, "@DCLI");
	strcat(data, "008");
	u32Temp0 = strlen(tmpBuf) + strlen(data) + 3;
	sprintf(tmpBuf,"%03d",u32Temp0);
	strcat(data, tmpBuf);
	strcat(data, buff);
	*len = strlen(data);
	return 0;
}

uint16_t DGPS_Add(MSG_STATUS_RECORD *logRecordSend,char *data,uint32_t *len)
{
	uint32_t u32Temp0,u32Temp1;
	char tmpBuf[32],buff[128];
	float fTemp;
	double lfTemp;
	buff[0] = 0;
	data[0] = 0;
	sprintf(tmpBuf,"%s",sysCfg.imei);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%02d%02d%02d",logRecordSend->currentTime.mday,logRecordSend->currentTime.month,logRecordSend->currentTime.year % 100);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%02d%02d%02d",logRecordSend->currentTime.hour,logRecordSend->currentTime.min,logRecordSend->currentTime.sec);
	strcat(buff, tmpBuf);
	// Add lat and long
	lfTemp = formatLatLng(logRecordSend->GPS_lat);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);
	if (logRecordSend->GPS_ns != 0)
	{
		sprintf(tmpBuf,"%c",logRecordSend->GPS_ns);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_GPS_NS);
	}
	strcat(buff,tmpBuf);
	
	lfTemp = formatLatLng(logRecordSend->GPS_lon);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);
	if (logRecordSend->GPS_ew != 0)
	{
		sprintf(tmpBuf,"%c",logRecordSend->GPS_ew);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_GPS_EW);
	}
	strcat(buff,tmpBuf);
	//add speed
	fTemp = logRecordSend->speed;
	u32Temp0 = (uint32_t)fTemp;
	u32Temp1 = (uint32_t)((fTemp - (float)u32Temp0)*10);
	sprintf(tmpBuf,"%03d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%01d",u32Temp1);
	strcat(buff, tmpBuf);
	//add direction
	sprintf(tmpBuf,"%03d",(uint32_t)logRecordSend->dir);
	strcat(buff, tmpBuf);
	strcat(buff, "00000");
	strcat(buff, "V");
	//15 bytes reserve
	//1
	//sprintf(tmpBuf,"%01d",(logRecordSend->SOS_action % 5));
	sprintf(tmpBuf,"%01d", 0);
	strcat(buff, tmpBuf);
	//2
	sprintf(tmpBuf,"%01d",(logRecordSend->geoFenceCheck % 3));
	strcat(buff, tmpBuf);
	//3/4
	sprintf(tmpBuf,"%02d",(logRecordSend->GSM_csq % 100));
	strcat(buff, tmpBuf);
	//5/6/7
	sprintf(tmpBuf,"%03d",(uint32_t)((((float)logRecordSend->batteryPercent)/100 * 4.15) * 100));
	strcat(buff, tmpBuf);
	//8
	if(logRecordSend->status & 1)
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//9
	if(logRecordSend->batteryPercent < 5)
		strcat(buff, "2");
	else if(logRecordSend->batteryPercent < 25)
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//10
		strcat(buff, "0");
	//11 Man Down Check
	if(logRecordSend->status & (1 << 4))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//12 Lone worker Check
	if(logRecordSend->status & (1 << 5))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//13 No Movement Check
	if(logRecordSend->status & (1 << 6))
		strcat(buff, "1");
	else
		strcat(buff, "0");
	//14/15
		strcat(buff, "00");
		
	strcat(data, "@DGPS");
	strcat(data, "008");
	u32Temp0 = strlen(tmpBuf) + strlen(data) + 3;
	sprintf(tmpBuf,"%03d",u32Temp0);
	strcat(data, tmpBuf);
	strcat(data, buff);
	*len = strlen(data);
	return 0;
}

uint32_t DSOS_Add(char *data, uint8_t sosAction, uint8_t sosSession)
{
	uint32_t u32Temp0,u32Temp1;
	char tmpBuf[32],buff[128];
	double lat, lon, ns, ew;
	uint8_t gpsFixed;
	double lfTemp;
	DATE_TIME tempSysTime;
	buff[0] = 0;
	data[0] = 0;
	sprintf(tmpBuf,"%s", sysCfg.imei);
	strcat(buff, tmpBuf);
	u32Temp0 = TIME_GetSec(&sysTime) + timeZone;
	TIME_FromSec(&tempSysTime,u32Temp0);
	// add time
	sprintf(tmpBuf, "%02d%02d%02d", sysTime.mday, sysTime.month, sysTime.year % 100);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf, "%02d%02d%02d", sysTime.hour, sysTime.min, sysTime.sec);
	strcat(buff, tmpBuf);
	
	if ((sosAction != SOS_OFF) && (sosAction != SOS_CANCELLED))
	{
		sprintf((char*)sosStartTime, "%02d:%02d:%02d", tempSysTime.hour, tempSysTime.min, tempSysTime.sec);
		sosStartTime[8] = 0;
		sosActionSms = sosAction;
	}
	sprintf((char*)sosEndTime, "%02d:%02d:%02d", tempSysTime.hour, tempSysTime.min, tempSysTime.sec);
	sosEndTime[8] = 0;
	
	// Add lat and long	
	if((lastGpsInfo.lon != 0) && (lastGpsInfo.lat != 0) && (lastGpsInfo.sig >= 1) && (lastGpsFixed))
	{
		lat = lastGpsInfo.lat;
		lon = lastGpsInfo.lon;
		ew = lastGpsInfo.ew;
		ns = lastGpsInfo.ns;
		gpsFixed = 1;
	}
	else if ((cellLocate.lat >= -90) && (cellLocate.lat <= 90) &&
		(cellLocate.lon >= -180) && (cellLocate.lon <= 180) &&
		(cellLocate.uncertainty <= 3000))
	{
		lat = neamFormatLatLng(cellLocate.lat);
		lon = neamFormatLatLng(cellLocate.lon);
		ew = cellLocate.ew;
		ns = cellLocate.ns;
		gpsFixed = 0;
	}
	else if((lastGpsInfo.lon != 0) && (lastGpsInfo.lat != 0) && (lastGpsInfo.sig >= 1))
	{
		lat = lastGpsInfo.lat;
		lon = lastGpsInfo.lon;
		ew = lastGpsInfo.ew;
		ns = lastGpsInfo.ns;
		gpsFixed = 1;
	}
	else
	{
		lat = 0;
		lon = 0;
		ew = 'E';
		ns = 'N';
		gpsFixed = 0;
	}
	
	lfTemp = formatLatLng(lat);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);	
	if (ns != 0)
	{
		sprintf(tmpBuf, "%c", (char)ns);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_CELL_NS);
	}
	strcat(buff, tmpBuf);
	
	if ((sosAction != SOS_OFF) && (sosAction != SOS_CANCELLED))
	{
		sprintf((char*)sosStartLocation, "%02d.%07d", u32Temp0, u32Temp1);
		sosStartLocation[10] = 0;
	}
	sprintf((char*)sosEndLocation, "%02d.%07d", u32Temp0, u32Temp1);
	sosEndLocation[10] = 0;
	
	lfTemp = formatLatLng(lon);
	u32Temp0 = (uint32_t)lfTemp;
	u32Temp1 = (uint32_t)((lfTemp - (double)u32Temp0)*10000000);
	sprintf(tmpBuf,"%02d.",u32Temp0);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf,"%07d",u32Temp1);
	strcat(buff, tmpBuf);
	if (ew != 0)
	{
		sprintf(tmpBuf, "%c", (char)ew);
	}
	else
	{
		sprintf(tmpBuf, "%c", DEFAULT_CELL_EW);
	}
	strcat(buff, tmpBuf);
	
	if ((sosAction != SOS_OFF) && (sosAction != SOS_CANCELLED))
	{
		sprintf((char*)sosStartLocation, "%s,%02d.%07d", (char*)sosStartLocation, u32Temp0, u32Temp1);
		sosStartLocation[21] = 0;
	}
	sprintf((char*)sosEndLocation, "%s,%02d.%07d", (char*)sosStartLocation, u32Temp0, u32Temp1);
	sosEndLocation[21] = 0;
	
	// add gps status
	if (gpsFixed)
	{
		sprintf(tmpBuf, "%c", 'G');
	}
	else
	{
		sprintf(tmpBuf, "%c", 'C');
	}
	strcat(buff, tmpBuf);
	
	sprintf(tmpBuf, "%01d", sosAction);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf, "%01d", sysCfg.familyPalMode);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf, "%03d", sosSession);
	strcat(buff, tmpBuf);
	sprintf(tmpBuf, "%02d", batteryPercent);
	strcat(buff, tmpBuf);
		
	data[0] = 0;
	strcat(data, "@DSOS");
	strcat(data, "008");
	u32Temp0 = strlen(tmpBuf) + strlen(data) + 3;
	sprintf(tmpBuf, "%03d", u32Temp0);
	strcat(data, tmpBuf);
	strcat(data, buff);
	
	return strlen(data);
}
