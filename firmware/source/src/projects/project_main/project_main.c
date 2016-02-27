/*
 * File:		usb_main.c
 * Purpose:		Main process
 *
 */

/* Includes */
#include "start.h"
#include "common.h"
#include "uart.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "cdc.h"
#include "cdcuser.h"
#include "tick.h"
#include "dbg_cfg_app.h"
#include "system_config.h"
#include "spi.h"
#include "sst25.h"
#include "MKL25Z4.h"
#include "ff.h"
#include "diskio.h"
#include "project_common.h"
#include "audio.h"
#include "app.h"
#include "sysinit.h"
#include "lptmr.h"
#include "hw_config.h"
#include "modem.h"
#include "power_management.h"
#include "db.h"
#include "tracker.h"
#include "at_command_parser.h"
#include "dtmf_app.h"
#include "gprs_cmd_ctrl.h"
#include "accelerometer.h"
#include "gsm_gprs_tasks.h"
#include "sos_tasks.h"
#include "io_tasks.h"
#include "xl362_io.h"
#include "ambo_pedometer.h"
#include "pedometer_task.h"
#include "llwu.h"
#include "gps_task.h"

#define USER_FLASH_START		0x00002800
#define SMS_CHECK_TIMEOUT		300

void AppRun(void);

Timeout_Type tSMSCheckTimeout;
Timeout_Type tSleepTimeout;
extern uint8_t logEnable;
extern uint8_t incomingCall;
extern uint8_t powerOnLedFlash;
extern uint8_t ringing;
//extern uint8_t llwuInfoBuf[128];

uint8_t flagPwrOn = 0;
uint8_t flagPwrOff = 0;

uint8_t flagInitModem = 0,retryInitModemTimes = 0;
uint32_t mainCnt = 0;
uint32_t offset = 0;
uint8_t flagAutoAnswer = 0;

uint8_t ringingCount = 0;
uint8_t missCall = 0;

uint8_t pedometerEnable = 1;
uint8_t pedometerCntDownTime = 0;

void HardFault_Handler(void)
{
		NVIC_SystemReset();
}

void PIN_InterruptInit(void)
{

	// Configure CHG_DIS
  PORTB_PCR0 = ( PORT_PCR_MUX(1) |
									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */ 
                   PORT_PCR_PFE_MASK);
	// Configure ACCL_INT
  PORTC_PCR1 = ( PORT_PCR_MUX(1) |
									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */
                   PORT_PCR_PE_MASK |
                   PORT_PCR_PFE_MASK |
                   PORT_PCR_PS_MASK);
	// Configure PWR_KEY
  PORTC_PCR5 = ( PORT_PCR_MUX(1) |
									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */
                   PORT_PCR_PE_MASK |
                   PORT_PCR_PFE_MASK |
                   PORT_PCR_PS_MASK);
	
	// Configure SOS_KEY
  PORTC_PCR6 = ( PORT_PCR_MUX(1) |
									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */
                   PORT_PCR_PE_MASK |
                   PORT_PCR_PFE_MASK |
                   PORT_PCR_PS_MASK);
	
	// Configure GSM_RI
  PORTD_PCR4 = ( PORT_PCR_MUX(1) |
									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */
                   PORT_PCR_PE_MASK |
                   PORT_PCR_PFE_MASK |
                   PORT_PCR_PS_MASK);
	
	// Configure GPS_PULSE
//  PORTD_PCR6 = ( PORT_PCR_MUX(1) |
//									 PORT_PCR_IRQC(0x0A) | /* IRQ Falling edge */
//                   PORT_PCR_PE_MASK |
//                   PORT_PCR_PFE_MASK |
//                   PORT_PCR_PS_MASK);
	llwu_configure(0x0000, LLWU_PIN_FALLING, 0x1); //lpwtmr
	llwu_configure(0x0020, LLWU_PIN_FALLING, 0x1);
	llwu_configure(0x0040, LLWU_PIN_FALLING, 0x1);
	llwu_configure(0x0200, LLWU_PIN_FALLING, 0x1);
	llwu_configure(0x0400, LLWU_PIN_FALLING, 0x1);
	llwu_configure(0x4000, LLWU_PIN_FALLING, 0x1);
//	llwu_configure(0x8000, LLWU_PIN_FALLING, 0x1);
	NVIC_EnableIRQ(LLW_IRQn);
	NVIC_SetPriority(PORTD_IRQn, (1<< 0) - 1);
	NVIC_EnableIRQ(PORTD_IRQn);
}



void PORTD_IRQHandler (void)
{
	//GSM Ring PIN
	if(PORTD_ISFR == (1<<4))
	{
		//DIS_AU_CMU_EN_SET;
		PORTD_PCR4 |= PORT_PCR_ISF_MASK;
		AudioEnable();
		ringingFlag = 1;
		if (VoiceCallTask_GetPhase() == INCOMING_CALL)
		{
			ringingCount = 0;
			ringing = 1;
		}
	}
	//GPS_Pulse
	if(PORTD_ISFR == (1<<6))
	{
		PORTD_PCR6 |= PORT_PCR_ISF_MASK;
	}
}
#ifdef DEVICE_TESTING
extern uint32_t timer2Cnt;
#endif


void SysPwrOff(void)
{
	while(1)
	{
		SetLowPowerModes(LPW_LEVEL_3);
		SetLowPowerModes(LPW_NONE);
		if(llwuSource & LLWU_MWUF0)//LPWTMR
		{
			llwuSource &= ~LLWU_MWUF0;
			if(powerKeyHold3Sec)
			{
			PIN_InterruptInit();
			ledControlEnable = 1;
			USB_Init();                               // USB Initialization
			USB_Connect(TRUE);                        // USB Connect
			TPM0_Init(mcg_clk_hz,1000);
			powerKeyHold3Sec = 0;
			powerOnLedFlash = 1;			
			flagPwrOn = 1;
			flagPwrOff = 0;
			AudioStopAll();
			AudioPlayFile("1_1",TOP_INSERT);				// power on
			pwrStatus.value = POWER_ON;
			pwrStatus.oldValue = POWER_OFF;
				return;
			}
		}
	}
}

void SysSleep(void)
{
	uint32_t oldPedometer;
	uint32_t llpwWkTimes = 0;
	GSM_RTS_PIN_SET; /*Sleep module GSM*/
//	AudioStopAll();
//	AudioPlayFile("5_5", TOP_INSERT);
//	DelayMs(500);
	AudioStopAll();
	AUDIO_EN_CLR;
	SetLowPowerModes(LPW_NOMAL);
	PIN_InterruptInit();
	oldPedometer = Pedometer.stepCount;
	pedometerEnable = 0;
	while(1)
	{
		TPM0_DeInit();
		SetLowPowerModes(LPW_LEVEL_3);
		SetLowPowerModes(LPW_NONE);
		if(llwuSource & LLWU_WUF5) //CHG_DIS
		{
			llwuSource &= ~LLWU_WUF5;
			LLWU_F1 |= LLWU_F1_WUF5_MASK;	// write one to clear the flag
			break;
		}
		if(llwuSource & LLWU_WUF6)//ACCL_INT
		{
			llwuSource &= ~LLWU_WUF6;
			LLWU_F1 |= LLWU_F1_WUF6_MASK;	// write one to clear the flag
			pedometerEnable = 1;
			pedometerCntDownTime = 40; //10 second
		}
		else if(ACCEL_IN == 0 && pedometerEnable == 0)
		{
			pedometerEnable = 1;
			pedometerCntDownTime = 40; //10 second
		}
		if(pedometerCntDownTime)
		{
			//LED_BLUE_PIN_CLR;
			pedometerCntDownTime--;
		}
		else
		{
			//LED_BLUE_PIN_SET;
			pedometerEnable = 0;
		}
		
		if(llwuSource & LLWU_WUF9 || PWR_KEY_IN == 0)//PWR_KEY
		{
			InitTimeout(&tSleepTimeout,TIME_SEC(5));
			llwuSource &= ~LLWU_WUF9;
			LLWU_F2 |= LLWU_F2_WUF9_MASK;   // write one to clear the flag
			break;
		}
		if(llwuSource & LLWU_WUF10 || SOS_KEY_IN == 0)//SOS_KEY
		{
			InitTimeout(&tSleepTimeout,TIME_SEC(5));
			llwuSource &= ~LLWU_WUF10;
			LLWU_F2 |= LLWU_F2_WUF10_MASK;   // write one to clear the flag
			break;
		}
		if(llwuSource & LLWU_WUF14)//GSM_RI
		{
			llwuSource &= ~LLWU_WUF14;
			LLWU_F2 |= LLWU_F2_WUF14_MASK;   // write one to clear the flag
		//	DIS_AU_CMU_EN_SET;
			AudioEnable();
			ringingFlag = 1;
			if (VoiceCallTask_GetPhase() == INCOMING_CALL)
			{
				ringingCount = 0;
				ringing = 1;
			}
			break;
		}
//		if(llwuSource & LLWU_WUF15)//GPS_PULSE
//		{
//			llwuSource &= ~LLWU_WUF15;
//			LLWU_F2 |= LLWU_F2_WUF15_MASK;   // write one to clear the flag
//			if(GpsTask_GetPhase() == GPS_WAITING_FIX)
//				GpsTask_SetPhase(GPS_GET_LOCATION_PHASE);
//			break;
//		}
		if(llwuSource & LLWU_MWUF0)//LPWTMR
		{
			llwuSource &= ~LLWU_MWUF0;
			llpwWkTimes++;
			if(llpwWkTimes % 4 == 0)
			{
				TrackerTask();
				
				if(!((CHG_EN_IN == 0) &&
					(smsNewMessageFlag == 0) &&
					(VoiceCallTask_GetPhase() == NO_CALL) &&
					(SmsTask_GetPhase() == SMS_IDLE) &&
					(GprsTask_GetPhase() == TASK_SYS_SLEEP) &&
					(SosTask_IsAlarmActivated() == 0) && 
					(GpsTask_GetPhase() == GPS_IDLE)))
				{
					break;
				}
				
				if(Pedometer.updateReadyFlag)
				{
					Pedometer.updateReadyFlag = 0;
					//break;
				}
			}
			if(llpwWkTimes % 1200 == 0)
			{
				
				GSM_RTS_PIN_CLR; /*Wakeup module GSM*/ 
				DelayMs(100);
				MODEM_SendCommand(modemOk,modemError,1000,1,"AT+CBC\r"); //Batery check status)
				MODEM_SendCommand(modemOk,modemError,500,0,"AT+CCLK?\r"); //get rtc
				DelayMs(100);
				GSM_RTS_PIN_SET; /*Sleep module GSM*/
				
					// power off device if battery percent < 3%
				if (batteryPercent < 3)
				{
					break;
				}
			}
		}
	}
//	uart0_init(UART0_BASE_PTR,uart0_clk_khz,9600);
//	uart_init(UART1_BASE_PTR,periph_clk_khz,2400);
//	TICK_Init(1,mcg_clk_hz);
	TPM0_Init(mcg_clk_hz,1000);
//	SST25_Init();
//	AUDIO_Init();
	if(CHG_EN_IN)
	{
		USB_Init();
		USB_Connect(TRUE);                        // USB Connect
	}
}

/********************************************************************/
int main (void)
{
	uint32_t i;
	#ifdef DEVICE_TESTING
	uint8_t op_mode;
	#endif
	CFG_Load();
	
	#ifdef CMSIS  // If we are conforming to CMSIS, we need to call start here
	start();
	#endif
	SIM_SCGC5 |= (	SIM_SCGC5_PORTA_MASK
										| SIM_SCGC5_PORTB_MASK
										| SIM_SCGC5_PORTC_MASK
										| SIM_SCGC5_PORTD_MASK
										| SIM_SCGC5_PORTE_MASK );
	
	LED_RED_PIN_SET_OUTPUT;
	LED_BLUE_PIN_SET_OUTPUT;
	LED_GREEN_PIN_SET_OUTPUT;
	// turn all leds off
	LED_RED_PIN_SET;
	LED_GREEN_PIN_SET;
	LED_BLUE_PIN_SET;
	
	SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
	SIM_SOPT2 |= SIM_SOPT2_UART0SRC(0); // select the UART0 clock source

	SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
	SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1); // select the TPM clock source
	
	#ifdef DEVICE_TESTING
	PWR_EN_INIT;
	DIS_AU_CMU_EN_INIT;
	PWR_EN_SET;
	uart_init(UART1_BASE_PTR,periph_clk_khz,115200);
	PedometerTaskInit();
	PIN_InterruptInit();
	TICK_Init(1,mcg_clk_hz);
	TPM0_Init(mcg_clk_hz,1000);
	SST25_Init();
	AUDIO_Init();
	AudioPlayFile("1_1",TOP_INSERT);
	LPTMR_init(1000,LPTMR_USE_LPOCLK);
//	uart_putstr(UART1_BASE_PTR,"GT1022\r\n");
//	uart_putstr(UART1_BASE_PTR,"thienhaiblue\r\n");
//	sprintf((char *)mainBuf,"mcg_clk_khz:%d\r\n",mcg_clk_khz);
//	uart_putstr(UART1_BASE_PTR,mainBuf);
//	sprintf((char *)mainBuf,"core_clk_khz:%d\r\n",core_clk_khz);
//	uart_putstr(UART1_BASE_PTR,mainBuf);
//	sprintf((char *)mainBuf,"periph_clk_khz:%d\r\n",periph_clk_khz);
//	uart_putstr(UART1_BASE_PTR,mainBuf);
//	sprintf((char *)mainBuf,"pll_clk_khz:%d\r\n",pll_clk_khz);
//	uart_putstr(UART1_BASE_PTR,mainBuf);
	DelayMs(3000);
	//SST25_Write(0,"thienhaiblue",12);
//	SST25_Read(0,mainBuf,12);
		op_mode = what_mcg_mode();
		if (op_mode ==1) 
				DbgCfgPrintf("in BLPI mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==2) 
				DbgCfgPrintf(" in FBI mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==3) 
				DbgCfgPrintf(" in FEI mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==4) 
				DbgCfgPrintf(" in FEE mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==5) 
				DbgCfgPrintf(" in FBE mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==6) 
				DbgCfgPrintf(" in BLPE mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==7) 
				DbgCfgPrintf(" in PBE mode now at %d Hz\r\n",mcg_clk_hz );
		if (op_mode ==8) 
				DbgCfgPrintf(" in PEE mode now at %d Hz\r\n",mcg_clk_hz );
	while(1)
	{
		
	//	SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
	//	SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
		LED_RED_PIN_SET;
	//	TICK_DeInit();
		TPM0_DeInit();
		SetLowPowerModes(LPW_LEVEL_3);
		SetLowPowerModes(LPW_NOMAL);
		LED_RED_PIN_CLR;
		//SetLowPowerModes(LPW_NONE);
		
		op_mode = what_mcg_mode();
	if (op_mode ==1) 
			DbgCfgPrintf("in BLPI mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==2) 
			DbgCfgPrintf(" in FBI mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==3) 
			DbgCfgPrintf(" in FEI mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==4) 
			DbgCfgPrintf(" in FEE mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==5) 
			DbgCfgPrintf(" in FBE mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==6) 
			DbgCfgPrintf(" in BLPE mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==7) 
			DbgCfgPrintf(" in PBE mode now at %d Hz\r\n",mcg_clk_hz );
	if (op_mode ==8) 
			DbgCfgPrintf(" in PEE mode now at %d Hz\r\n",mcg_clk_hz );
		
		
		TICK_Init(1,mcg_clk_hz);
	//	TPM0_Init(mcg_clk_hz,1000);
		//SST25_Init();
		//AUDIO_Init();
		//AudioPlayFile("1_1",TOP_INSERT);
		
//		
//		SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
//		SIM_SOPT2 |= SIM_SOPT2_UART0SRC(0); // select the UART0 clock source
//		
//		SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
//		SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1); // select the TPM clock source
		
//		mcg_clk_khz = mcg_clk_hz / 1000;
//		core_clk_khz = mcg_clk_khz;
//		periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
//		uart0_clk_khz = ((4000000 ) / 1000); // UART0 clock frequency will equal fast irc
		
//		uart_init(UART1_BASE_PTR,periph_clk_khz,115200);
//		uart_putstr(UART1_BASE_PTR,llwuInfoBuf);
		
//		sprintf((char *)mainBuf,"mcg_clk_khz:%d\r\n",mcg_clk_khz);
//		uart_putstr(UART1_BASE_PTR,mainBuf);
//		sprintf((char *)mainBuf,"core_clk_khz:%d\r\n",core_clk_khz);
//		uart_putstr(UART1_BASE_PTR,mainBuf);
//		sprintf((char *)mainBuf,"periph_clk_khz:%d\r\n",periph_clk_khz);
//		uart_putstr(UART1_BASE_PTR,mainBuf);
//		sprintf((char *)mainBuf,"pll_clk_khz:%d\r\n",pll_clk_khz);
//		uart_putstr(UART1_BASE_PTR,mainBuf);
//		sprintf((char *)mainBuf,"%d:%d:%d,%d,%d:%d:%02X:%d\r\n",mainCnt++,timer2Cnt,xl362.x,xl362.y,xl362.z,devicePosition,xl362.status,xl362.fifo_samples);
//		uart_putstr(UART1_BASE_PTR,mainBuf);
//		if(mainCnt % 4 ==0)
//			PedometerTask();
	//	sprintf((char *)mainBuf,"%d:%d:%d,%d,%d:%d:%02X:%d\r\n",mainCnt++,timer2Cnt,xl362.x,xl362.y,xl362.z,devicePosition,xl362.status,xl362.fifo_samples);
		//uart_putstr(UART1_BASE_PTR,mainBuf);
		
//		if(Pedometer.updateReadyFlag)
//		{
//			Pedometer.updateReadyFlag = 0;
//		//	sprintf((char *)mainBuf,"Pedometer.counter=%d\r\n",Pedometer.lastStepCount);
//			//uart_putstr(UART1_BASE_PTR,mainBuf);
//		}
		
		
		//DelayMs(3000);
	}
	#endif
	
	// turn all leds off
	LED_RED_PIN_SET;
	LED_GREEN_PIN_SET;
	LED_BLUE_PIN_SET;

	PWR_KEY_INIT;
	SOS_KEY_INIT;
	CHGB_STATUS_INIT;
	CHG_EN_PIN_SET_INPUT;
	PWR_EN_INIT;
	PWR_EN_SET;
	GPS_EN_INIT;
	DIS_AU_CMU_EN_INIT;
// 	GSM_DTR_PIN_SET_OUTPUT;
// 	GSM_DTR_PIN_SET; 
	
	SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
	SIM_SOPT2 |= SIM_SOPT2_UART0SRC(0); // select the UART0 clock source

	SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
	SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1); // select the TPM clock source
	
	GPSInit();
	PedometerTaskInit();
	//LOW POWER TIMER WAKEUP INIT
	llwu_configure(0x0000, LLWU_PIN_FALLING, 0x1); //lpwtmr
	NVIC_EnableIRQ(LLW_IRQn);
	//Tick init
	TICK_Init(1,mcg_clk_hz);
	//GPS uart0 init
	uart0_init(UART0_BASE_PTR,uart0_clk_khz,9600);
	//GSM uart1 Init
	uart_init(UART1_BASE_PTR,periph_clk_khz,115200);
	/// External Flash memory init
	SST25_Init();
	//Database init
	DB_FlashMemInit();
	//Audio Init
	AUDIO_Init();
	//Tracking init
	TrackerInit();
	//low power timer init
	LPTMR_init(250,LPTMR_USE_LPOCLK);
	Keys_Init();
	key.pwr.lastTime = 0;
	key.pwr.time = 0;
	key.pwr.status = KEY_UP;
	key.pwr.lastStatus = KEY_UP;
	if(pwrStatus.value == POWER_OFF)
	{
		//PWR_EN_INIT;
		//PWR_EN_CLR;
		while(pwrStatus.value == POWER_OFF)
		{
			if(CHG_EN_IN)
			{
				i = 3;
				GSM_RTS_PIN_SET_OUTPUT;
				GSM_RTS_PIN_CLR;
				DelayMs(200);
				while(i--)
				{
						if(MODEM_Init()==0)
							break;
				}
				GSM_RTS_PIN_SET;
				i = 30;
				while(CHG_EN_IN)
				{
					i++;
					if(i >= 30)//12.5s
					{
						i = 0;
						GSM_RTS_PIN_CLR;
						DelayMs(200);
						if(MODEM_SendCommand(modemOk,modemError,1000,0,"AT+CBC\r")) //Batery check status)
						{
							if(MODEM_Init())
								i = 20;
						}
						else
							MODEM_SendCommand(modemOk,modemError,500,0,"AT+CCLK?\r"); //get rtc
						GSM_RTS_PIN_SET;
					}
					SetLowPowerModes(LPW_LEVEL_3);
					SetLowPowerModes(LPW_NONE);
					if(powerKeyHold3Sec)
					{
						PIN_InterruptInit();
						ledControlEnable = 1;
						USB_Init();                               // USB Initialization
						USB_Connect(TRUE);                        // USB Connect
						TPM0_Init(mcg_clk_hz,1000);
						powerKeyHold3Sec = 0;
						powerOnLedFlash = 1;			
						flagPwrOn = 1;
						flagPwrOff = 0;
						AudioStopAll();
						AudioPlayFile("1_1",TOP_INSERT);				// power on
						pwrStatus.value = POWER_ON;
						pwrStatus.oldValue = POWER_OFF;
						break;
					}
				}
			}
			else
			{
				while(MODEM_PowerOff());
				SysPwrOff();
			}
		}
	}
	else
	{
		PIN_InterruptInit();
		ledControlEnable = 1;
		USB_Init();                               // USB Initialization
		USB_Connect(TRUE);                        // USB Connect
		TPM0_Init(mcg_clk_hz,1000);
		pwrStatus.value = POWER_ON;
		Keys_Init();
	}
	
	//PWR_EN_INIT;
//	PWR_EN_SET;
	
	sendFirstPacket = 1;
	//Task Init
	GpsTask_Init();
	GsmTask_Init();
	GprsTask_Init();
	SosTask_Init();
	VoiceCallTask_Init();
	SmsTask_Init();
	
	InitTimeout(&tSleepTimeout,TIME_SEC(5));
	
	AppRun();
	NVIC_SystemReset();
	while(1);
}

//Application
void AppRun(void)
{
	uint32_t temp = 0;
	logEnable = 3;
	GprsTask_StartTask(SEND_POWER_CMD_TASK);
	while(1)
	{
		// GSM process
		ModemCmd_Task();
		if((incomingCall == 0) && (inCalling == 0) && ModemCmdTask_IsIdle())
		{
			Gsm_Task();
		}
		
		Sos_Task();
		Io_Task();
		VoiceCall_Task();
		Sms_Task();
		Gprs_Task();
		if (SosTask_GetPhase() != SOS_END)
		{
			continue;
		}
		
		if(Pedometer.updateReadyFlag)
		{
			Pedometer.updateReadyFlag = 0;
			//AudioPlayFile("6_1", TOP_INSERT);
		}
		// Check incomming SMS
		if(
			 (GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK) &&
			 (SosTask_GetPhase() == SOS_END) &&
			 (SosTask_IsAlarmActivated() == 0) &&
			 (VoiceCallTask_GetPhase() == NO_CALL) &&
			 (SmsTask_GetPhase() == SMS_IDLE) &&
			 (ModemCmdTask_IsIdle())
		)
		{
			SetLowPowerModes(LPW_NOMAL);
			if(flagSaveTime)
			{
				flagSaveTime = 0;
				MODEM_SendCommand(modemOk,modemError,1000,1,"AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d+00\"\r",sysTime.year % 100,sysTime.month,sysTime.mday,sysTime.hour,sysTime.min,sysTime.sec);
			}
			if((smsNewMessageFlag || (CheckTimeout(&tSMSCheckTimeout) == TIMEOUT)))
			{
				InitTimeout(&tSMSCheckTimeout, TIME_SEC(SMS_CHECK_TIMEOUT));
				
				SMS_Manage(mainBuf,sizeof(mainBuf));
				
				if(smsNewMessageFlag)
				{
					smsNewMessageFlag = 0;
				}
			}
		}
		if(AudioStopCheck() &&
			(VoiceCallTask_GetPhase() == NO_CALL) &&
			(SosTask_IsAlarmActivated() == 0) &&
			(ringingDetectFlag == 0) && 
			(ringingFlag == 0) &&
			(smsNewMessageFlag == 0) &&
			(flagSaveTime == 0)
		)
		{
			AUDIO_EN_CLR;
		}
		pedometerEnable = 1;
		//System Sleep
		if(
			(VoiceCallTask_GetPhase() == NO_CALL) &&
			(SmsTask_GetPhase() == SMS_IDLE) &&
			(GprsTask_GetPhase() == TASK_SYS_SLEEP) &&
			(SosTask_IsAlarmActivated() == 0) &&
			(AudioStopCheck()) &&
			(PWR_KEY_IN) && 
			(smsNewMessageFlag == 0) &&
			(SOS_KEY_IN) &&
			(CheckTimeout(&tSleepTimeout) == TIMEOUT))
		{
			GSM_RTS_PIN_SET; //GSM sleep
			DIS_AU_CMU_EN_CLR;
			if(pwrStatus.value == POWER_OFF)
			{
				NVIC_SystemReset();
			}
			if((CHG_EN_IN == 0))
			{
				if((GpsTask_GetPhase() == GPS_IDLE))
					SysSleep();
			}
		}
		else
		{
			GSM_RTS_PIN_CLR;
			SetLowPowerModes(LPW_NOMAL);
		}
	}
}
