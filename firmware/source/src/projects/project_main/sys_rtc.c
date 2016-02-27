
/**
* \file
*         rtc driver
* \author
*         Nguyen Van Hai <blue@ambo.com.vn>
*/
#include "rtc.h"
#include "tick.h"
#include "stm32f10x_iwdg.h"
#include "sys_time.h"

#define CRL_CNF_Set      ((uint16_t)0x0010)      /* Configuration Flag Enable Mask */
#define CRL_CNF_Reset    ((uint16_t)0xFFEF)      /* Configuration Flag Disable Mask */
#define RTC_LSB_Mask     ((uint32_t)0x0000FFFF)  /* RTC LSB Mask */
#define PRLH_MSB_Mask    ((uint32_t)0x000F0000)  /* RTC Prescaler MSB Mask */


#define RTC_FLAG_RTOFF       ((uint16_t)0x0020)  /* RTC Operation OFF flag */
#define RTC_FLAG_RSF         ((uint16_t)0x0008)  /* Registers Synchronized flag */
#define RTC_FLAG_OW          ((uint16_t)0x0004)  /* Overflow flag */
#define RTC_FLAG_ALR         ((uint16_t)0x0002)  /* Alarm flag */
#define RTC_FLAG_SEC         ((uint16_t)0x0001)  /* Second flag */

#define MAIN_LOOP_TIME_MAX 120 //120sec
#define TIMER0_LOOP_TIME_MAX 60 //60sec
#define TIMER1_LOOP_TIME_MAX 60 //60sec




uint32_t timepowerOffDelay = 0,sec = 0;

volatile uint8_t rtcTimeSecFlagUpdate = 0;
volatile uint32_t rtcTimeSec = 0;
extern uint32_t gpsTimeSec;
extern uint32_t gsmTimeSec;
extern uint32_t rpSampleCount;
extern uint32_t serverSendDataFlag;
extern SYS_STATUS systemStatus;
extern uint32_t batteryPercent;
uint32_t rtcCnt = 0;
uint32_t rtcSecCnt = 0;
uint32_t timeoutSleep = 0;
uint32_t timeoutSleepStatus = MOVING_MODE;
uint32_t batteryWarningCnt = 0;
uint32_t powerWarningCnt = 0;
uint32_t securityOkFlag = 0;
uint32_t autoSecurityCnt = 0;
uint8_t flagAccountCheck = 0;



uint32_t watchdogEnable[WATCHDOG_NUM] = {0};
volatile uint32_t watchdogCnt[WATCHDOG_NUM] = {0};
volatile uint32_t watchdogFeed[WATCHDOG_NUM] = {0};
const uint32_t watchdogCntValueMax[WATCHDOG_NUM] = {MAIN_LOOP_TIME_MAX,TIMER0_LOOP_TIME_MAX,TIMER1_LOOP_TIME_MAX};

extern void MODEM_AgpsProcess(void);

void RTC_IRQHandler(void)
{
	RTC->CRL &= (uint16_t)RTC_FLAG_SEC;
	RTC->CRL &= (uint16_t)RTC_FLAG_ALR;
	sec++;
}

void RTCAlarm_IRQHandler(void)
{
	uint32_t RTC_counter,temp;
	RTC->CRL &= (uint16_t)RTC_FLAG_ALR;
	EXTI->PR = EXTI_Line17;
	RTC_counter = RTC_GetCounter();
	RTC_SetAlarm((RTC_counter + RTC_FREQUENCY/ALARM_FREQUENCY - 1));
	rtcCnt++;
	
	IWDG_ReloadCounter();
	
	//1 Sec interval
	if((rtcCnt % ALARM_FREQUENCY) == 0)
	{
		rtcSecCnt++;
		rtcTimeSec++;
		
		if((!flagAccountCheck) && (sysTime.hour == sysCfg.accountAlarmCheck) && (sysTime.min == 0) && (sysTime.sec == 0))
		 flagAccountCheck = 1;
		
		if((rtcTimeSec % 600) == 0)
			sysEventFlag |= FLAG_SEND_INFO;
		if(systemStatus != SYS_DEINIT)
		{
			temp = rtcTimeSec % WATCHDOG_NUM;
			if(watchdogEnable[temp])
			{
				//check watchdog value for funtion
				watchdogCnt[temp] += WATCHDOG_NUM;
				if(watchdogCnt[temp] >= watchdogCntValueMax[temp]) 
				{
					flagAccountCheck = 0;
				//	NVIC_SystemReset();
				}
				if(watchdogFeed[temp] == 0)
				{
					watchdogCnt[temp] = 0;
					watchdogFeed[temp] = 1;
				}
			}
			else	
			{
				watchdogCnt[temp] = 0;
			}
		}
		
		
		if(rtcTimeSecFlagUpdate == 1)
		{
			rtcTimeSecFlagUpdate = 0;
			rtcTimeSec = gpsTimeSec;
		}
		else if(rtcTimeSecFlagUpdate == 2)
		{
			rtcTimeSecFlagUpdate = 0;
			rtcTimeSec = gsmTimeSec;
		}
		
		accelerometerFlagResetTime++;
		if(batteryWarningCnt)
			batteryWarningCnt--;
		if(powerWarningCnt)
			powerWarningCnt--;
		MODEM_AgpsProcess();
		
		if(rtcSecCnt % 60 == 0)
			sysEventFlag |= REINIT_ACCL_FLAG;
		
		if(rtcSecCnt % 300 == 0)
		{
			if(securityOkFlag)
				securityOkFlag = 0;
		}
		
		if((accelerometerFlagResetTime == 10))
		{
			if(GET_VIN_PIN)
			{
				LedSetStatus(1000,3000,LED_TURN_ON);
			}
			else
			{
				LedSetStatus(1000,1000,LED_TURN_ON);
			}
			accelerometerFlagCnt = 0;
		}
		
			//check auto security mode
		if(sysCfg.featureSet & FEATURE_AUTO_ENABLE_SECURIRY)
		{
			if(autoSecurityCnt)
			{
				autoSecurityCnt--;
			}
			else 
			{
				autoSecurityCnt = sysCfg.autoSecurityTime;
				securityOkFlag = 0;
				sysCfg.securityOn = 1;
				OUTPUT1_PIN_SET;
			}
		}
	}
	//IO Process an fillter
	IO_Control();
	//Run and Stop interval process
	if(timepowerOffDelay > RTC_counter)
		timepowerOffDelay = RTC_counter;
	if(GET_ACC_PIN == ACC_ON)
	{
			timepowerOffDelay = RTC_counter;
	}
	else if((RTC_counter - timepowerOffDelay)/RTC_FREQUENCY >= sysCfg.powerOffDelayTime)
	{
		timepowerOffDelay = RTC_counter;
		POWER_EN_PIN_CLR;
	}
	
	if(rpSampleCount > RTC_counter)
		rpSampleCount = RTC_counter;
	if(timeoutSleep  == 0)
	{
		if((RTC_counter - rpSampleCount)/RTC_FREQUENCY > sysCfg.stopReportInterval)
		{
			rpSampleCount = RTC_counter;
			serverSendDataFlag = 1;
		}
	}
	else
	{
		if((RTC_counter - rpSampleCount)/RTC_FREQUENCY > sysCfg.runReportInterval)
		{
			rpSampleCount = RTC_counter;
			serverSendDataFlag = 1;
		}
	}
	//Led process when MCU sleep
	if(systemStatus == SYS_DEINIT)
	{
		if((rtcCnt % (ALARM_FREQUENCY*10)) == 0)
		{
			LED_PIN_SET;
		}
		else
		{
			LED_PIN_CLR;
		}
	}
	//Process when have theft alarm
	if(sysEventFlag & THEFT_ALARM_FLAG)
	{
		sysEventFlag &= ~THEFT_ALARM_FLAG;
		OUTPUT1_PIN_SET;
	}
	//Check time for disable power
	if((ioStatus.din[3].bitOld) && 
		(powerWarningCnt == 0) && 
		(sysCfg.featureSet & FEATURE_REMOVE_POWER_WARNING) && 
		((RTC_counter - timepowerOffDelay)/RTC_FREQUENCY < sysCfg.powerOffDelayTime)
	)
	{
		powerWarningCnt = sysCfg.powerLowWarningPeriod;
		sysEventFlag |= POWER_WARNING_FLAG;
	}
	//check battery level
	if((batteryPercent <= sysCfg.batteryLowWarningLevel) && 
		(batteryWarningCnt == 0) && 
		(sysCfg.featureSet & FEATURE_LOW_BATTERY_WARNING)
	)
	{
		batteryWarningCnt = sysCfg.batteryLowWarningPeriod;
		sysEventFlag |= BATTERY_WARNING_FLAG;
	}
	
	sysEventFlag |=  RTC_ALARM_FLAG;	
}

void RTC_Init(void)
{
	
	IO_Init();
	autoSecurityCnt = sysCfg.autoSecurityTime;
	
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;                            // enable clock for Power interface
	PWR->CR      |= PWR_CR_DBP;                                   // enable access to RTC, BDC registers

	RCC->CSR |= RCC_CSR_LSION;                                  // enable LSI
	while ((RCC->CSR & RCC_CSR_LSIRDY) == 0);                   // Wait for LSERDY = 1 (LSI is ready)

	RCC->BDCR |= (RCC_BDCR_RTCSEL_1 | RCC_BDCR_RTCEN);             // set RTC clock source, enable RTC clock
	//  RTC->CRL   &= ~(1<<3);                                        // reset Registers Synchronized Flag
	//  while ((RTC->CRL & (1<<3)) == 0);                             // wait until registers are synchronized
	RTC->CRL  |=  RTC_CRL_CNF;                                    // set configuration mode
  RTC->PRLH  = 0;   // set prescaler load register high
  RTC->PRLL  = 40000/RTC_FREQUENCY - 1;// set prescaler load register low
  RTC->CNTH  = 0;                      // set counter high
  RTC->CNTL  = 0;                      // set counter low
  RTC->ALRH  = 0;                      // set alarm high
  RTC->ALRL  = 0;                      // set alarm low
	
	//RTC->CRH = RTC_CRH_ALRIE;                                       // enable Alarm RTC interrupts
  NVIC->ISER[1] |= (1 << (RTCAlarm_IRQn & 0x1F));            		// enable interrupt
	
	RTC->CRH =  RTC_CRH_ALRIE;
	NVIC->ISER[0] |= (1 << (RTC_IRQn & 0x1F));            		// enable interrupt
	
	RTC->CRL  &= ~RTC_CRL_CNF;                                    // reset configuration mode
  while ((RTC->CRL & RTC_CRL_RTOFF) == 0);                      // wait until write is finished
	//RTC_SetAlarm((60));
  //PWR->CR   &= ~PWR_CR_DBP;                                     // disable access to RTC registers
}	

uint32_t RTC_GetCounter(void)
{
	uint16_t tmp = 0;
  tmp = RTC->CNTL;
  return (((uint32_t)RTC->CNTH << 16 ) | tmp) ;
}

/**
  * @brief  Sets the RTC alarm value.
  * @param AlarmValue: RTC alarm new value.
  * @retval : None
  */
void RTC_SetAlarm(uint32_t AlarmValue)
{  
 /* Set the CNF flag to enter in the Configuration Mode */
  RTC->CRL |= CRL_CNF_Set;
  /* Set the ALARM MSB word */
  RTC->ALRH = AlarmValue >> 16;
  /* Set the ALARM LSB word */
  RTC->ALRL = (AlarmValue & 0x0000FFFF);
  /* Reset the CNF flag to exit from the Configuration Mode */
  RTC->CRL &= CRL_CNF_Reset;
}



