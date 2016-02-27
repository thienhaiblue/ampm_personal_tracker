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
#include "ringbuf.h"
#include "system_config.h"
#include "gps.h"
#include "db.h"
#include "at_command_parser.h"
#include "modem.h"
#include "hw_config.h"
#include "gprs_cmd_ctrl.h"
#include "project_common.h"
#include "sst25.h"
#include "gsm_gprs_tasks.h"
#include "pedometer_task.h"
#include "audio.h"
#include "gps_task.h"

#define TRACKER(...)	//DbgCfgPrintf(__VA_ARGS__)


extern uint8_t sosAction;
uint32_t timer0Cnt = 0;
uint32_t gsmTimeSec = 0;
uint8_t hasMoved = 0;



MSG_STATUS_RECORD	logRecord,logRecordTemp;
uint8_t trackerEnable = 0;
TRACKER_MSG_TYPE trackerMsg;

void TrackerInit(void)
{
	trackerMsg.oldMsgIsEmpty = 1;
	trackerMsg.newMsgIsEmpty = 1;
	trackerMsg.lastMsgIsEmpty = 1;
}


uint8_t sendCellFlag = 0;

extern uint8_t pedometerEnable;

void TrackerTask(void) // Timer0 interrupt subroutine 
{
	
	DATE_TIME timeTemp;
	uint32_t u32TimeTemp;
	uint8_t  flagCellLocationUpdated;
	
	
	timer0Cnt++;
	if(AudioStopCheck())
		if(pedometerEnable)
			PedometerTask();
	if(Pedometer.updateReadyFlag)
	{
		gpsPedometerFlag = 1;
		intervalPedometerFlag = 1;
	}
	
	GpsTask_Control();
//	DB_U32Save(&dbTimeZone,TIME_ZONE_ADDR);
	//Save old power status to db
	DB_U32Save(&pwrStatus,POWER_STATUS_ADDR);
	if(timer0Cnt % 60 == 0)
	{
		timeSave.value = rtcTimeSec;
		DB_U32Save(&timeSave,TIME_SAVE_ADDR);
	}
	//enable GPS before 1min
	if(sysCfg.reportInterval > 2)
	{
		if((timer0Cnt % ((sysCfg.reportInterval - 1)*60) == 0) && intervalPedometerFlag)
		{
			intervalPedometerFlag = 0;
			GpsTask_EnableGps();
			serverWaitGpsFlag = 1;
		}
	}
	//Check time interval
	if((timer0Cnt % (sysCfg.reportInterval*60)) == 0)
	{
		GprsTask_StartTask(SEND_GPS_DATA_TASK);
	}
	
	//Get cellocation
	flagCellLocationUpdated = ParserCellLocate();
	TIME_FromGsm(&timeTemp,&cellLocate.time);
	gsmTimeSec = TIME_GetSec(&timeTemp);
	//if Cell is pass
	if(flagCellLocationUpdated)
		switch(flagCellLocationUpdated)
		{
			case 1:
				cellLat.value = DB_FloatToU32(cellLocate.lat);
				cellLon.value = DB_FloatToU32(cellLocate.lon);
				cellHDOP.value = DB_FloatToU32(cellLocate.uncertainty);
				cellDir.value = DB_FloatToU32(cellLocate.dir);
				DB_U32Save(&cellLat,CELL_LAT_ADDR);
				DB_U32Save(&cellLon,CELL_LON_ADDR);
				DB_U32Save(&cellHDOP,CELL_HDOP_ADDR);
				DB_U32Save(&cellDir,CELL_DIR_ADDR);
				UpdateRtcTime(gsmTimeSec);
				sendCellFlag = 1;
			case 2:
			default:
			break;
		}
	if(gsmGetTimeFlag)
	{
		gsmGetTimeFlag = 0;
		u32TimeTemp = TIME_GetSec(&sysGsmTime);
		__disable_irq();
		rtcTimeSec = u32TimeTemp;
		__enable_irq();
	}
	//Updata sysTime
	TIME_FromSec(&sysTime,rtcTimeSec);

		///////////////////////////////////////////////////////////
		// ASCII protocol
		///////////////////////////////////////////////////////////
	
	if(trackerEnable || sendCellFlag || saveGpsFlag)
	{
		saveGpsFlag = 0;
		trackerEnable = 0;
		logRecord.currentTime = sysTime;
		logRecord.speed = currentSpeed;
		
		if((cellLocate.lat <= 90 || cellLocate.lat >= -90) 
		&& (cellLocate.lon <= 180  || cellLocate.lon >= -180)
		&& cellLocate.uncertainty <= 3000
		)
		{
			logRecord.CELL_lat = neamFormatLatLng(cellLocate.lat);
			logRecord.CELL_lon = neamFormatLatLng(cellLocate.lon);
			logRecord.CELL_hdop = cellLocate.uncertainty;
			logRecord.CELL_ew = cellLocate.ew;
			logRecord.CELL_ns = cellLocate.ns;
			logRecord.gpsFixed = 0;
		}
		
		
		if(sendGpsFlag == 0 && sendCellFlag
		&& (cellLocate.lat <= 90 || cellLocate.lat >= -90) 
		&& (cellLocate.lon <= 180  || cellLocate.lon >= -180)
		&& cellLocate.uncertainty <= 3000
		)
		{
			sendCellFlag = 0;
			logRecord.CELL_lat = neamFormatLatLng(cellLocate.lat);
			logRecord.CELL_lon = neamFormatLatLng(cellLocate.lon);
			logRecord.CELL_hdop = cellLocate.uncertainty;
			logRecord.CELL_ew = cellLocate.ew;
			logRecord.CELL_ns = cellLocate.ns;
			logRecord.gpsFixed = 0;
		}
 		else if((lastGpsInfo.lon != 0) && (lastGpsInfo.lat != 0) && (lastGpsInfo.sig >= 1))
 		{
			sendGpsFlag = 0;
 			logRecord.GPS_lat = lastGpsInfo.lat;
 			logRecord.GPS_lon = lastGpsInfo.lon;
 			logRecord.GPS_hdop = lastGpsInfo.HDOP;	
 			logRecord.GPS_ew = lastGpsInfo.ew;
 			logRecord.GPS_ns = lastGpsInfo.ns;
 			logRecord.dir = lastNmeaInfo.direction;
			
			logRecord.CELL_lat = lastGpsInfo.lat;
			logRecord.CELL_lon = lastGpsInfo.lon;
			logRecord.CELL_hdop = lastGpsInfo.HDOP;
			logRecord.CELL_ew = lastGpsInfo.ew;
			logRecord.CELL_ns = lastGpsInfo.ns;
			
			logRecord.gpsFixed = 1;
 		}
		else
		{
			sendCellFlag = 0;
			sendGpsFlag = 0;
			logRecord.GPS_lat = 0;
			logRecord.GPS_lon = 0;
			logRecord.GPS_ew = 'E';
 			logRecord.GPS_ns = 'N';
			logRecord.GPS_hdop = 99.99;
			logRecord.gpsFixed = 0;
		}
		
		logRecord.mileage = mileage;
		
		logRecord.SOS_action = sosAction;
		//2
		logRecord.geoFenceCheck = sysCfg.geoFenceType;
		//3/4
		logRecord.GSM_csq = (uint8_t)csqValue;
		//5/6/7
		logRecord.batteryPercent = batteryPercent;
// 		//8
// 		if(CHGB_STATUS_IN)
// 			logRecord.status = 1;
// 		else
// 			logRecord.status = 0;
// 		//9
// 		if(batteryPercent < 5)
// 			logRecord.status |= 2<<2;
// 		else if(batteryPercent < 25)
// 			logRecord.status |= 1<<2;
// 		//10
// 		if (hasMoved)
// 		{
// 			hasMoved = 0;
// 			logRecord.status |= 1 << 3;
// 		}
// 		//11 Man Down Check
// 			if(sysCfg.featureSet & FEATURE_MAN_DOWN_SET)
// 				logRecord.status |= 1<<4;
// 		//12 Lone worker Check
// 			if(sysCfg.featureSet & FEATURE_LONE_WORKER_SET)
// 				logRecord.status |= 1<<5;
// 		//13 No Movement Check
// 			if(sysCfg.featureSet & FEATURE_NO_MOVEMENT_WARNING_SET)
// 				logRecord.status |= 1<<6;
 		logRecord.status = 0;
		if (hasMoved)
		{
			hasMoved = 0;
			logRecord.status |= 1 << 3;
		}
		logRecord.serverSent = 0;
		if(trackerMsg.newMsgIsEmpty && (!(sysCfg.featureSet & FEATURE_SUPPORT_FIFO_MSG)))
		{
			trackerMsg.newMsgIsEmpty = 0;
			trackerMsg.newMsg = logRecord;
			logRecord.serverSent = 1;
		}
		logRecordTemp = logRecord;
		DB_SaveDataLog((uint8_t *)&logRecord,&trackerLog);
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
	
	if((trackerMsg.newMsgIsEmpty == 0 || trackerMsg.oldMsgIsEmpty == 0) && 
		(GprsTask_GetPhase() == TASK_SYS_SLEEP))
	{
		GprsTask_StartTask(SEND_GPS_DATA_TASK);
	}
	
}



void PrintTrackerInfo(MSG_STATUS_RECORD *log)
{
//	DbgCfgPrintf("\r\nTRACKING LOG\r\n");
//	DbgCfgPrintf("TIME:%02d:%02d:%02d,",log->currentTime.hour,log->currentTime.min,log->currentTime.sec);
//	DbgCfgPrintf("DATE:%02d/%02d/%04d,",log->currentTime.mday,log->currentTime.month,log->currentTime.year);
//	DbgCfgPrintf("ID:%s",sysCfg.id);
//	DbgCfgPrintf("DRV No:%d",1);
//	DbgCfgPrintf("LAT:%f,%c",log->GPS_lat,log->GPS_ew);
//	DbgCfgPrintf("LNG:%f,%c",log->GPS_lon,log->GPS_ns);
//	DbgCfgPrintf("HDOP:%f",log->GPS_hdop);
//	DbgCfgPrintf("SPEED:%f",log->speed);
//	DbgCfgPrintf("DIR:%f",log->dir);
//	DbgCfgPrintf("MILEAGE:%f",log->mileage);
//	DbgCfgPrintf("GSM signal:%d",log->GSM_csq);
}
uint32_t CheckTrackerMsgFill(void)
{
	return DB_DataLogFill(&trackerLog);
}

uint32_t CheckTrackerMsgIsReady(void)
{
	if((!trackerMsg.oldMsgIsEmpty || !trackerMsg.newMsgIsEmpty))
		return 1;
	return 0;
}

uint32_t GetLastTrackerMsg(char *buff,uint32_t *len)
{
	if(!trackerMsg.lastMsgIsEmpty)
	{
		DGPS_Add(&trackerMsg.lastMsg,buff,len);
		trackerMsg.lastMsgIsEmpty = 1;
		//PrintTrackerInfo(&trackerMsg.lastMsg);
		return 0;
	}
	*len = 0;
	return 1;
}

uint32_t GetTrackerMsg(char *buff,uint32_t *len)
{
	if((!trackerMsg.oldMsgIsEmpty || !trackerMsg.newMsgIsEmpty))
	{
		if(!trackerMsg.newMsgIsEmpty)
		{
			if(trackerMsg.newMsg.gpsFixed)
			{
				DGPS_Add(&trackerMsg.newMsg,buff,len);
			}
			else
			{
				DCLI_Add(&trackerMsg.newMsg,buff,len);
			}
			trackerMsg.lastMsg = trackerMsg.newMsg;
			trackerMsg.newMsgIsEmpty = 1;
			trackerMsg.lastMsgIsEmpty = 0;
			//PrintTrackerInfo(&trackerMsg.newMsg);
		}
		else
		{
			
			if(trackerMsg.oldMsg.gpsFixed)
				DGPS_Add(&trackerMsg.oldMsg,buff,len);
			else
				DCLI_Add(&trackerMsg.oldMsg,buff,len);
			trackerMsg.lastMsg = trackerMsg.oldMsg;
			trackerMsg.oldMsgIsEmpty = 1;
			trackerMsg.lastMsgIsEmpty = 0;
			//PrintTrackerInfo(&trackerMsg.oldMsg);
		}
		return 0;
	}
	*len = 0;
	return 1;
}


//calculate haversine distance for linear distance
// float haversine_km(float lat1, float long1, float lat2, float long2)
// {
//     double dlong = (long2 - long1) * d2r;
//     double dlat = (lat2 - lat1) * d2r;
//     double a = pow(sin(dlat/2.0), 2) + cos(lat1*d2r) * cos(lat2*d2r) * pow(sin(dlong/2.0), 2);
//     double c = 2 * atan2(sqrt(a), sqrt(1-a));
//     double d = 6367 * c;

//     return d;
// }


