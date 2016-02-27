#ifndef __RTC_H__
#define __RTC_H__
#include "stm32f10x.h"
#include <stdint.h>
#include "exti.h"
#include "system_config.h"
#include "hw_config.h"

#define WTD_MAIN_LOOP	0
#define WTD_TIMER0_LOOP	1
#define WTD_TIMER1_LOOP	2

#define WAKEUP_TIME2	30
#define WAKEUP_TIME1	120
#define WAKEUP_TIME		300
#define RTC_FREQUENCY 1000 //Hz
#define ALARM_FREQUENCY	10

#define MOVING_MODE	0
#define STOP_MODE	1


extern uint32_t rtcSecCnt;
extern uint32_t rtcCnt;
extern volatile uint32_t rtcTimeSec;
extern volatile uint8_t rtcTimeSecFlagUpdate;
extern uint32_t timeoutSleep;
extern uint32_t timeoutSleepStatus;
extern uint32_t timepowerOffDelay;
extern uint32_t batteryWarningCnt;
extern uint32_t powerWarningCnt;
extern uint32_t securityOkFlag;
extern uint32_t autoSecurityCnt;
extern uint8_t flagAccountCheck;


extern volatile uint32_t watchdogFeed[WATCHDOG_NUM];
extern uint32_t watchdogEnable[WATCHDOG_NUM];


void RTC_Init(void);
uint32_t RTC_GetCounter(void);
void RTC_SetAlarm(uint32_t AlarmValue);
#endif

