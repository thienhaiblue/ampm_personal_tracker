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
#include "tick.h"
#include "common.h"
#include "hw_config.h"
#include "gps.h"
#include "system_config.h"
#include "tracker.h"
#include "power_management.h"
#include "modem.h"
#include "tick.h"
#include "accelerometer.h"
#include "project_common.h"
#include "at_command_parser.h"
#include "gsm_gprs_tasks.h"
#include "io_tasks.h"
#include "gps_task.h"

extern uint32_t batteryPercent;

extern uint8_t powerOffDevice;
extern uint8_t incomingCall;
extern uint8_t ringingCount;
extern uint8_t missCall;

extern uint8_t powerAndSosKeyHold3Sec;
extern uint8_t powerAndSosKeyHold1Sec;
extern uint8_t sosKeyPressed;
extern uint8_t powerKeyHold3Sec;
extern uint8_t powerKeyHold2Sec;
extern uint8_t powerKeyHold1Sec;
extern uint8_t powerKeyPressed;
extern uint8_t initEmergencyCall;

volatile uint32_t rtcTimeSec = 0;
#define GetCurrentTime  TICK_Get

#define LPTMR_USE_IRCLK 0 
#define LPTMR_USE_LPOCLK 1
#define LPTMR_USE_ERCLK32 2
#define LPTMR_USE_OSCERCLK 3

volatile uint32_t tickCounter32 = 0;
volatile uint64_t tickCounter64 = 0;

extern uint8_t loneWorkerSos;
extern uint8_t noMovementSos;
extern uint8_t manDownSos;

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

#define CALIB_FREQ	60


uint32_t calibFlag = 0;
uint32_t PIT_cntValue;
uint32_t LPTM_calibValue = 1000;
uint32_t LPT_tickSec = 0;
uint32_t LPTM_CalcOk = 0;
uint32_t loneWorkerCountdownCnt = 0;
uint32_t loneWorkerIntervalCnt = 0;
uint8_t loneWorkerAlarmActiveFlag = 0;
uint8_t loneWorkerWarningFlag = 0;
uint8_t flagLoneWorkerReset = 1;
uint32_t noMovementCountdownCnt = 0;
uint32_t noMovementIntervalCnt = 0;
uint8_t noMovementAlarmActiveFlag = 0;
uint8_t noMovementWarningFlag = 0;
uint8_t flagNoMovementReset = 1;
uint8_t manDownCountdownCnt = 0;
uint32_t manDownIntervalCnt = 0;
uint8_t manDownAlarmActiveFlag = 0;
uint8_t manDownWarningFlag = 0;
uint8_t flagManDownReset = 1;
uint8_t ringingDetectFlag = 0;
uint8_t ringingCountDown = 0;
uint8_t ringingFlag = 0;
uint32_t LPTcnt = 0;

void LedControl(void);
void ChargerLedControl(void);
void GpsLedControl(void);
void GsmLedControl(void);
void KeyTask_Control(void);


void LPTimer_IRQHandler (void)
{ 
		LPTimerTask();
}

void LPTimerTask(void)
{
	uint32_t u32Temp;
	SIM_SCGC5 |= SIM_SCGC5_LPTMR_MASK;
  LPTMR0_CSR |=  LPTMR_CSR_TCF_MASK;   // write 1 to TCF to clear the LPT timer compare flag
  LPTMR0_CSR = ( LPTMR_CSR_TEN_MASK | LPTMR_CSR_TIE_MASK | LPTMR_CSR_TCF_MASK  );
	ChargerLedControl();
	KeyTask_Control();
	if(pwrStatus.value == POWER_ON)
	{
		LedControl();
		LPTcnt++;
		if(LPTcnt >= 4)
		{
			LPTcnt = 0;
			rtcTimeSec++;
			LPT_tickSec++;
			
			if(flagLoneWorkerReset)
			{
				flagLoneWorkerReset = 0;
				loneWorkerAlarmActiveFlag = 0;
				loneWorkerWarningFlag = 0;
				loneWorkerIntervalCnt = sysCfg.loneWorkerInterval * 60;
				loneWorkerCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
			}
			if (flagNoMovementReset)
			{
				flagNoMovementReset = 0;
				noMovementAlarmActiveFlag = 0;
				noMovementWarningFlag = 0;
				noMovementIntervalCnt = sysCfg.noMovementInterval * 60;
				noMovementCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
			}
			if(flagManDownReset)
			{
				flagManDownReset = 0;
				manDownAlarmActiveFlag = 0;
				manDownWarningFlag = 0;
				manDownIntervalCnt = DEFAULT_MAN_DOWN_INTERVAL;
				manDownCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
			}
			
			if (sysCfg.featureSet & FEATURE_LONE_WORKER_SET)
			{
				if (loneWorkerAlarmActiveFlag == 0)
				{
					if(loneWorkerIntervalCnt) 
					{
						loneWorkerIntervalCnt--;
					}
					else
					{
						loneWorkerAlarmActiveFlag = 1;
						loneWorkerWarningFlag = 1;
						PlayAlarmWarningSound(1);
						loneWorkerIntervalCnt = sysCfg.loneWorkerInterval*60;
						loneWorkerCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
					}
				}
				else
				{
					if(loneWorkerCountdownCnt)
					{
						loneWorkerCountdownCnt--;
						if ((loneWorkerCountdownCnt < 6) && ((loneWorkerCountdownCnt % 2) == 1))
						{
							PlayAlarmWarningSound(0);
						}
					}
					else
					{
						loneWorkerAlarmActiveFlag = 0;
						loneWorkerSos = 1;
						flagLoneWorkerReset = 1;
						initEmergencyCall = 1;
					}
				}
			}
			else
			{
				loneWorkerIntervalCnt = sysCfg.loneWorkerInterval * 60;
				loneWorkerCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
				loneWorkerAlarmActiveFlag = 0;
			}
			
			if ((sysCfg.featureSet & FEATURE_NO_MOVEMENT_WARNING_SET) && ((sysCfg.featureSet & FEATURE_MAN_DOWN_SET) == 0))
			{
				if (noMovementAlarmActiveFlag == 0)
				{
					if(noMovementIntervalCnt) 
					{
						noMovementIntervalCnt--;
					}
					else
					{
						noMovementAlarmActiveFlag = 1;
						noMovementWarningFlag = 1;
						PlayAlarmWarningSound(1);
						noMovementIntervalCnt = sysCfg.noMovementInterval*60;
						noMovementCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
					}
				}
				else
				{
					if(noMovementCountdownCnt)
					{
						noMovementCountdownCnt--;
						if ((noMovementCountdownCnt < 6) && ((noMovementCountdownCnt % 2) == 1))
						{
							PlayAlarmWarningSound(0);
						}
					}
					else
					{
						noMovementAlarmActiveFlag = 0;
						noMovementSos = 1;
						flagNoMovementReset = 1;
						initEmergencyCall = 1;				
					}
				}
			}
			else
			{
				noMovementIntervalCnt = sysCfg.noMovementInterval * 60;
				noMovementCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
				noMovementAlarmActiveFlag = 0;
			}
			
			if (sysCfg.featureSet & FEATURE_MAN_DOWN_SET)
			{
				if (CHG_EN_IN == 0)
				{
					if (manDownAlarmActiveFlag == 0)
					{
						if(manDownIntervalCnt) 
						{
							manDownIntervalCnt--;
						}
						else
						{
							manDownAlarmActiveFlag = 1;
							manDownWarningFlag = 1;
							PlayAlarmWarningSound(1);
							manDownIntervalCnt = DEFAULT_MAN_DOWN_INTERVAL;
							manDownCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
						}
					}
					else
					{
						if(manDownCountdownCnt)
						{
							manDownCountdownCnt--;
							if ((manDownCountdownCnt < 6) && ((manDownCountdownCnt % 2) == 1))
							{
								PlayAlarmWarningSound(0);
							}
						}
						else
						{
							manDownAlarmActiveFlag = 0;
							manDownSos = 1;
							flagManDownReset = 1;
							initEmergencyCall = 1;				
						}
					}
				}
				else
				{
					manDownIntervalCnt = DEFAULT_MAN_DOWN_INTERVAL;
					manDownCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
					manDownAlarmActiveFlag = 0;
				}
			}
			else
			{
				manDownIntervalCnt = DEFAULT_MAN_DOWN_INTERVAL;
				manDownCountdownCnt = DEFAULT_ALARM_COUNTDOWN_TIME + 1;
				manDownAlarmActiveFlag = 0;
			}
			
			if(ringingFlag && ringingDetectFlag == 0)
			{
				ringingFlag = 0;
				ringingCountDown = 5;
				ringingDetectFlag = 1;
			}
			if(ringingDetectFlag)
			{
				if(ringingCountDown) 
				{
					ringingCountDown--;
				}
				else
				{
					ringingDetectFlag = 0;
					if(ringingFlag)
					{
						incomingCall = 1;
						ringFlag = 1;
					}
					else
					{
						smsNewMessageFlag = 1;
					}
				}
			}
			
			if (VoiceCallTask_GetPhase() == INCOMING_CALL)
			{
				ringingCount++;
				if (ringingCount > 5)
				{
					missCall = 1;
					ringingCount = 0;
				}
			}
			else
			{
				ringingCount = 0;
			}
		}
	}
	else
	{
		LED_GREEN_PIN_SET;
		LED_BLUE_PIN_SET;
		if(PWR_KEY_IN == 0 && SOS_KEY_IN == 0)
			NVIC_SystemReset();
	}

}


void UpdateRtcTime(uint32_t updateTimeSec)
{
	//if (updateTimeSec > rtcTimeSec)
	{
		__disable_irq();
		rtcTimeSec = updateTimeSec;
		//Updata sysTime
		TIME_FromSec(&sysTime,rtcTimeSec);
		flagSaveTime = 1;
		__enable_irq();
	}
}

void TICK_Init(uint32_t timeMs,uint32_t coreClock) 
{ 
	SysTick->CTRL = 0x00;
	SysTick->LOAD  = ((coreClock/1000)*timeMs - 1);      // set reload register
	/* preemption = TICK_PRIORITY, sub-priority = 1 */
	NVIC_SetPriority(SysTick_IRQn, (1<< 0) - 1);
  SysTick->VAL   =  0;                                          // clear  the counter
  SysTick->CTRL = 0x07;   
} // end

void TICK_DeInit(void)
{
	SysTick->CTRL = 0x00;
}

uint8_t ledControlEnable = 0;
uint8_t powerOnLedFlash = 0;
uint16_t powerOnLedFlashCnt = 0;
uint16_t redLedFlashCnt = 0;
uint16_t greenLedFlashCnt = 0;
uint16_t blueLedFlashCnt = 0;

KEY_TYPE key;

uint8_t GetKeyPressed(_KEY_TYPE *key)
{
	if(key->pressed)
	{
		key->pressed = 0;
		return 1;
	}
	return 0;
}

uint8_t CheckKeyPressed(_KEY_TYPE *key)
{
	if(key->pressed)
		return 1;
	return 0;
}

uint32_t GetKeyPressTime(_KEY_TYPE *key)
{
	if (key->pressed)
	{
		return key->pressTime;
	}
	else
	{
		return 0;
	}
}

void ClearLastKeyStatus(_KEY_TYPE *key)
{
	key->lastStatus = KEY_UP;
	key->lastTime = 0;
}

void LedControl(void)
{
	#ifndef DEVICE_TESTING
	if(ledControlEnable && 	pwrStatus.value == POWER_ON)
	{
		// power on: blink all leds 300ms ON/300ms OFF 3 times
		if (powerOnLedFlash)
		{
			powerOnLedFlashCnt += 250;
			if (powerOnLedFlashCnt > 2000)
			{
				LED_RED_PIN_SET;
				LED_GREEN_PIN_SET;
				LED_BLUE_PIN_SET;
				powerOnLedFlashCnt = 0;
				powerOnLedFlash = 0;
			}
			else if (((powerOnLedFlashCnt >= 500) && (powerOnLedFlashCnt <= 800)) || ((powerOnLedFlashCnt >= 1100) && (powerOnLedFlashCnt <= 1400)) ||
								((powerOnLedFlashCnt >= 1700) && (powerOnLedFlashCnt <= 2000)))
			{
				LED_RED_PIN_CLR;
				LED_GREEN_PIN_CLR;
				LED_BLUE_PIN_CLR;
			}
			else
			{
				LED_RED_PIN_SET;
				LED_GREEN_PIN_SET;
				LED_BLUE_PIN_SET;
			}
		}
		else
		{
			GpsLedControl();
			GsmLedControl();
		}
	}
	else
	{
		//powerOnLedFlash = 1;
		LED_RED_PIN_SET;
		LED_GREEN_PIN_SET;
		LED_BLUE_PIN_SET;
		
	}
	#endif
}

void GpsLedControl(void)
{
	// GPS signal 
	//	if GPS is fixed: solid led
	if(gpsInfo.sig /*&& GPS_EN_IN*/)
	{
		LED_BLUE_PIN_CLR;
	}
	else // blink 300ms ON/2000ms OFF
	{
		blueLedFlashCnt += 250;
		if (blueLedFlashCnt > 2300)
		{
			blueLedFlashCnt = 0;
		}
		if (blueLedFlashCnt <= 250)
		{
			LED_BLUE_PIN_CLR;
		}
		else
		{
			LED_BLUE_PIN_SET;
		}
	}
}

void GsmLedControl(void)
{
// if GSM is OK: blink 300ms ON/300ms OFF 2 times for every 15s
	if (flagGsmStatus != GSM_NO_REG_STATUS)
	{
		if ((GprsTask_GetPhase() >= TASK_GPRS_SERVER_CONNECT) && (GprsTask_GetPhase() <= TASK_GPRS_SERVER_DISCONECT))
		{
			// server connected --> solid led
			LED_GREEN_PIN_CLR;
			greenLedFlashCnt = 0;
		}
		else
		{
			greenLedFlashCnt += 250;
			if (greenLedFlashCnt > 15000)
			{
				greenLedFlashCnt = 0;
			}
			if ((greenLedFlashCnt < 250) || ((greenLedFlashCnt >= 500) && (greenLedFlashCnt < 750)))
			{
				LED_GREEN_PIN_CLR;
			}
			else
			{
				LED_GREEN_PIN_SET;
			}
		}
	}
	else // led off
	{
		LED_GREEN_PIN_SET;
	}
}

void ChargerLedControl(void)
{
	// if charger is plugged in
		if(CHG_EN_IN)
		{
			// if battery full: solid led
			if(batteryPercent >= 98)
			{
				LED_RED_PIN_CLR;
			}
			else // blink 1s ON/1s OFF
			{
				redLedFlashCnt += 250;
				if (redLedFlashCnt >= 2000)
				{
					redLedFlashCnt = 0;
				}
				if (redLedFlashCnt < 1000)
				{
					LED_RED_PIN_CLR;
				}
				else
				{
					LED_RED_PIN_SET;
				}
			}
		}
		else if(batteryPercent < 25) // if low battery: blink 300ms ON/2000ms OFF
		{
			redLedFlashCnt += 250;
			if (redLedFlashCnt > 2300)
			{
				redLedFlashCnt = 0;
			}
			if (redLedFlashCnt <= 250)
			{
				LED_RED_PIN_CLR;
			}
			else
			{
				LED_RED_PIN_SET;
			}
		}
		else // battery OK: led off
		{
			LED_RED_PIN_SET;
		}
}


void KeyTask_Control(void)
{
	uint8_t keyStatus;
	keyStatus = SOS_KEY_IN;
	if (keyStatus == key.sos.status)
	{
		key.sos.time += 250;
	}
	else
	{
		key.sos.lastStatus = key.sos.status;
		key.sos.lastTime = key.sos.time;
		key.sos.status = keyStatus;
		key.sos.time = 0;
		if (keyStatus == KEY_DOWN)
		{
			if (key.pwr.status == KEY_UP)
			{
				ClearLastKeyStatus(&key.pwr);
			}
		}
	}
	
	keyStatus = PWR_KEY_IN;
	if (keyStatus == key.pwr.status)
	{
		key.pwr.time += 250;
	}
	else
	{
		key.pwr.lastStatus = key.pwr.status;
		key.pwr.lastTime = key.pwr.time;
		key.pwr.status = keyStatus;
		key.pwr.time = 0;
		if (keyStatus == KEY_DOWN)
		{
			if (key.sos.status == KEY_UP)
			{
				ClearLastKeyStatus(&key.sos);
			}
		}
	}
	
	if ((key.sos.status == KEY_UP) && (key.pwr.status == KEY_UP)) // both keys have to be released.
	{
		if ((key.sos.lastStatus == KEY_DOWN) || (key.pwr.lastStatus == KEY_DOWN))
		{
			Keys_Init();
		}
		else if ((key.sos.time > 3000) && (key.pwr.time > 3000))
		{
			Keys_Init();
		}
		if ((key.sos.lastStatus == KEY_DOWN) && (key.pwr.lastStatus == KEY_DOWN)) // sos & pwr button is pressed at the same time.
		{
			if ((key.sos.lastTime >= 3000) && (key.pwr.lastTime >= 3000))
			{
				ClearLastKeyStatus(&key.sos);
				ClearLastKeyStatus(&key.pwr);
				powerAndSosKeyHold3Sec = 1;
			}
			else if ((key.sos.lastTime >= 1000) && (key.pwr.lastTime >= 1000))
			{
				ClearLastKeyStatus(&key.sos);
				ClearLastKeyStatus(&key.pwr);
				powerAndSosKeyHold1Sec = 1;
			}
		}
		else if (key.sos.lastStatus == KEY_DOWN)
		{
			if (key.sos.lastTime >= 100) // sos button pressed for 1 sec.
			{
				ClearLastKeyStatus(&key.sos);
				sosKeyPressed = 1;
			}
		}
		else if (key.pwr.lastStatus == KEY_DOWN)
		{
			if (key.pwr.lastTime >= 3000) // pwr button pressed for 3 sec.
			{
				ClearLastKeyStatus(&key.pwr);
				powerKeyHold3Sec = 1;
				powerKeyPressed = 1;
			}
			else if (key.pwr.lastTime >= 2000) // pwr button pressed for 2 sec.
			{
				ClearLastKeyStatus(&key.pwr);
				powerKeyHold2Sec = 1;
				powerKeyPressed = 1;
			}
			else if (key.pwr.lastTime >= 1000) // pwr button pressed for 1 sec.
			{
				ClearLastKeyStatus(&key.pwr);
				powerKeyHold1Sec = 1;
				powerKeyPressed = 1;
			}
			else if (key.pwr.lastTime >= 100) // pwr button pressed
			{
				ClearLastKeyStatus(&key.pwr);
				powerKeyPressed = 1;
			}
		}
	}
	else
	{
		Keys_Init();
	}
}

void SysTick_Handler(void)
{
	tickCounter32++;
	tickCounter64++;
}

uint32_t TICK_Get(void)
{
	return tickCounter32;
}

uint32_t TICK_Get64(void)
{
   return tickCounter64;
}

void DelayMs(uint32_t ms)
{
   	uint32_t currentTicks = TICK_Get();
		while(TICK_Get()- currentTicks < ms);
}

void InitTimeout(Timeout_Type *t,uint32_t timeout)
{
	t->start_time = GetCurrentTime();
	t->timeout = timeout;
	t->crc = t->start_time + t->timeout;
}

uint32_t CheckTimeout(Timeout_Type *t)
{
	uint32_t u32temp,u32temp1;
	u32temp = t->start_time + t->timeout;
	if(u32temp != t->crc) 
		NVIC_SystemReset();
	u32temp = GetCurrentTime();
	t->crc = t->start_time + t->timeout;
	if(u32temp >= t->start_time)
		u32temp1 = u32temp - t->start_time;
	else
		u32temp1 = (0xFFFFFFFF - t->start_time) + u32temp;
	if(u32temp1 >= t->timeout) return 0;
	return (t->timeout - u32temp1);
}

void TimerDelayms(uint32_t time)
{
	uint32_t start_time,current_time;
	start_time = GetCurrentTime();
	while(1)
	{
		current_time = GetCurrentTime();
		if(current_time >= start_time)
		{
			if((current_time - start_time) >= time) break;
		}
		else
		{
			if(((0xFFFFFFFF - start_time) + current_time) >= time) break;
		}
	}
}

void InitFindData(COMPARE_TYPE *cmpData,uint8_t *data)
{
	cmpData->buff = data;
	cmpData->index = 0;
	cmpData->len = strlen((char *)data);
}
uint8_t FindData(COMPARE_TYPE *cmpData,uint8_t c)
{
	if(cmpData->buff[cmpData->index] == c) cmpData->index++;
	else cmpData->index = 0;
	
	if(cmpData->index >= cmpData->len) return 0;
	
	return 0xff;
}

