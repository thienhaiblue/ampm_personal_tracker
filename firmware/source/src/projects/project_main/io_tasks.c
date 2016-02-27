#include "project_common.h"
#include "hw_config.h"
#include "at_command_parser.h"
#include "audio.h"
#include "modem.h"
#include "dtmf_app.h"
#include "tracker.h"
#include "gsm_gprs_tasks.h"
#include "sos_tasks.h"
#include "io_tasks.h"
#include "gps_task.h"

#define	SMS_CHECK_TIMEOUT			300
#define SMS_PHONE_LENGTH			16
#define SMS_MAX_NUMBER				6
#define SMS_DATA_MAX					160
#define SMS_SEND_TIMEOUT			10000 	// 10 sec
#define SMS_RESEND_CNT_MAX		2 			// resend 3 time

typedef struct {
	uint8_t 	offset;
	uint32_t 	timeout;
	uint8_t 	tryNum[SMS_MAX_NUMBER];
  uint8_t 	number[SMS_MAX_NUMBER][SMS_PHONE_LENGTH]; 	//maximun 6 number
	uint8_t 	data[SMS_DATA_MAX]; 												//  maximun 160 character
} SMS_DATA_TYPE;

extern uint8_t powerOn;
extern uint8_t powerOnLedFlash;
extern uint8_t incomingCall;
extern uint8_t getIncomingNumber;
extern uint8_t farHangUp;
extern uint8_t missCall;
extern uint8_t loneWorkerSos;
extern uint8_t noMovementSos;
extern uint8_t manDownSos;
extern uint8_t loneWorkerAlarmActiveFlag;
extern uint8_t noMovementAlarmActiveFlag;
extern uint8_t manDownAlarmActiveFlag;
extern uint16_t extendedErrorCode;
extern SOS_ACTION sosAction;
extern uint8_t callerPhoneNo[PHONE_NO_LENGTH];

extern uint8_t sendCallMeSms;
extern uint8_t addressData[110];
extern uint8_t sosStartTime[9];
extern uint8_t sosEndTime[9];
extern uint8_t sosStartLocation[22];
extern uint8_t sosEndLocation[22];
extern SOS_ACTION sosActionSms;
extern uint8_t sendSosAddressSms;
extern uint8_t sendSosGGLinkSms;
extern uint8_t sendSosEndAddressSms;
extern uint8_t sendSosEndGGLinkSms;
extern uint8_t sendSosCancelSms;
extern uint8_t sosSmsSent;

Timeout_Type tVoiceCallTaskTimeOut;
Timeout_Type tCallRingCheckTimeOut;
Timeout_Type tVoiceCallRetryTimeOut;
Timeout_Type tStartCallErrorTimeOut;
Timeout_Type tSmsTaskTimeOut;
Timeout_Type tSmsCheckTimeOut;
Timeout_Type tSmsWaitPauseTimeOut;
Timeout_Type tPowerOffTimeOut;
Timeout_Type tWaitCallActionTimeOut;

uint8_t initEmergencyCall = 0;
uint8_t startCallRetry = 0;
uint8_t answerCallRetry = 0;
uint8_t hangUpCallRetry = 0;
uint8_t ringing = 0;
uint8_t dialingPhoneNumber[16];
uint8_t incomingCallPhoneNo[16];
uint8_t requestInitDtmf = 0;
uint8_t dtmfInitialized = 0;
uint8_t dtmfSearchKey = 0;
uint8_t dtmfSearchKeyPressed = 0;
uint8_t startSendSmsRetry = 0;
uint8_t smsSent = 0;
uint8_t smsPauseRequest = 0;

uint8_t powerAndSosKeyHold3Sec = 0;
uint8_t powerAndSosKeyHold1Sec = 0;
uint8_t sosKeyPressed = 0;
uint8_t powerKeyHold3Sec = 0;
uint8_t powerKeyHold2Sec = 0;
uint8_t powerKeyHold1Sec = 0;
uint8_t powerKeyPressed = 0;

uint8_t promptRechargeBattery = 1;
uint8_t promptAlarmWarning = 0;
uint8_t playBeepSound = 0;

uint8_t startCall = 0;
uint8_t endCall = 0;
uint8_t answerCall = 0;

VOICE_CALL_PHASE_TYPE voiceCall_TaskPhase = NO_CALL;
SMS_PHASE_TYPE smsPhase = SMS_IDLE;
SMS_PHASE_TYPE smsPausedPhase;
SMS_DATA_TYPE smsSend;

void PlayBatteryStatus(void)
{
	if(batteryPercent < 25)
	{
		AudioPlayFile("2_2",BOT_INSERT);
	}
	else
	{
		AudioPlayFile("2_1",BOT_INSERT);
	}
}

void PlayAlarmWarningSound(uint8_t firstBeep)
{
	if ((VoiceCallTask_GetPhase() != IN_CALLING) && (SosTask_GetPhase() == SOS_END))
	{
		if (firstBeep)
		{
			promptAlarmWarning = 1;
		}
		else
		{
			playBeepSound = 1;
		}
	}
}

void Keys_Init(void)
{
	 powerAndSosKeyHold3Sec = 0;
	 powerAndSosKeyHold1Sec = 0;
	 sosKeyPressed = 0;
	 powerKeyHold3Sec = 0;
	 powerKeyHold2Sec = 0;
	 powerKeyHold1Sec = 0;
	 powerKeyPressed = 0;
}

void IoTask_Init()
{
	Keys_Init();
	promptAlarmWarning = 0;
	playBeepSound = 0;
}

uint8_t Io_Task(void)
{
	// power on/off the device if user press Power more than 3 seconds
	if ((powerKeyHold3Sec == 1))
	{
		powerKeyHold3Sec = 0;
		if ((pwrStatus.value == POWER_OFF))
		{
			// power on procedure
			PWR_EN_SET;
			
			batteryPercent = 100;
			GsmTask_Init();
			GprsTask_Init();
			SosTask_Init();
			IoTask_Init();
			VoiceCallTask_Init();
			
			powerOnLedFlash = 1;			
			flagPwrOn = 1;
			flagPwrOff = 0;
			AudioStopAll();
			AudioPlayFile("1_1",TOP_INSERT);				// power on
			pwrStatus.value = POWER_ON;
			GSM_RTS_PIN_CLR;
			DelayMs(2000);
			GprsTask_StartTask(SEND_POWER_CMD_TASK);
			USB_Init();                               // USB Initialization
			USB_Connect(TRUE);                        // USB Connect
		}
		else
		{
			// power off procedure
			flagPwrOff = 1;
			flagPwrOn = 0;
			AudioStopAll();
			AudioPlayFile("1_2",BOT_INSERT);				// power off
			pwrStatus.value = POWER_OFF;
			USB_Connect(FALSE);                        // USB Connect
			GprsTask_StartTask(SEND_POWER_CMD_TASK);
			InitTimeout(&tPowerOffTimeOut, TIME_SEC(30));
		}
	}
	
	// power off device if battery percent < 3%
	if (batteryPercent < 3)
	{
		if (pwrStatus.value == POWER_ON)
		{
			// power off procedure
			flagPwrOff = 1;
			flagPwrOn = 0;
			AudioStopAll();
			AudioPlayFile("1_2",BOT_INSERT);				// power off
			pwrStatus.value = POWER_OFF;
			GprsTask_StartTask(SEND_POWER_CMD_TASK);
			InitTimeout(&tPowerOffTimeOut, TIME_SEC(30));
		}
	}
	
	if (pwrStatus.value == POWER_OFF)
	{
		if (((GprsTask_GetPhase() == TASK_SYS_SLEEP) && (flagPwrOff == 0)) || (CheckTimeout(&tPowerOffTimeOut) == TIMEOUT))
		{
			PWR_EN_CLR;
		}
		return 0;
	}
	
	// power button is pressed
	if (powerKeyPressed)
	{
		powerKeyPressed = 0;
		promptAlarmWarning = 0;
		playBeepSound = 0;
		if (SosTask_IsAlarmActivated())
		{
			SosTask_Reset();
		}
		// if SOS is in place, skip it and other power key functions
		if (SosTask_GetPhase() != SOS_END)
		{
			powerKeyHold3Sec = 0;
			powerKeyHold2Sec = 0;
			powerKeyHold1Sec = 0;
			SosTask_End();
		}
		// hang up in calling, incoming call
		VoiceCallTask_EndCall();
	}
	
	if ((VoiceCallTask_GetPhase() != IN_CALLING) && (SosTask_GetPhase() == SOS_END))
	{			
		if (sosKeyPressed)
		{
			sosKeyPressed = 0;
			if (VoiceCallTask_GetPhase() != INCOMING_CALL && (ringingDetectFlag == 0) && (ringingFlag == 0))
			{
				// start emergency call
				promptAlarmWarning = 0;
				playBeepSound = 0;
				sosAction = SOS_KEY_ON;
				SosTask_Start();
			}
			else
			{
				VoiceCallTask_AnswerCall();
			}
		}
		// process SOS emergency
		else if (initEmergencyCall)
		{
			initEmergencyCall = 0;
			GpsTask_EnableGps();
			if (loneWorkerSos)
			{
				loneWorkerSos = 0;
				sosAction = SOS_LONE_WORKER_ON;
			}
			else if (noMovementSos)
			{
				noMovementSos = 0;
				sosAction = SOS_NO_MOVEMENT_ON;
			}
			else if (manDownSos)
			{
				manDownSos = 0;
				sosAction = SOS_MAN_DOWN_ON;
			}
			promptAlarmWarning = 0;
			playBeepSound = 0;
			SosTask_Start();
		}
		else if (promptAlarmWarning)
		{
			promptAlarmWarning = 0;
			// wake up the device & prompt SOS check
			AudioStopAll();
			AudioPlayFile("5_1", TOP_INSERT);				// Are you OK? Press test button
		}
		else if (playBeepSound)
		{
			playBeepSound = 0;
			AudioStopAll();
			AudioPlayFile("5_5", TOP_INSERT);					// beep beep
		}
		// prompt firmware version
		else if (powerAndSosKeyHold3Sec)
		{
			powerAndSosKeyHold3Sec = 0;
			PlayVersion(FIRMWARE_VERSION, BOT_INSERT);
		}
		// announce Lone Worker State
		else if (powerAndSosKeyHold1Sec)
		{
			powerAndSosKeyHold1Sec = 0;
			if (sysCfg.featureSet & FEATURE_LONE_WORKER_SET)
			{
				AudioPlayFile("5_3", BOT_INSERT);
			}
			else
			{
				AudioPlayFile("5_4", BOT_INSERT);
			}
		}
		// power button is hold for 2 sec
		// --> prompt system status
		else if (powerKeyHold2Sec)
		{
			powerKeyHold2Sec = 0;			
			if (batteryPercent < 25)
			{
				AudioPlayFile("2_2", BOT_INSERT);				// recharge battery soon
			}
			else
			{
				AudioPlayFile("2_1", BOT_INSERT);				// battery ok
			}
			if (flagGsmStatus == GSM_NO_REG_STATUS)
			{
				AudioPlayFile("2_4", BOT_INSERT);				// no service coverage
			}
			else
			{
				AudioPlayFile("2_7", BOT_INSERT);				// service coverage ok
			}
			if (nmeaInfo.fix < 3)
			{
				AudioPlayFile("2_5", BOT_INSERT);				// no current gps fix
			}
			else
			{
				AudioPlayFile("2_6", BOT_INSERT);				// gps ok
			}
		}
		// power button hold for 1 sec
		// --> reset all alarm warning
		else if (powerKeyHold1Sec)
		{
			powerKeyHold1Sec = 0;
			loneWorkerSos = 0;
			noMovementSos = 0;
			manDownSos = 0;
			SosTask_Reset();
		}
		// prompt GPS fixed
		else if(gpsPromptFlag)
		{
			gpsPromptFlag = 0;
			AudioPlayFile("2_6", BOT_INSERT);	// GPS ok
		}
		else if (batteryPercent > 30)
		{
			promptRechargeBattery = 1;
		}
		else if (batteryPercent < 25)
		{
			if (promptRechargeBattery)
			{
				promptRechargeBattery = 0;
				AudioPlayFile("2_2", BOT_INSERT);	// recharge battery soon
			}
		}
	}
	
	return 0;
}

void VoiceCallTask_Init(void)
{
	ringing = 0;
	incomingCall = 0;
	farHangUp = 0;
	busyFlag = 0;
	noAnswerFlag = 0;
	callUnsuccess = 0;
	
	startCallRetry = 0;
	answerCallRetry = 0;
	hangUpCallRetry = 0;
	requestInitDtmf = 0;
	dtmfInitialized = 0;
	dtmfSearchKey = 0;
	dtmfSearchKeyPressed = 0;
	voiceCall_TaskPhase = NO_CALL;
}

void VoiceCallTask_InitDtmf(void)
{
	if (sysCfg.featureSet & FEATURE_DTMF_ON)
	{
		if (dtmfInitialized == 0)
		{
			DTMF_Init();
			dtmfInitialized = 1;
		}
	}
}

void VoiceCallTask_DeInitDtmf(void)
{
	DTMF_DeInit();
	dtmfInitialized = 0;
}

uint8_t GetDtmfKey()
{
	uint8_t result = 0;
	uint8_t c;
	
	if (dtmfInitialized)
	{
		if (RINGBUF_Get(&DTMF_ringBuff, &c) == 0)
		{
			result = c;
		}
	}
	
	return result;
}

uint8_t VoiceCallTask_IsDtmfKeyPressed()
{
	return dtmfSearchKeyPressed;
}

void VoiceCallTask_StartCall(char* phoneNo, uint8_t dtmfKey)
{
	if (VoiceCallTask_IsIdle())
 	{
		if ((phoneNo != NULL) && (phoneNo[0] != 0))
		{
			strcpy((char*)dialingPhoneNumber, phoneNo);
			startCallRetry = 0;
			if (dtmfKey != 0)
			{
				requestInitDtmf = 1;
				dtmfSearchKey = dtmfKey;
				dtmfSearchKeyPressed = 0;
			}
			else
			{
				requestInitDtmf = 0;
				dtmfSearchKey = 0;
				dtmfSearchKeyPressed = 0;
			}
			startCall = 1;
			endCall = 0;
			answerCall = 0;
			extendedErrorCode = 0;
			VoiceCallTask_ClearEndCallStatus();
			InitTimeout(&tWaitCallActionTimeOut, TIME_SEC(30));
		}
	}
}

void VoiceCallTask_AnswerCall(void)
{
	if (voiceCall_TaskPhase == INCOMING_CALL)
	{
		answerCallRetry = 0;
		answerCall = 1;
		startCall = 0;
		endCall = 0;
		extendedErrorCode = 0;
		InitTimeout(&tWaitCallActionTimeOut, TIME_SEC(30));
	}
}

void VoiceCallTask_EndCall(void)
{
	if ((voiceCall_TaskPhase == INCOMING_CALL) || (voiceCall_TaskPhase == IN_CALLING) ||
			(voiceCall_TaskPhase == WAIT_START_CALL) || (voiceCall_TaskPhase == WAIT_ANSWER_CALL))
	{
		hangUpCallRetry = 0;
		endCall = 1;
		startCall = 0;
		answerCall = 0;
		InitTimeout(&tWaitCallActionTimeOut, TIME_SEC(30));
	}
	else if ((voiceCall_TaskPhase == SETUP_ANSWER_CALL) || (voiceCall_TaskPhase == ANSWER_CALL))
	{
		hangUpCallRetry = 0;
		endCall = 0;
		startCall = 0;
		answerCall = 0;
		GprsTask_Stop();
		voiceCall_TaskPhase = HANG_UP_CALL;
	}
	else
	{
		InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
		voiceCall_TaskPhase = END_CALL;
	}
}

uint8_t VoiceCallTask_GetCallerPhoneNo(void)
{
	if (getIncomingNumber)
	{
		getIncomingNumber = 0;
		return 1;
	}
	
	return 0;
}

uint8_t VoiceCallTask_IsModemBusy(void)
{
	if ((voiceCall_TaskPhase == INCOMING_CALL) || (voiceCall_TaskPhase == IN_CALLING) ||
			(voiceCall_TaskPhase == WAIT_START_CALL) || (voiceCall_TaskPhase == WAIT_ANSWER_CALL))
	{
		return 1;
	}
	
	return 0;
}

VOICE_CALL_PHASE_TYPE VoiceCallTask_GetPhase(void)
{
	return voiceCall_TaskPhase;
}

void VoiceCallTask_ClearEndCallStatus(void)
{
	switch (voiceCall_TaskPhase)
	{
		case END_CALL:
		case MISS_CALL:
		case BUSY_CALL:
		case NO_ANSWER_CALL:
		case UNSUCCESS_CALL:
			voiceCall_TaskPhase = NO_CALL;
			break;
	}
}

void VoiceCallTask_ClearCallAction(void)
{
	startCall = 0;
	endCall = 0;
	answerCall = 0;
	VoiceCallTask_ClearEndCallStatus();
}

uint8_t VoiceCallTask_IsIdle(void)
{
	switch (voiceCall_TaskPhase)
	{
		case NO_CALL:
		case END_CALL:
		case MISS_CALL:
		case BUSY_CALL:
		case NO_ANSWER_CALL:
		case UNSUCCESS_CALL:
			return 1;
	}
	
	return 0;
}

uint8_t VoiceCall_Task(void)
{
	switch (voiceCall_TaskPhase)
	{
		case NO_CALL:
			DIS_AU_CMU_EN_CLR;
			if (incomingCall)
			{
				GprsTask_Stop();
				if (SosTask_IsAlarmActivated() || ((SosTask_GetPhase() != SOS_END) && (SosTask_IsWaititngForCall() == 0)))
				{
//					voiceCall_TaskPhase = INCOMING_CALL;
//					VoiceCallTask_EndCall();
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
				else
				{
					incomingCall = 0;
					InitTimeout(&tCallRingCheckTimeOut, TIME_SEC(10));
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(60));
					voiceCall_TaskPhase = INCOMING_CALL;
				}
			}
			else if (startCall)
			{
				GprsTask_Stop();
				if (ModemCmdTask_IsIdle())
				{
					startCall = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(5));
					voiceCall_TaskPhase = SETUP_START_CALL;
				}
				else if (CheckTimeout(&tWaitCallActionTimeOut) == TIMEOUT)
				{
					startCall = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = UNSUCCESS_CALL;
				}
				else
				{
					COMM_Putc(0x1B);
					DelayMs(100);
				}
			}
			break;
			
		case SETUP_START_CALL:
			SmsTask_Pause();
			if (VoiceSetup() == 0)
			{
				startCallRetry = 0;
				voiceCall_TaskPhase = START_CALL;
			}
			else if (CheckTimeout(&tVoiceCallTaskTimeOut))
			{
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = UNSUCCESS_CALL;
			}
			else
			{
				if (startCallRetry++ >= 3)
				{
					startCallRetry = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = UNSUCCESS_CALL;
				}
			}
			break;
			
		case START_CALL:
			AudioStopAll();
			COMM_Putc(0x1A);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "ATD%s;\r", dialingPhoneNumber);
			InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(3));
			voiceCall_TaskPhase = WAIT_START_CALL;
			break;
			
		case WAIT_START_CALL:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (endCall)
				{
					GprsTask_Stop();
					endCall = 0;
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
				else
				{
					incomingCall = 0;
					if (requestInitDtmf)
					{
						VoiceCallTask_InitDtmf();
					}
					InitTimeout(&tStartCallErrorTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = IN_CALLING;
				}
			}
			else if ((CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (endCall)
				{
					endCall = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = END_CALL;
				}
				else
				{
					if (startCallRetry++ < 3)
					{
						voiceCall_TaskPhase = START_CALL;
					}
					else
					{
						startCallRetry = 0;
						InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
						voiceCall_TaskPhase = UNSUCCESS_CALL;
					}
				}
			}
			break;
			
		case INCOMING_CALL:
			incomingCall = 0;
			if (missCall)
			{
				missCall = 0;
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = MISS_CALL;
				break;
			}
			// if alarm is activated --> end call
			else if (SosTask_IsAlarmActivated() || ((SosTask_GetPhase() != SOS_END) && (SosTask_IsWaititngForCall() == 0)))
			{
				//VoiceCallTask_EndCall();
				voiceCall_TaskPhase = HANG_UP_CALL;
			}
			else if (answerCall)
			{
				GprsTask_Stop();
				if (ModemCmdTask_IsIdle())
				{
					answerCall = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(5));
					voiceCall_TaskPhase = SETUP_ANSWER_CALL;
				}
				else if (CheckTimeout(&tWaitCallActionTimeOut) == TIMEOUT)
				{
					answerCall = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = UNSUCCESS_CALL;
				}
				else
				{
					COMM_Putc(0x1B);
					DelayMs(100);
				}
			}
			else if (endCall)
			{
				GprsTask_Stop();
				if (ModemCmdTask_IsIdle())
				{
					endCall = 0;
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
				else
				{
					COMM_Putc(0x1B);
					DelayMs(100);
				}
			}
			else if ((sysCfg.featureSet & FEATURE_AUTO_ANSWER) && (SosTask_GetPhase() == SOS_END))
			{
				if (VoiceCallTask_GetCallerPhoneNo() && (incomingCallPhoneNo[0] != 0))
				{
					memset(callerPhoneNo, 0, sizeof(callerPhoneNo));
					memcpy(callerPhoneNo, incomingCallPhoneNo, PHONE_NO_LENGTH);
					callerPhoneNo[PHONE_NO_LENGTH - 1] = 0;
					if ((ComparePhoneNumber((char*)sysCfg.userList[0].phoneNo, (char*)callerPhoneNo) == SAME_NUMBER) ||
							(ComparePhoneNumber((char*)sysCfg.userList[1].phoneNo, (char*)callerPhoneNo) == SAME_NUMBER))
					{
						VoiceCallTask_AnswerCall();
					}
				}
			}
			else if (CheckTimeout(&tCallRingCheckTimeOut) == TIMEOUT)
			{
				if (ringing)
				{
					ringing = 0;
					InitTimeout(&tCallRingCheckTimeOut, TIME_SEC(10));
				}
				else
				{
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = MISS_CALL;
				}
			}
			else if (CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT)
			{
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = MISS_CALL;
			}
			else
			{
				SmsTask_Pause();
			}
			break;
			
		case SETUP_ANSWER_CALL:
			SmsTask_Pause();
			if (VoiceSetup() == 0)
			{
				answerCallRetry = 0;
				voiceCall_TaskPhase = ANSWER_CALL;
			}
			else if (CheckTimeout(&tVoiceCallTaskTimeOut))
			{
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = END_CALL;
			}
			else
			{
				if (answerCallRetry++ >= 3)
				{
					answerCallRetry = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = END_CALL;
				}
			}
			break;
			
		case ANSWER_CALL:
			AudioStopAll();
			COMM_Putc(0x1A);
			if(ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "ATA\r") == 0)
			{
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(3));
				voiceCall_TaskPhase = WAIT_ANSWER_CALL;
			}
			break;
			
		case WAIT_ANSWER_CALL:
			if(missCall)
			{
				missCall = 0;
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = MISS_CALL;
			}
			else if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (endCall)
				{
					GprsTask_Stop();
					endCall = 0;
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
				else 
				{
					incomingCall = 0;
					voiceCall_TaskPhase = IN_CALLING;
				}
			}
			else if ((CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				endCall = 0;
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = UNSUCCESS_CALL;
			}
			break;
			
		case IN_CALLING:
			DIS_AU_CMU_EN_SET;
			if (endCall)
			{
				GprsTask_Stop();
				if (ModemCmdTask_IsIdle())
				{
					endCall = 0;
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
			}
			else if (farHangUp)
			{
				farHangUp = 0;
				callUnsuccess = 0;
				extendedErrorCode = 0;
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CEER\r");
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_MS(500));
				voiceCall_TaskPhase = CHECK_END_CALL;
			}
			else if (busyFlag)
			{
				busyFlag = 0;
				AudioStopAll();
				AudioPlayFile("6_9", TOP_INSERT);		// the line is busy
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = BUSY_CALL;
			}
			else if (noAnswerFlag)
			{
				noAnswerFlag = 0;
				AudioStopAll();
				AudioPlayFile("6_3", TOP_INSERT);		// there is no answer
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = NO_ANSWER_CALL;
			}
			else if (callUnsuccess)
			{
				callUnsuccess = 0;
				AudioStopAll();
				AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessfull
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = UNSUCCESS_CALL;
			}
			else if (SosTask_GetPhase() == SOS_END)
			{
				SosTask_Reset();
			}
			if (dtmfInitialized)
			{
				if ((dtmfSearchKeyPressed == 0) && (dtmfSearchKey != 0))
				{
					if (GetDtmfKey() == dtmfSearchKey)
					{
						dtmfSearchKeyPressed = 1;
					}
				}
			}
			break;
			
		case HANG_UP_CALL:
			SmsTask_Pause();
			DIS_AU_CMU_EN_CLR;
			COMM_Putc(0x1A);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "ATH\r");
			InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(3));
			voiceCall_TaskPhase = WAIT_HANG_UP_CALL;
			break;
		
		case WAIT_HANG_UP_CALL:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				incomingCall = 0;
				if ((SosTask_IsAlarmActivated() == 0) && (SosTask_GetPhase() == SOS_END) && (SosTask_IsCancelCall() == 0))
				{
					AudioStopAll();
					AudioPlayFile("3_3", TOP_INSERT);		// hanging up
					PlayBatteryStatus();
				}
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(3));
				voiceCall_TaskPhase = ENSURE_HANG_UP_CALL;
			}
			else if ((CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (hangUpCallRetry++ < 5)
				{
					voiceCall_TaskPhase = HANG_UP_CALL;
				}
				else
				{
					hangUpCallRetry = 0;
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = END_CALL;
				}
			}
			break;
			
		case ENSURE_HANG_UP_CALL:
			if (CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT)
			{
				InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
				voiceCall_TaskPhase = END_CALL;
			}
			break;
			
		case CHECK_END_CALL:
			if ((ModemCmdTask_GetPhase() == MODEM_CMD_OK) || (CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT))
			{
				incomingCall = 0;
				if (callUnsuccess)
				{
					callUnsuccess = 0;
					AudioStopAll();
					if ((extendedErrorCode == 290) && (CheckTimeout(&tStartCallErrorTimeOut) != TIMEOUT))
					{
						if (startCallRetry++ < 3)
						{
							voiceCall_TaskPhase = START_CALL;
						}
						else
						{
							startCallRetry = 0;
							AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessfull
							InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
							voiceCall_TaskPhase = UNSUCCESS_CALL;
						}
					}
					else
					{
						AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessfull
						InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
						voiceCall_TaskPhase = UNSUCCESS_CALL;
					}
				}
				else
				{
					if (SosTask_IsAlarmActivated() == 0)
					{
						AudioStopAll();
						AudioPlayFile("3_3", TOP_INSERT);		// hanging up
						PlayBatteryStatus();
					}
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(10));
					voiceCall_TaskPhase = END_CALL;
				}
			}
			break;
			
		case END_CALL:
		case MISS_CALL:
		case BUSY_CALL:
		case NO_ANSWER_CALL:
		case UNSUCCESS_CALL:
			DIS_AU_CMU_EN_CLR;
			farHangUp = 0;
			busyFlag = 0;
			noAnswerFlag = 0;
			callUnsuccess = 0;
			if (incomingCall)
			{
				GprsTask_Stop();
				if (SosTask_IsAlarmActivated() || ((SosTask_GetPhase() != SOS_END) && (SosTask_IsWaititngForCall() == 0)))
				{
					voiceCall_TaskPhase = INCOMING_CALL;
					VoiceCallTask_EndCall();
				}
				else
				{
					incomingCall = 0;
					InitTimeout(&tCallRingCheckTimeOut, TIME_SEC(10));
					InitTimeout(&tVoiceCallTaskTimeOut, TIME_SEC(60));
					voiceCall_TaskPhase = INCOMING_CALL;
				}
			}
			else if (CheckTimeout(&tVoiceCallTaskTimeOut) == TIMEOUT)
			{
				voiceCall_TaskPhase = NO_CALL;
			}
			break;
	}
	
	return 0;
}

void SmsTask_Init(void)
{
	SmsTask_ClearAllSms();
	sprintf((char*)addressData, "No address available");
	sprintf((char*)sosStartTime, "??:??:??");
	sosStartTime[8] = 0;
	sprintf((char*)sosEndTime, "??:??:??");
	sosEndTime[8] = 0;
}

void SmsTask_ClearAllSms(void)
{
	uint8_t i;
	
	for (i = 0; i < SMS_MAX_NUMBER; i++)
	{
		memset(smsSend.number[i], NULL, SMS_PHONE_LENGTH);
		smsSend.tryNum[i] = 0;
	}
	smsSend.timeout = TICK_Get();
	smsSend.offset = 0;
	if (smsPhase == SMS_WAIT_MODEM_ANSWER)
	{
		COMM_Putc(0x1B); 					// Send ESC
	}
	smsSent = 0;
	smsPhase = SMS_IDLE;
}

void SmsTask_AddPhoneNo(uint8_t *phoneNo)
{
	uint8_t i;
	
	if ((phoneNo != NULL) && (phoneNo[0] != 0))
	{
		for (i = 0; i < SMS_MAX_NUMBER; i++)
		{
			if (smsSend.number[i][0] == 0)
			{
				strcpy((char*)smsSend.number[i], (char*)phoneNo);
				smsSend.number[i][SMS_PHONE_LENGTH - 1] = 0;
				break;
			}
		}
	}
}

void SmsTask_AddAllContactPhoneNo()
{
	uint8_t i;
	
	for (i = 0; i < SMS_MAX_NUMBER; i++)
	{
		smsSend.number[i][0] = 0;
		if (sysCfg.userList[i].phoneNo[0] != 0)
		{
			strcpy((char*)smsSend.number[i], (char*)sysCfg.userList[i].phoneNo);
			smsSend.number[i][SMS_PHONE_LENGTH - 1] = 0;
		}
	}
}

void SmsTask_SetMessage(uint8_t *message)
{
	uint8_t msgLen;
	
	if ((message != NULL) && (message[0] != 0))
	{
		msgLen = strlen((char*)message);
		if (msgLen > SMS_DATA_MAX)
		{
			msgLen = SMS_DATA_MAX;
		}
		memcpy(smsSend.data, message, msgLen);
		smsSend.data[msgLen] = 0;
	}
}

void SmsTask_SetSosAddressMessage(SOS_ACTION sosAction)
{
	int len;
	uint8_t buf[32];
	len = 0;
	smsSend.data[0] = 0;
	switch (sosAction)
	{
		case SOS_KEY_ON:
			len = sprintf((char*)buf, "SOS key pressed");
			break;
		case SOS_LONE_WORKER_ON:
			len = sprintf((char*)buf, "Lone worker SOS");
			break;
		case SOS_NO_MOVEMENT_ON:
			len = sprintf((char*)buf, "No movement SOS");
			break;
		case SOS_MAN_DOWN_ON:
			len = sprintf((char*)buf, "Man down SOS");
			break;
		case SOS_OFF:
			len = sprintf((char*)buf, "Emergency call is ended");
			break;
	}
	
	if (len > 0)
	{
		smsSend.data[len] = 0;
		if (sosAction != SOS_OFF)
		{
		//	len = sprintf((char*)smsSend.data, "%s at %s Address: %s.", (char*)smsSend.data, (char*)sosStartTime, (char*)addressData);
			len = sprintf((char*)smsSend.data, "Battery: %d%% %s at %s Google Maps link: https://maps.google.com/maps?hl=%s&q=%s", 
																			batteryPercent,(char*)buf, (char*)sosStartTime, "en", sosStartLocation);
		}
		else
		{
		//	len = sprintf((char*)smsSend.data, "%s at %s Address: %s.", (char*)smsSend.data, (char*)sosEndTime, (char*)addressData);
			len = sprintf((char*)smsSend.data, "Battery: %d%% %s at %s Google Maps link: https://maps.google.com/maps?hl=%s&q=%s", 
																			batteryPercent,(char*)buf, (char*)sosEndTime, "en", sosEndLocation);
		}
		smsSend.data[len] = 0;
	}
}

// https://maps.google.com/maps?hl=" + language + "&q=" + lat.ToString() + "," + lon.ToString()
void SmsTask_SetSosGGLinkMessage(char* location, int sosStart)
{
	smsSend.data[0] = 0;
	
	if (sosStart)
	{
		sprintf((char*)smsSend.data, "Battery: %d%%. SOS Google Maps link: https://maps.google.com/maps?hl=%s&q=%s", batteryPercent, "en", location);
	}
	else
	{
		sprintf((char*)smsSend.data, "Battery: %d%%. SOS end Google Maps link: https://maps.google.com/maps?hl=%s&q=%s", batteryPercent, "en", location);
	}
}

void SmsTask_SendSms(void)
{
	if (smsPhase == SMS_IDLE)
	{
		smsSend.offset = 0;
		smsSent = 0;
		smsPhase = SMS_CHECK;
	}
}

uint8_t SmsTask_IsIdle(void)
{
	if ((smsPhase == SMS_IDLE) || (smsPhase == SMS_PAUSED))
	{
		return 1;
	}
	
	return 0;
}

uint8_t SmsTask_IsModemBusy(void)
{
	if ((smsPhase == SMS_WAIT_MODEM_INIT) || (smsPhase == SMS_WAIT_MODEM_ANSWER) ||
			(smsPhase == SMS_WAIT_SEND))
	{
		return 1;
	}
	
	return 0;
}

uint8_t IoTask_IsModemBusy(void)
{
	if (VoiceCallTask_IsModemBusy() || SmsTask_IsModemBusy())
	{
		return 1;
	}
	
	return 0;
}

uint8_t SmsTask_IsSmsSent(void)
{
	if (smsSent)
	{
		smsSent = 0;
		return 1;
	}
	
	return 0;
}

SMS_PHASE_TYPE SmsTask_GetPhase(void)
{
	return smsPhase;
}

void SmsTask_Stop(void)
{
	if (SmsTask_IsModemBusy())
	{
		COMM_Putc(0x1B);				// send ESC;
	}
	smsPhase = SMS_IDLE;
}

void SmsTask_Pause(void)
{
	if ((smsPhase != SMS_IDLE) && (smsPhase != SMS_PAUSED))
	{
		InitTimeout(&tSmsWaitPauseTimeOut, TIME_SEC(10));
		if ((smsPhase == SMS_WAIT_MODEM_ANSWER) || (smsPhase == SMS_WAIT_SEND))
		{
			smsPauseRequest = 1;
			while (((smsPhase == SMS_WAIT_MODEM_ANSWER) || (smsPhase == SMS_WAIT_SEND)) &&
							(CheckTimeout(&tSmsWaitPauseTimeOut) != TIMEOUT))
			{
				COMM_Putc(0x1B);
				ModemCmd_Task();
				Sms_Task();
			}
			if (smsPhase != SMS_SENT)
			{
				smsPhase = SMS_CHECK;
			}
		}
		smsPausedPhase = smsPhase;
		smsPhase = SMS_PAUSED;
	}
}

uint8_t Sms_Task(void)
{
	uint8_t i;
	
	switch(smsPhase)
	{
		case SMS_CHECK:
			if(GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
			{
				for (i = 0; i <= 6; i++)
				{
					if((smsSend.number[smsSend.offset][0] != NULL) && (smsSend.data[0] != NULL))
					{
						startSendSmsRetry = 0;
						if (VoiceCallTask_GetPhase() == IN_CALLING)
						{
							InitTimeout(&tSmsTaskTimeOut, TIME_SEC(5));
						}
						else
						{
							InitTimeout(&tSmsTaskTimeOut, TIME_SEC(5));
						}
						smsPhase = SMS_DELAY_START_SEND;
					}
					else
					{
						smsSend.offset++;
						if (smsSend.offset >= SMS_MAX_NUMBER)
						{
							smsSend.offset = 0;
						}
					}
				}
				if (smsPhase != SMS_DELAY_START_SEND)
				{					
					// no number found
					smsPhase = SMS_IDLE;
				}
			}
			break;
			
		case SMS_DELAY_START_SEND:
			if (CheckTimeout(&tSmsTaskTimeOut) == TIMEOUT)
			{
				InitTimeout(&tSmsTaskTimeOut, TIME_SEC(10));
				smsPhase = SMS_START_SEND;
			}
			break;
			
		case SMS_START_SEND:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tSmsTaskTimeOut) == TIMEOUT))
			{
				if((smsSend.number[smsSend.offset][0] != NULL) && (smsSend.data[0] != NULL))
				{
					
					InitTimeout(&tSmsTaskTimeOut, TIME_SEC(1));
					smsPhase = SMS_WAIT_MODEM_INIT;
				}
				else
				{
					smsPhase = SMS_CHECK;
				}
			}
			else
			{
				COMM_Putc(0x1B);
				DelayMs(100);
			}
			break;
			
		case SMS_WAIT_MODEM_INIT:
			if (CheckTimeout(&tSmsTaskTimeOut) == TIMEOUT)
			{
				smsModemAccept = 0;
				COMM_Puts("AT+CMGS=\"");
				COMM_Puts(smsSend.number[smsSend.offset]);
				COMM_Puts("\"\r");
				InitTimeout(&tSmsTaskTimeOut, TIME_SEC(5));
				smsPhase = SMS_WAIT_MODEM_ANSWER;
			}
			break;
				
		case SMS_WAIT_MODEM_ANSWER:
			if (smsModemAccept)
			{
				smsModemAccept = 0;
				startSendSmsRetry = 0;
				COMM_Puts(smsSend.data);
				COMM_Putc(0x1A); 					// Send Ctl-Z
				smsSentFlag = 0;
				ModemCmdTask_SetWait(modemOk, modemError, 10000);
				InitTimeout(&tSmsCheckTimeOut, TIME_SEC(3));
				InitTimeout(&tSmsTaskTimeOut, TIME_SEC(10));
				smsPhase = SMS_WAIT_SEND;
			}
			else if (CheckTimeout(&tSmsTaskTimeOut) == TIMEOUT)
			{
				smsPhase = SMS_SEND_FAIL;
			}
			break;
		
// 		case SMS_SEND:
// 			
// 			break;
		
		case SMS_WAIT_SEND:
// 			if (smsSentFlag)
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				smsSentFlag = 0;
				smsPhase = SMS_SENT;
			}
			else if ((ModemCmdTask_GetPhase() == MODEM_CMD_ERROR) || (CheckTimeout(&tSmsTaskTimeOut) == TIMEOUT))
			{
				smsPhase = SMS_SEND_FAIL;
			}
			else if (CheckTimeout(&tSmsCheckTimeOut) == TIMEOUT)
			{
				InitTimeout(&tSmsCheckTimeOut, TIME_SEC(1));
				COMM_Putc(0x1A);
			}
			break;

		case SMS_SENT:
			memset(smsSend.number[smsSend.offset], NULL, SMS_PHONE_LENGTH);
			smsSend.tryNum[smsSend.offset] = 0;
			smsSend.offset++;
			if(smsSend.offset >= SMS_MAX_NUMBER)
			{
				smsSend.offset = 0;
			}
			smsSent = 1;
			if (((sendSosAddressSms == 0) && (sendSosGGLinkSms == 0)) || (SosTask_GetPhase() != SOS_END))
			{
				sosSmsSent = 1;
			}
			InitTimeout(&tSmsCheckTimeOut, TIME_SEC(2));
			smsPhase = SMS_FINISH_SEND;
			break;
		case SMS_FINISH_SEND:
			if (CheckTimeout(&tSmsCheckTimeOut) == TIMEOUT)
				smsPhase = SMS_CHECK;
		break;
		case SMS_SEND_FAIL:
			//COMM_Putc(0x1B); 				//Send ESC
			if ((VoiceCallTask_GetPhase() != IN_CALLING) && (smsPauseRequest == 0))
			{
				if(MODEM_SendCommand("+CREG: 0,1",modemError,1000,3,"AT+CREG?\r"))
				{
						GsmTask_Init();
				}
				MODEM_SendCommand(modemOk, modemError, 1000, 0, "AT+CMGF=1\r");
			}
			if (smsSend.tryNum[smsSend.offset]++ >= SMS_RESEND_CNT_MAX)
			{
				memset(smsSend.number[smsSend.offset], NULL, SMS_PHONE_LENGTH);
				smsSend.tryNum[smsSend.offset] = 0;
			}
			smsSend.offset++;
			if(smsSend.offset >= SMS_MAX_NUMBER)
			{
				smsSend.offset = 0;
			}
			smsPhase = SMS_CHECK;
			break;
			
		case SMS_IDLE:
			if ((GprsTask_GetTask() == NO_TASK) &&
					(VoiceCallTask_IsIdle() ||
					(VoiceCallTask_GetPhase() == IN_CALLING)))
			{
				if (sendCallMeSms)
				{
					// send "Call Me" SMS before sending address SMS
					sendCallMeSms = 0;
					SmsTask_ClearAllSms();
					SmsTask_AddAllContactPhoneNo();
					smsSend.data[0] = 0;
					if (sysCfg.callMeSms[0])
					{
						sprintf((char*)smsSend.data, (char*)sysCfg.callMeSms);
					}
					else
					{
						sprintf((char*)smsSend.data, DEFAULT_CALL_ME_SMS);
					}
					SmsTask_SendSms();
				}
				else
				{
					if (sendSosAddressSms)
					{
						if ((sysCfg.familyPalMode != MODE_FAMILY_VER2) || (SosTask_GetPhase() != SOS_INIT))
						{
							sendSosAddressSms = 0;
							SmsTask_ClearAllSms();
							SmsTask_AddAllContactPhoneNo();
							SmsTask_SetSosAddressMessage(sosActionSms);
							SmsTask_SendSms();
						}
					}
					else if (sendSosEndAddressSms)
					{
						sendSosEndAddressSms = 0;
						SmsTask_ClearAllSms();
						SmsTask_AddAllContactPhoneNo();
						SmsTask_SetSosAddressMessage(SOS_OFF);
						SmsTask_SendSms();
					}
					else if (sendSosCancelSms)
					{
						sendSosCancelSms = 0;
						SmsTask_ClearAllSms();
						SmsTask_AddAllContactPhoneNo();
						smsSend.data[0] = 0;
						if (sysCfg.sosCancelledSms[0])
						{
							sprintf((char*)smsSend.data, (char*)sysCfg.sosCancelledSms);
						}
						else
						{
							sprintf((char*)smsSend.data, DEFAULT_SOS_CANCELLED_SMS);
						}
						SmsTask_SendSms();
					}
					else if ((sendSosAddressSms == 0) && (sendSosGGLinkSms == 1))
					{
						sendSosGGLinkSms = 0;
						SmsTask_ClearAllSms();
						SmsTask_AddAllContactPhoneNo();
						SmsTask_SetSosGGLinkMessage((char*)sosStartLocation, 1);
						SmsTask_SendSms();
					}
					else if ((sendSosEndAddressSms == 0) && (sendSosEndGGLinkSms == 1))
					{
						sendSosEndGGLinkSms = 0;
						SmsTask_ClearAllSms();
						SmsTask_AddAllContactPhoneNo();
						SmsTask_SetSosGGLinkMessage((char*)sosEndLocation, 0);
						SmsTask_SendSms();
					}
				}
			}
			break;
			
		case SMS_PAUSED:
			smsPauseRequest = 0;
			if ((GprsTask_GetTask() == NO_TASK) &&
					(VoiceCallTask_IsIdle() ||
					(VoiceCallTask_GetPhase() == IN_CALLING)))
			{
				smsPhase = smsPausedPhase;
			}
			break;
	}
	
	return 0;
}
