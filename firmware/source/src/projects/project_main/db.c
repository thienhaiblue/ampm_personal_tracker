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
#include "db.h"
#include "sst25.h"
#include "modem.h"
#define DATABASE_DBG(...)		//DbgCfgPrintf(__VA_ARGS__)

LOG_TYPE trackerLog;

uint32_t logPositionCnt = 0;
uint32_t flagNewLog = 1;

float mileage;
uint32_t mileageCnt = 0;

uint32_t totalPulse = 0;
uint32_t totalPulseCnt = 0;

uint32_t resetNum;
uint32_t resetCnt = 0;

DB_U32 dbTimeZone;
DB_U32 pwrStatus;
DB_U32 gpsLat;
DB_U32 gpsLon;
DB_U32 gpsHDOP;
DB_U32 gpsDir;
DB_U32 cellLat;
DB_U32 cellLon;
DB_U32 cellHDOP;
DB_U32 cellDir;
DB_U32 timeSave;

uint8_t DbCalcCheckSum(uint8_t *buff, uint32_t length)
{
	uint32_t i;
	uint8_t crc = 0;
	for(i = 0;i < length; i++)
	{
		crc += buff[i];
	}
	return crc;
}


uint32_t DB_FlashMemInit(void)
{
	float lat,lon;
	DB_InitLog(&trackerLog,TRACKER_RING_ADDR,1,TRACKER_DATA_ADDR,sizeof(MSG_STATUS_RECORD),TRACKER_DATA_SIZE_MAX);
	DB_MileageLoad();
	DB_ResetCntLoad();
	DB_ResetCntSave();
	DB_U32Load(&pwrStatus,POWER_STATUS_ADDR);
	DB_U32Load(&gpsLat,GPS_LAT_ADDR);
	DB_U32Load(&gpsLon,GPS_LON_ADDR);
	DB_U32Load(&gpsHDOP,GPS_HDOP_ADDR);
	DB_U32Load(&gpsDir,GPS_DIR_ADDR);
	DB_U32Load(&cellLat,CELL_LAT_ADDR);
	DB_U32Load(&cellLon,CELL_LON_ADDR);
	DB_U32Load(&cellHDOP,CELL_HDOP_ADDR);
	DB_U32Load(&cellDir,CELL_DIR_ADDR);
	DB_U32Load(&dbTimeZone,TIME_ZONE_ADDR);
	timeZone = dbTimeZone.value;
	
	lat = *((float *)(&gpsLat.value));
	lon = *((float *)(&gpsLon.value));
	lastNmeaInfo.lat = neamFormatLatLng(lat);
	lastNmeaInfo.lon = neamFormatLatLng(lon);
	if(lat < 0)
		lastNmeaInfo.ns = 'S';
	else 
		lastNmeaInfo.ns = 'N';
	if(lon < 0)
		lastNmeaInfo.ew = 'W';
	else 
		lastNmeaInfo.ew = 'E';
	lastNmeaInfo.HDOP = *((float *)(&gpsHDOP.value));
	lastNmeaInfo.direction = *((float *)(&gpsDir.value));
	if((lastNmeaInfo.lat <= 90 || lastNmeaInfo.lat >= -90) 
		&& (lastNmeaInfo.lon <= 180  || lastNmeaInfo.lon >= -180)
		)
		{
			lastNmeaInfo.fix = 3;
			lastNmeaInfo.sig = 1;
		}
		else
		{
			lastNmeaInfo.lat = 0;
			lastNmeaInfo.lon = 0;
		}
	if((cellLocate.lat <= 90 || cellLocate.lat >= -90) 
		&& (cellLocate.lon <= 180  || cellLocate.lon >= -180)
		&& cellLocate.uncertainty <= 3000
		)
	{
		cellLocate.lat = *((float *)(&cellLat.value));
		cellLocate.lon = *((float *)(&cellLon.value));
		cellLocate.uncertainty = *((float *)(&cellHDOP.value));
		cellLocate.dir = *((float *)(&cellDir.value));
		
		if(cellLocate.lat < 0)
			cellLocate.ns = 'S';
		else 
			cellLocate.ns = 'N';
		if(cellLocate.lon < 0)
			cellLocate.ew = 'W';
		else 
			cellLocate.ew = 'E';
		
	}
	else
	{
		cellLocate.lat = 0;
		cellLocate.lon = 0;
	}
	DB_U32Load(&timeSave,TIME_SAVE_ADDR);
	//UpdateRtcTime(timeSave.value);
//	TIME_FromSec(&sysTime,rtcTimeSec);
	
	return 0;
}


void DB_ResetCntLoad(void)
{
	uint8_t u8temp,buf[5];
	int32_t i;
	uint32_t *u32pt;
	resetNum = 0;
	resetCnt = 0;
	u32pt = (uint32_t *)buf;
	for(i = 4090;i >= 0;i -= 5)
	{
		SST25_Read(RESET_CNT_ADDR + i,buf,5);
		if(*u32pt != 0xffffffff)
		{
			u8temp = DbCalcCheckSum(buf,4);
			if(resetCnt == 0)
				resetCnt = i;
			if(buf[4] == u8temp)
			{
				memcpy((uint8_t *)&resetNum,buf,4);
				break;
			}
		}
	}
}

void DB_ResetCntSave(void)
{
	uint8_t buff[5];
	resetNum++;
	memcpy(buff,(uint8_t *)&resetNum,4);
	buff[4] = DbCalcCheckSum((uint8_t *)buff,4);
	SST25_Write(RESET_CNT_ADDR + resetCnt,buff,4);
	SST25_Write(RESET_CNT_ADDR + resetCnt + 4,&buff[4],1);
	resetCnt += 5;
	if(resetCnt >= 4095)
	{
		SST25_Erase(RESET_CNT_ADDR,block4k);
		resetCnt = 0;
	}
}


void DB_MileageLoad(void)
{
	uint8_t u8temp,buf[5];
	int32_t i;
	uint32_t *u32pt;
	mileage = 0;
	mileageCnt = 0;
	u32pt = (uint32_t *)buf;
	for(i = 4090;i >= 0;i -= 5)
	{
		SST25_Read(MILEAGE_ADDR + i,buf,5);
		if(*u32pt != 0xffffffff)
		{
			u8temp = DbCalcCheckSum(buf,4);
			if(mileageCnt == 0)
				mileageCnt = i;
			if(buf[4] == u8temp)
			{
				memcpy((uint8_t *)&mileage,buf,4);
				break;
			}
		}
	}
}

void DB_MileageSave(void)
{
	uint8_t buff[5];
	memcpy(buff,(uint8_t *)&mileage,4);
	buff[4] = DbCalcCheckSum((uint8_t *)buff,4);
	SST25_Write(MILEAGE_ADDR + mileageCnt,buff,4);
	SST25_Write(MILEAGE_ADDR + mileageCnt + 4,&buff[4],1);
	mileageCnt += 5;
	if(mileageCnt >= 4095)
	{
		SST25_Erase(MILEAGE_ADDR,block4k);
		mileageCnt = 0;
	}
}

uint32_t DB_FloatToU32(double lf)
{
	uint32_t *u32pt;
	float f = lf,*tPt;
	tPt = &f;
	u32pt = (uint32_t *)tPt;
	return *u32pt;
}

float DB_U32ToFloat(uint32_t *u32)
{
	float *fpt;
	fpt = (float *)u32;
	return *fpt;
}

void DB_U32Load(DB_U32 *dbu32,uint32_t addr)
{
	uint8_t u8temp,buf[5];
	int32_t i;
	uint32_t *u32pt;
	dbu32->oldValue = 1;
	dbu32->value = 0;
	dbu32->cnt = 0;
	u32pt = (uint32_t *)buf;
	for(i = 4090;i >= 0;i -= 5)
	{
		SST25_Read(addr + i,buf,5);
		if(*u32pt != 0xffffffff)
		{
			u8temp = DbCalcCheckSum(buf,4);
			if(buf[4] == u8temp)
			{
				dbu32->cnt = i + 5;
				memcpy((uint8_t *)&dbu32->value,buf,4);
				dbu32->oldValue = dbu32->value;
				break;
			}
		}
	}
}

void DB_U32Save(DB_U32 *dbu32,uint32_t addr)
{
	uint8_t buff[5],bufR[5];
	if(dbu32->oldValue != dbu32->value)
	{
		dbu32->oldValue = dbu32->value;
		memcpy(buff,(uint8_t *)&dbu32->value,4);
		if(dbu32->cnt >= 4095)
		{
			dbu32->cnt = 0;
		}
		for(;dbu32->cnt <= 4095; dbu32->cnt += 5)
		{
			buff[4] = DbCalcCheckSum((uint8_t *)buff,4);
			SST25_Write(addr + dbu32->cnt,buff,4);
			SST25_Write(addr + dbu32->cnt + 4,&buff[4],1);
			SST25_Read(addr + dbu32->cnt,bufR,5);
			if(memcmp(buff,bufR,5) != NULL)
			{
				if(dbu32->cnt >= 4095)
				{
					dbu32->cnt = 0;
					break;
				}
			}
			else
			{
				dbu32->cnt += 5;
				break;
			}
		}
	}
}


void DB_InitLog(LOG_TYPE *log,uint32_t logBeginAddr,uint32_t logSectorCnt,uint32_t dataBeginAddr,uint32_t dataLogSize,uint32_t dataLogAreaSize)
{
	uint8_t u8temp;
	int32_t i;
	uint32_t u32Temp;
	LOG_RING_TYPE logTemp;
	log->ring.head = 0;
	log->ring.tail = 0;
	log->ring.crc = 0;
	log->logBeginAddr = logBeginAddr;
	log->logSectorCnt = logSectorCnt;
	log->dataBeginAddr = dataBeginAddr;
	log->dataLogSize = dataLogSize;
	log->dataLogAreaSize = dataLogAreaSize;
	log->cnt = 0;
	if((log->dataLogSize % LOG_WRITE_BYTES) != 0)
		while(1); //khai bao lai kieu bien
	
	u32Temp = log->dataLogAreaSize / log->dataLogSize;
	for(i = ((uint32_t)((log->logSectorCnt * SECTOR_SIZE) / sizeof(LOG_RING_TYPE))) * sizeof(LOG_RING_TYPE) - sizeof(LOG_RING_TYPE) ; i >= 0; i -= sizeof(LOG_RING_TYPE))
	{
		SST25_Read(log->logBeginAddr + i,(uint8_t *)&logTemp, sizeof(LOG_RING_TYPE));
		if(logTemp.head != 0xffffffff && 
			logTemp.tail != 0xffffffff &&
			logTemp.head <= u32Temp &&
			logTemp.tail <= u32Temp
		)
		{
			u8temp = DbCalcCheckSum((uint8_t *)&logTemp,sizeof(LOG_RING_TYPE) - 1);
			if(log->cnt == 0)
				log->cnt = i;
			if(logTemp.crc == u8temp)
			{
				log->ring = logTemp;
				break;
			}
		}
	}
	i = ((uint32_t)((log->logSectorCnt * SECTOR_SIZE) / sizeof(LOG_RING_TYPE)));
	if(log->ring.head > i ||
		log->ring.tail > i
		)
		{
			DB_LogRingReset(log);
		}
	else
		log->crc = DbCalcCheckSum((uint8_t *)log,sizeof(LOG_TYPE) - 1);
}


void DB_LogRingReset(LOG_TYPE *log)
{
	log->ring.head = 0;
	log->ring.tail = 0;
	DB_LogRingSave(log);
}

void DB_LogRingSave(LOG_TYPE *log)
{
	uint32_t i;
	if(log->cnt >= ((uint32_t)((log->logSectorCnt * SECTOR_SIZE) / sizeof(LOG_RING_TYPE))) * sizeof(LOG_RING_TYPE))
	{
		for(i = log->logBeginAddr;i < (log->logBeginAddr + log->logSectorCnt);i += log->logSectorCnt)
			SST25_Erase(i,block4k);
		log->cnt = 0;
	}
	log->ring.crc = DbCalcCheckSum((uint8_t *)&log->ring,sizeof(LOG_RING_TYPE) - 1);
	SST25_Write(log->logBeginAddr + log->cnt,(uint8_t *)&log->ring.head,sizeof(log->ring.head));
	log->cnt += sizeof(log->ring.head);
	SST25_Write(log->logBeginAddr + log->cnt,(uint8_t *)&log->ring.tail,sizeof(log->ring.tail));
	log->cnt += sizeof(log->ring.tail);
	SST25_Write(log->logBeginAddr + log->cnt,(uint8_t *)&log->ring.crc,sizeof(log->ring.crc));
	log->cnt += sizeof(log->ring.crc);
	log->crc = DbCalcCheckSum((uint8_t *)log,sizeof(LOG_TYPE) - 1);
}

uint32_t DB_DataLogFill(LOG_TYPE *log)
{
	if(log->ring.head >= log->ring.tail)
	{
		return (log->ring.head - log->ring.tail);
	}
	else
	{
		return (log->dataLogAreaSize / log->dataLogSize - log->ring.tail + log->ring.head);
	}
}

char DB_LoadDataInfo(uint8_t *data,LOG_TYPE *log)
{
	uint8_t u8temp;
	uint32_t u32Temp0,i;
	u32Temp0 = log->ring.head;
	for(i = 0;i < log->dataLogAreaSize / log->dataLogSize;i++)
	{
		if(log->ring.head == 0)
			u32Temp0 = log->dataLogAreaSize / log->dataLogSize - 1;
		else
			u32Temp0--;
		SST25_Read(log->dataBeginAddr + u32Temp0*log->dataLogSize,data,log->dataLogSize);
		u8temp = DbCalcCheckSum((uint8_t *)data,log->dataLogSize - 1);
		if(data[log->dataLogSize - 1] == u8temp)
		{
			return 0;
		}
	}
	return 0xff;
}

char DB_LoadDataLogTail(uint8_t *data,LOG_TYPE *log)
{
	uint8_t u8temp;
	if(log->ring.tail == log->ring.head) return 0xff;
	
	SST25_Read(log->dataBeginAddr + log->ring.tail*log->dataLogSize,data,log->dataLogSize);
	u8temp = DbCalcCheckSum((uint8_t *)data,log->dataLogSize - 1);
	log->ring.tail++;
	if(log->ring.tail >= log->dataLogAreaSize / log->dataLogSize)
		log->ring.tail = 0;
	DB_LogRingSave(log);
	if(data[log->dataLogSize - 1] == u8temp)
	{
		return 0;
	}
	else 
	{
		return 0xff;
	}
}

char DB_SaveDataLog(uint8_t *data,LOG_TYPE *log)
{
	uint32_t u32temp,tailSector = 0,headSector = 0,headSectorOld = 0,i;
	tailSector = log->ring.tail * log->dataLogSize / SECTOR_SIZE;
	headSectorOld = log->ring.head * log->dataLogSize / SECTOR_SIZE;
	u32temp = log->ring.head;
	u32temp++;
	if(u32temp >= log->dataLogAreaSize / log->dataLogSize)	
	{
		u32temp = 0;
	}
	headSector = u32temp * log->dataLogSize / SECTOR_SIZE;
	if(headSector != headSectorOld)
	{
		SST25_Erase(log->dataBeginAddr + headSector * SECTOR_SIZE,block4k);
	}
	if((headSector == tailSector) && (u32temp <= log->ring.tail))
	{
		tailSector++;
		log->ring.tail = tailSector * SECTOR_SIZE / log->dataLogSize;
		if((tailSector * SECTOR_SIZE) % log->dataLogSize)
		{
			log->ring.tail++;
		}
		if(log->ring.tail >= log->dataLogAreaSize / log->dataLogSize)
			log->ring.tail = 0;
	}

	data[log->dataLogSize - 1] = DbCalcCheckSum((uint8_t *)data,log->dataLogSize - 1);
	
	for(i = 0;i < log->dataLogSize;i += LOG_WRITE_BYTES)
	{
		SST25_Write(log->dataBeginAddr + log->ring.head * log->dataLogSize + i,data + i,LOG_WRITE_BYTES);
	}
	log->ring.head = u32temp;
	DB_LogRingSave(log);
	
	return 0;
}
