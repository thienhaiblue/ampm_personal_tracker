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
#include "common.h"
#include "system_config.h"
#include "tick.h"
#include "fmc.h"
#include "flash_FTFA.h"
#include "sos_tasks.h"

CONFIG_POOL sysCfg;
uint8_t sysUploadConfig = 2;

void CFG_Load(void)
{
	uint32_t saveFlag = 0,u32Temp = 0;
	int16_t i;
	uint8_t *u8Pt;
	Flash_Init(56);
	memcpy(&sysCfg, (void*)CONFIG_AREA_START, sizeof(CONFIG_POOL));
	u8Pt = (uint8_t *)&sysCfg;
	u32Temp = 0;
	for(i = 0;i < sizeof(CONFIG_POOL)-sizeof(sysCfg.crc);i++)
	{
		u32Temp += u8Pt[i];
	}
	if(u32Temp != sysCfg.crc)
	{
		memset((void*)&sysCfg, 0xFF, sizeof sysCfg);
		saveFlag = 1;
	}
	
	if(sysCfg.size != sizeof(CONFIG_POOL))
	{
		sysCfg.size = sizeof(CONFIG_POOL);
		saveFlag = 1;
	}
	
	if((uint8_t)sysCfg.imei[0] == 0xFF)
	{
		saveFlag = 1;
		sysCfg.imei[0] = 0;
	}
	
	
	if((uint8_t)sysCfg.id[0] == 0xFF)
	{
		saveFlag = 1;
		sysCfg.id[0] = 'Z';
		sysCfg.id[1] = 'T';
		sysCfg.id[2] = 'A';
		sysCfg.id[3] = '-';
		sysCfg.id[4] = '0';
		sysCfg.id[5] = '5';
		sysCfg.id[6] = 0;
	}
	
	// load default SMS config password
	if((uint8_t)sysCfg.smsPwd[0] == 0xFF || sysCfg.smsPwd[0] == 0)
	{
		saveFlag = 1;
		strcpy((char *)sysCfg.smsPwd, DEFAULT_SMS_PWD);
	}
	
	if((uint8_t)sysCfg.whiteCallerNo[0] == 0xFF)
	{
		saveFlag = 1;
		strcpy((char *)sysCfg.whiteCallerNo, DEFAULT_BOSS_NUMBER);
	}
	
	// load default GPRS config
	if((uint8_t)sysCfg.gprsApn[0] == 0xFF || sysCfg.gprsApn[0] == 0)
	{
		strcpy((char *)sysCfg.gprsApn, DEFAULT_GPSR_APN);
		strcpy((char *)sysCfg.gprsUsr, DEFAULT_GPRS_USR);
		strcpy((char *)sysCfg.gprsPwd, DEFAULT_GPRS_PWD);
		saveFlag = 1;
	}
	//////////////////////////////////////////////
	// load default data server IP
	if((sysCfg.priDserverIp[0]==0xFFFF || sysCfg.priDserverIp[1]==0xFFFF))
	{
		((uint8_t*)(sysCfg.priDserverIp))[0] = DEFAULT_DSERVER_IP0;
		((uint8_t*)(sysCfg.priDserverIp))[1] = DEFAULT_DSERVER_IP1;
		((uint8_t*)(sysCfg.priDserverIp))[2] = DEFAULT_DSERVER_IP2;
		((uint8_t*)(sysCfg.priDserverIp))[3] = DEFAULT_DSERVER_IP3;		
		sysCfg.priDserverPort = DEFAULT_DSERVER_PORT;
		
		((uint8_t*)(sysCfg.secDserverIp))[0] = DEFAULT_DSERVER_IP0;
		((uint8_t*)(sysCfg.secDserverIp))[1] = DEFAULT_DSERVER_IP1;
		((uint8_t*)(sysCfg.secDserverIp))[2] = DEFAULT_DSERVER_IP2;
		((uint8_t*)(sysCfg.secDserverIp))[3] = DEFAULT_DSERVER_IP3;		
		sysCfg.secDserverPort = DEFAULT_DSERVER_PORT;
		
		saveFlag = 1;
	}
	
	// load default data server name
	if((uint8_t)sysCfg.priDserverName[0] == 0xFF)
	{
		strcpy((char *)sysCfg.priDserverName, DEFAULT_DSERVER_NAME);
		strcpy((char *)sysCfg.secDserverName, DEFAULT_DSERVER_NAME);
		saveFlag = 1;
	}
	
	// use IP or domain
	if(sysCfg.dServerUseIp != 0 && sysCfg.dServerUseIp != 1)
	{
		sysCfg.dServerUseIp = DEFAULT_DSERVER_USEIP;
		saveFlag = 1;
	}
	/////////////////////////////////////////////
	// load default firmware server name
	if((sysCfg.priFserverIp[0]==0xFFFF || sysCfg.priFserverIp[1]==0xFFFF)){
		((uint8_t*)(sysCfg.priFserverIp))[0] = DEFAULT_FSERVER_IP0;
		((uint8_t*)(sysCfg.priFserverIp))[1] = DEFAULT_FSERVER_IP1;
		((uint8_t*)(sysCfg.priFserverIp))[2] = DEFAULT_FSERVER_IP2;
		((uint8_t*)(sysCfg.priFserverIp))[3] = DEFAULT_FSERVER_IP3;		
		sysCfg.priFserverPort = DEFAULT_FSERVER_PORT;
		
		((uint8_t*)(sysCfg.secFserverIp))[0] = DEFAULT_FSERVER_IP0;
		((uint8_t*)(sysCfg.secFserverIp))[1] = DEFAULT_FSERVER_IP1;
		((uint8_t*)(sysCfg.secFserverIp))[2] = DEFAULT_FSERVER_IP2;
		((uint8_t*)(sysCfg.secFserverIp))[3] = DEFAULT_FSERVER_IP3;		
		sysCfg.secFserverPort = DEFAULT_FSERVER_PORT;
		
		saveFlag = 1;
	}
	
	// load default firmware server name
	if((uint8_t)sysCfg.priFserverName[0] == 0xFF)
	{
		strcpy((char *)sysCfg.priFserverName, DEFAULT_FSERVER_NAME);
		strcpy((char *)sysCfg.secFserverName, DEFAULT_FSERVER_NAME);
		saveFlag = 1;
	}
	
	// load default driver name
	if(sysCfg.userIndex == 0xFF)
	{
		sysCfg.userIndex = 0;
		saveFlag = 1;
	}
	// use IP or domain
	if(sysCfg.fServerUseIp != 0 && sysCfg.fServerUseIp != 1)
	{
		sysCfg.fServerUseIp = DEFAULT_FSERVER_USEIP;
		saveFlag = 1;
	}
	
	for(i=0;i<CONFIG_MAX_USER;i++)
	{
		if((uint8_t)sysCfg.userList[i].userName[0] == 0xFF){
			sysCfg.userList[i].userName[0] = 0;
			sysCfg.userList[i].phoneNo[0] = 0;
			saveFlag = 1;
		}
	}
	
	for(i=0;i<10;i++)
	{
		if(sysCfg.geoFence[i].geoFenceEnable == 0xFF){
			sysCfg.geoFence[i].geoFenceEnable = 0;
			sysCfg.geoFence[i].lat = 0;
			sysCfg.geoFence[i].lon = 0;
			sysCfg.geoFence[i].radius = 0;
			sysCfg.geoFence[i].time = 0;
			saveFlag = 1;
		}
	}

	if(sysCfg.featureSet == 0xFFFF)
	{
		saveFlag = 1;
		sysCfg.featureSet = 0;
	}
	
	// report interval
	if(sysCfg.reportInterval == 0xFFFF)
	{
		sysCfg.reportInterval = DEFAULT_REPORT_INTERVAL;
		saveFlag = 1;
	}
	if(sysCfg.SOS_detection == 0xff)
	{
		sysCfg.SOS_detection = 0;
		saveFlag = 1;
	}
	if(sysCfg.geoFenceType == 0xff)
	{
		sysCfg.geoFenceType = 0;
		saveFlag = 1;
	}
	if(sysCfg.loneWorkerInterval == 0xffff)
	{
		sysCfg.loneWorkerInterval = DEFAULT_LONE_WORKER_INTERVAL;
		saveFlag = 1;
	}
	
	sysCfg.alarmCountdownTime = DEFAULT_ALARM_COUNTDOWN_TIME;
	saveFlag = 1;
	
	if(sysCfg.noMovementInterval == 0xffff)
	{
		sysCfg.noMovementInterval = DEFAULT_NO_MOVEMENT_INTERVAL;
		saveFlag = 1;
	}
	
	if(sysCfg.operationMode == 0xff)
	{
		sysCfg.operationMode = DEFAULT_OPERATION_MODE;
		saveFlag = 1;
	}
	
	if(sysCfg.firmwareUpdateEnable == 0xff)
	{
		sysCfg.firmwareUpdateEnable = 0;
		saveFlag = 1;
	}
	if(sysCfg.language == 0xff)
	{
		sysCfg.language = 0;
		saveFlag = 1;
	}
	
	if(sysCfg.familyPalMode == 0xff)
	{
		sysCfg.familyPalMode = MODE_CALL_CENTER;
		saveFlag = 1;
	}
	
	if ((sysCfg.callMeSms[0] == 0) || (sysCfg.callMeSms[0] == 0xff))
	{
		strcpy((char*)sysCfg.callMeSms, DEFAULT_CALL_ME_SMS);
		saveFlag = 1;
	}
	
	if ((sysCfg.sosRespondedSms[0] == 0) || (sysCfg.sosRespondedSms[0] == 0xff))
	{
		strcpy((char*)sysCfg.sosRespondedSms, DEFAULT_SOS_RESPONDED_SMS);
		saveFlag = 1;
	}
	
	if ((sysCfg.sosCancelledSms[0] == 0) || (sysCfg.sosCancelledSms[0] == 0xff))
	{
		strcpy((char*)sysCfg.sosCancelledSms, DEFAULT_SOS_CANCELLED_SMS);
	}
	
	if(saveFlag)
	{
		CFG_Save();
	}
}

uint8_t CFG_Check(CONFIG_POOL *sysCfg)
{

	uint32_t saveFlag = 0,u32Temp = 0;
	int16_t i;
	uint8_t *u8Pt;
	u8Pt = (uint8_t *)sysCfg;
	u32Temp = 0;
	
	for(i = 0;i < sizeof(CONFIG_POOL)-sizeof(sysCfg->crc);i++)
	{
		u32Temp += u8Pt[i];
	}
	if(u32Temp != sysCfg->crc)
	{
		return 0;
	}
	
	if(sysCfg->size != sizeof(CONFIG_POOL))
	{
		sysCfg->size = sizeof(CONFIG_POOL);
		saveFlag = 1;
	}
	
	if((uint8_t)sysCfg->imei[0] == 0xFF)
	{
		saveFlag = 1;
		sysCfg->imei[0] = 0;
	}
	
	
	if((uint8_t)sysCfg->id[0] == 0xFF)
	{
		saveFlag = 1;
		sysCfg->id[0] = 'Z';
		sysCfg->id[1] = 'T';
		sysCfg->id[2] = 'A';
		sysCfg->id[3] = '-';
		sysCfg->id[4] = '0';
		sysCfg->id[5] = '5';
		sysCfg->id[6] = 0;
	}
	
	// load default SMS config password
	if((uint8_t)sysCfg->smsPwd[0] == 0xFF || sysCfg->smsPwd[0] == 0)
	{
		saveFlag = 1;
		strcpy((char *)sysCfg->smsPwd, DEFAULT_SMS_PWD);
	}
	
	if((uint8_t)sysCfg->whiteCallerNo[0] == 0xFF)
	{
		saveFlag = 1;
		strcpy((char *)sysCfg->whiteCallerNo, DEFAULT_BOSS_NUMBER);
	}
	
	// load default GPRS config
	if((uint8_t)sysCfg->gprsApn[0] == 0xFF || sysCfg->gprsApn[0] == 0)
	{
		strcpy((char *)sysCfg->gprsApn, DEFAULT_GPSR_APN);
		strcpy((char *)sysCfg->gprsUsr, DEFAULT_GPRS_USR);
		strcpy((char *)sysCfg->gprsPwd, DEFAULT_GPRS_PWD);
		saveFlag = 1;
	}
	//////////////////////////////////////////////
	// load default data server IP
	if((sysCfg->priDserverIp[0]==0xFFFF || sysCfg->priDserverIp[1]==0xFFFF))
	{
		((uint8_t*)(sysCfg->priDserverIp))[0] = DEFAULT_DSERVER_IP0;
		((uint8_t*)(sysCfg->priDserverIp))[1] = DEFAULT_DSERVER_IP1;
		((uint8_t*)(sysCfg->priDserverIp))[2] = DEFAULT_DSERVER_IP2;
		((uint8_t*)(sysCfg->priDserverIp))[3] = DEFAULT_DSERVER_IP3;		
		sysCfg->priDserverPort = DEFAULT_DSERVER_PORT;
		
		((uint8_t*)(sysCfg->secDserverIp))[0] = DEFAULT_DSERVER_IP0;
		((uint8_t*)(sysCfg->secDserverIp))[1] = DEFAULT_DSERVER_IP1;
		((uint8_t*)(sysCfg->secDserverIp))[2] = DEFAULT_DSERVER_IP2;
		((uint8_t*)(sysCfg->secDserverIp))[3] = DEFAULT_DSERVER_IP3;		
		sysCfg->secDserverPort = DEFAULT_DSERVER_PORT;
		
		saveFlag = 1;
	}
	
	// load default data server name
	if((uint8_t)sysCfg->priDserverName[0] == 0xFF)
	{
		strcpy((char *)sysCfg->priDserverName, DEFAULT_DSERVER_NAME);
		strcpy((char *)sysCfg->secDserverName, DEFAULT_DSERVER_NAME);
		saveFlag = 1;
	}
	
	// use IP or domain
	if(sysCfg->dServerUseIp != 0 && sysCfg->dServerUseIp != 1)
	{
		sysCfg->dServerUseIp = DEFAULT_DSERVER_USEIP;
		saveFlag = 1;
	}
	/////////////////////////////////////////////
	// load default firmware server name
	if((sysCfg->priFserverIp[0]==0xFFFF || sysCfg->priFserverIp[1]==0xFFFF)){
		((uint8_t*)(sysCfg->priFserverIp))[0] = DEFAULT_FSERVER_IP0;
		((uint8_t*)(sysCfg->priFserverIp))[1] = DEFAULT_FSERVER_IP1;
		((uint8_t*)(sysCfg->priFserverIp))[2] = DEFAULT_FSERVER_IP2;
		((uint8_t*)(sysCfg->priFserverIp))[3] = DEFAULT_FSERVER_IP3;		
		sysCfg->priFserverPort = DEFAULT_FSERVER_PORT;
		
		((uint8_t*)(sysCfg->secFserverIp))[0] = DEFAULT_FSERVER_IP0;
		((uint8_t*)(sysCfg->secFserverIp))[1] = DEFAULT_FSERVER_IP1;
		((uint8_t*)(sysCfg->secFserverIp))[2] = DEFAULT_FSERVER_IP2;
		((uint8_t*)(sysCfg->secFserverIp))[3] = DEFAULT_FSERVER_IP3;		
		sysCfg->secFserverPort = DEFAULT_FSERVER_PORT;
		
		saveFlag = 1;
	}
	
	// load default firmware server name
	if((uint8_t)sysCfg->priFserverName[0] == 0xFF)
	{
		strcpy((char *)sysCfg->priFserverName, DEFAULT_FSERVER_NAME);
		strcpy((char *)sysCfg->secFserverName, DEFAULT_FSERVER_NAME);
		saveFlag = 1;
	}
	
	// load default driver name
	if(sysCfg->userIndex == 0xFF)
	{
		sysCfg->userIndex = 0;
		saveFlag = 1;
	}
	// use IP or domain
	if(sysCfg->fServerUseIp != 0 && sysCfg->fServerUseIp != 1)
	{
		sysCfg->fServerUseIp = DEFAULT_FSERVER_USEIP;
		saveFlag = 1;
	}
	
	for(i=0;i<CONFIG_MAX_USER;i++)
	{
		if((uint8_t)sysCfg->userList[i].userName[0] == 0xFF){
			sysCfg->userList[i].userName[0] = 0;
			sysCfg->userList[i].phoneNo[0] = 0;
			saveFlag = 1;
		}
	}
	
	for(i=0;i<10;i++)
	{
		if(sysCfg->geoFence[i].geoFenceEnable == 0xFF){
			sysCfg->geoFence[i].geoFenceEnable = 0;
			sysCfg->geoFence[i].lat = 0;
			sysCfg->geoFence[i].lon = 0;
			sysCfg->geoFence[i].radius = 0;
			sysCfg->geoFence[i].time = 0;
			saveFlag = 1;
		}
	}

	if(sysCfg->featureSet == 0xFFFF)
	{
		saveFlag = 1;
		sysCfg->featureSet = 0;
	}
	
	// report interval
	if(sysCfg->reportInterval == 0xFFFF)
	{
		sysCfg->reportInterval = DEFAULT_REPORT_INTERVAL;
		saveFlag = 1;
	}
	if(sysCfg->SOS_detection == 0xff)
	{
		sysCfg->SOS_detection = 0;
		saveFlag = 1;
	}
	if(sysCfg->geoFenceType == 0xff)
	{
		sysCfg->geoFenceType = 0;
		saveFlag = 1;
	}
	if(sysCfg->loneWorkerInterval == 0xffff)
	{
		sysCfg->loneWorkerInterval = DEFAULT_LONE_WORKER_INTERVAL;
		saveFlag = 1;
	}
	
	sysCfg->alarmCountdownTime = DEFAULT_ALARM_COUNTDOWN_TIME;
	
	if(sysCfg->noMovementInterval == 0xffff)
	{
		sysCfg->noMovementInterval = DEFAULT_NO_MOVEMENT_INTERVAL;
		saveFlag = 1;
	}
	
	if(sysCfg->operationMode == 0xff)
	{
		sysCfg->operationMode = DEFAULT_OPERATION_MODE;
		saveFlag = 1;
	}
	
	if(sysCfg->firmwareUpdateEnable == 0xff)
	{
		sysCfg->firmwareUpdateEnable = 0;
		saveFlag = 1;
	}
	if(sysCfg->language == 0xff)
	{
		sysCfg->language = 0;
		saveFlag = 1;
	}
	
	if(sysCfg->familyPalMode == 0xff)
	{
		sysCfg->familyPalMode = MODE_CALL_CENTER;
		saveFlag = 1;
	}
	
	if ((sysCfg->callMeSms[0] == 0) || (sysCfg->callMeSms[0] == 0xff))
	{
		strcpy((char*)sysCfg->callMeSms, DEFAULT_CALL_ME_SMS);
		saveFlag = 1;
	}
	
	if ((sysCfg->sosRespondedSms[0] == 0) || (sysCfg->sosRespondedSms[0] == 0xff))
	{
		strcpy((char*)sysCfg->sosRespondedSms, DEFAULT_SOS_RESPONDED_SMS);
		saveFlag = 1;
	}
	
	if ((sysCfg->sosCancelledSms[0] == 0) || (sysCfg->sosCancelledSms[0] == 0xff))
	{
		strcpy((char*)sysCfg->sosCancelledSms, DEFAULT_SOS_CANCELLED_SMS);
	}
	
	if (saveFlag)
	{
		return 0;
	}
	
	return 1;
}

uint8_t CFG_CheckSum(CONFIG_POOL *sysCfg)
{
	uint32_t u32Temp = 0;
	int16_t i;
	uint8_t *u8Pt;
	u8Pt = (uint8_t *)sysCfg;
	u32Temp = 0;
	for(i = 0;i < sizeof(CONFIG_POOL)-sizeof(sysCfg->crc);i++)
	{
		u32Temp += u8Pt[i];
	}
	if(u32Temp != sysCfg->crc)
	{
		return 0;
	}
	return 1;
}

void CFG_Save(void)
{
	int16_t i;
	uint32_t u32Temp;
	uint8_t *u8Pt;
	uint32_t *cfgdest;
	
	if (sysCfg.featureSet & FEATURE_LONE_WORKER_SET)
	{
		flagLoneWorkerReset = 1;
	}
	if (sysCfg.featureSet & FEATURE_NO_MOVEMENT_WARNING_SET)
	{
		flagNoMovementReset = 1;
	}
	if (sysCfg.featureSet & FEATURE_MAN_DOWN_SET)
	{
		flagManDownReset = 1;
	}
	
	__disable_irq();	
	
	u8Pt = (uint8_t *)&sysCfg;
	u32Temp = 0;
	for(i = 0;i < sizeof(CONFIG_POOL)-sizeof(sysCfg.crc);i++)
	{
		u32Temp += u8Pt[i];
	}
	sysCfg.crc = u32Temp;
	for(i=0; i <= sizeof(CONFIG_POOL)/ PAGE_SIZE; i++) 
	{
		if(Flash_SectorErase((uint32_t)(CONFIG_AREA_START + i*PAGE_SIZE)) != Flash_OK)
			NVIC_SystemReset();
	}
	cfgdest = (uint32_t*)&sysCfg;
	
	for(i=0; i < sizeof(CONFIG_POOL); i+=4) 
	{
		//FLASH_ProgramWord(CONFIG_AREA_START + i, *(uint32_t*)(cfgdest + i/4));
		if(Flash_ByteProgram(CONFIG_AREA_START + i,(uint32_t *)(cfgdest + i/4),4) != Flash_OK)
			NVIC_SystemReset();
		if(*(uint32_t*)(cfgdest + i/4) != *(uint32_t*)(CONFIG_AREA_START + i)) //check wrote data
		{
			NVIC_SystemReset();
		}
	}
	__enable_irq();
}



void CFG_Write(uint8_t *buff, uint32_t address, int offset, int length)
{
	int16_t i;
	uint32_t *cfgdest;
	cfgdest = (uint32_t *)buff;
	if(offset == 0)
	{
		if(Flash_SectorErase(address) != Flash_OK)
			NVIC_SystemReset();
	}
	else
	{
		for(i=0; i < length; i+=4) 
		{
			if(Flash_ByteProgram(address + offset + i,(uint32_t *)(cfgdest + i/4),4) != Flash_OK)
				NVIC_SystemReset();
			if(*(uint32_t*)(cfgdest + i/4) != *(uint32_t*)(CONFIG_AREA_START + i)) //check wrote data
			{
				NVIC_SystemReset();
			}
		}
	}
}
