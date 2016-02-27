#include "audio.h"
#include "gsm_gprs_tasks.h"
#include "at_command_parser.h"
#include "dtmf_app.h"
#include "tracker.h"
#include "gprs_cmd_ctrl.h"
#include "io_tasks.h"
#include "sos_tasks.h"
#include "gps_task.h"
#include "hw_config.h"

#define SEND_SOS_DATA_TIME_OUT					60

#define MAX_CALLING_NUMBER_CALL_CENTER	2
#define MAX_SEND_SMS_NUMBER_CALL_CENTER	4
#define MAX_CALLING_NUMBER_FAMILY_VER1 	3
#define MAX_SEND_SMS_NUMBER_FAMILY_VER1 3
#define MAX_SEND_SMS_NUMBER_FAMILY_VER2 6
#define MAX_SOS_START_CALL_RETRY				6

extern uint8_t loneWorkerSos;
extern uint8_t noMovementSos;
extern uint8_t manDownSos;
extern uint8_t loneWorkerAlarmActiveFlag;
extern uint8_t noMovementAlarmActiveFlag;
extern uint8_t manDownAlarmActiveFlag;
extern uint8_t incomingCallPhoneNo[PHONE_NO_LENGTH];
extern uint8_t mainBuf1[256];
extern uint32_t sosDataLength;
extern uint8_t sosData[256];

Timeout_Type tSosTaskTimeOut;
Timeout_Type tPromptStandByTimeOut;
Timeout_Type tPromptCallBackTimeOut;
Timeout_Type tWaitForCallTimeOut;
Timeout_Type tWaitDtmfKeyPressTimeout;
Timeout_Type tDtmfPromptTimeout;
Timeout_Type tWaitCallAnswer;
Timeout_Type tWaitModemInitTimeout;

uint8_t sosStart = 0;
uint8_t sosSession = 0;
uint8_t loneWorkerSos = 0;
uint8_t noMovementSos = 0;
uint8_t manDownSos = 0;
uint8_t sosSuccess;
uint8_t sosStartCallRetry = 0;
uint8_t sosCancelCall = 0;
uint8_t sosSendSmsRetry = 0;

uint8_t callerPhoneNo[PHONE_NO_LENGTH];
uint8_t dtmfKeyPressed = 0;
uint8_t dtmfPromptRetry = 0;
uint8_t callingNumberIndex = 0;
uint8_t nextNumberIndex = 0;
uint8_t familyVer2Retry;

uint8_t sendCallMeSms;
uint8_t addressData[110];
uint8_t sosStartTime[9];
uint8_t sosEndTime[9];
uint8_t sosStartLocation[22];
uint8_t sosEndLocation[22];
SOS_ACTION sosActionSms = SOS_OFF;
uint8_t sendSosAddressSms = 0;
uint8_t sendSosGGLinkSms = 0;
uint8_t sendSosEndAddressSms = 0;
uint8_t sendSosEndGGLinkSms = 0;
uint8_t sendSosCancelSms = 0;
uint8_t sosSmsSent = 0;

SOS_PHASE_TYPE sosPhase = SOS_END;
SOS_ACTION sosAction = SOS_OFF;

SOS_PHASE_TYPE SosTask_GetPhase(void)
{
	return sosPhase;
}

void ResetAllAlarmWarning(void)
{
	loneWorkerAlarmActiveFlag = 0;
	loneWorkerWarningFlag = 0;
	flagLoneWorkerReset = 1;
	
	noMovementAlarmActiveFlag = 0;
	noMovementWarningFlag = 0;
	flagNoMovementReset = 1;
	
	manDownAlarmActiveFlag = 0;
	manDownWarningFlag = 0;
	flagManDownReset = 1;
}

void SosTask_Reset(void)
{
	loneWorkerSos = 0;
	noMovementSos = 0;
	manDownSos = 0;
	ResetAllAlarmWarning();
}

void SosTask_Init(void)
{
	nextNumberIndex = 0;
	sosPhase = SOS_END;
	SosTask_Reset();
}

void SosTask_InitPromptTimeOut(void)
{
	InitTimeout(&tPromptStandByTimeOut, TIME_SEC(30));
	InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
}

void SosTask_PromptForWaiting(void)
{
	if ((sysCfg.familyPalMode != MODE_FAMILY_VER2) && (CheckTimeout(&tPromptStandByTimeOut) == TIMEOUT))
	{
		AudioPlayFile("3_2", BOT_INSERT);				// please stand by
		InitTimeout(&tPromptStandByTimeOut, TIME_SEC(30));
	}
	else if ((sysCfg.familyPalMode == MODE_FAMILY_VER2) && (CheckTimeout(&tPromptCallBackTimeOut) == TIMEOUT))
	{
		AudioPlayFile("4_2", BOT_INSERT);							// please wait for call back
		InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
	}
}

void SosTask_Start(void)
{
	if (sosPhase == SOS_END)
	{
		ResetAllAlarmWarning();
		VoiceCallTask_ClearCallAction();
		sosStartCallRetry = 0;
		familyVer2Retry = 0;
		sosCancelCall = 0;
		nextNumberIndex = 0;
		sosSendSmsRetry = 0;
		callerPhoneNo[0] = 0;
		incomingCallPhoneNo[0] = 0;
		sosStart = 1;
		sosSmsSent = 0;
		sosSession = rtcTimeSec;
		sosSmsSent = 0;
		sendCallMeSms = 0;
		sendSosAddressSms = 0;
		sendSosGGLinkSms = 0;
		sendSosEndAddressSms = 0;
		sendSosEndGGLinkSms = 0;
		sendSosCancelSms = 0;
		sosDataLength = DSOS_Add((char*)sosData, sosAction, sosSession);
		sosPhase = SOS_CHECK;
	}
}

void SosTask_End(void)
{
	//uint8_t i;
	
	if (sosPhase != SOS_END)
	{
		GprsTask_Stop();
		if ((VoiceCallTask_GetPhase() != IN_CALLING) &&
				(VoiceCallTask_GetPhase() != WAIT_START_CALL) && (VoiceCallTask_GetPhase() != WAIT_ANSWER_CALL))
		{
			sosCancelCall = 1;
			AudioStopAll();
			AudioPlayFile("3_7", TOP_INSERT);					// sos is cancelled
			sosDataLength = DSOS_Add((char*)sosData, SOS_CANCELLED, sosSession);			// sos data has been sent --> send sos cancelled data
			sosAction = SOS_OFF;
			GprsTask_StartTask(SEND_SOS_DATA_TASK);
			if (sosSmsSent)
			{
				sosSmsSent = 0;
				sendCallMeSms = 0;
				sendSosAddressSms = 0;
				sendSosGGLinkSms = 0;
				sendSosEndAddressSms = 0;
				sendSosEndGGLinkSms = 0;
				sendSosCancelSms = 1;
			}
			SmsTask_Stop();
		}
// 		if ((sysCfg.familyPalMode == MODE_FAMILY_VER2) && (sosPhase == FAMILY_VER2_WAIT_CALLING))
// 		{
// 			SmsTask_ClearAllSms();
// 			for (i = 0; i < MAX_SEND_SMS_NUMBER_FAMILY_VER2; i++)
// 			{
// 				if(sysCfg.userList[i].phoneNo[0])
// 				{
// 					SmsTask_AddPhoneNo((uint8_t*)sysCfg.userList[i].phoneNo);
// 				}
// 			}
// 			if (sysCfg.sosCancelledSms[0])
// 			{
// 				sprintf((char*)mainBuf1, (char*)sysCfg.sosCancelledSms);
// 			}
// 			else
// 			{
// 				sprintf((char*)mainBuf1, DEFAULT_SOS_CANCELLED_SMS);
// 			}
// 			SmsTask_SetMessage(mainBuf1);
// 			SmsTask_SendSms();
// 		}
		//sosAction = SOS_OFF;
		ResetAllAlarmWarning();
		VoiceCallTask_ClearCallAction();
		sosPhase = SOS_END;
	}
}

uint8_t SosTask_IsCancelCall(void)
{
	if (sosCancelCall)
	{
		sosCancelCall = 0;
		return 1;
	}
	
	return 0;
}

uint8_t SosTask_IsAlarmActivated(void)
{
	if (loneWorkerAlarmActiveFlag || noMovementAlarmActiveFlag || manDownAlarmActiveFlag)
	{
		return 1;
	}
	
	return 0;
}

uint8_t SosTask_IsWaititngForCall(void)
{
	if (sysCfg.familyPalMode == MODE_FAMILY_VER2)
	{
		if ((sosPhase == FAMILY_VER2_WAIT_CALLING) ||
				(sosPhase == FAMILY_VER2_CHECK_PHONE_CALL) ||
				(sosPhase == FAMILY_VER2_WAIT_ANSWER_CALL))
		{
			return 1;
		}
	}
	
	return 0;
}

uint8_t ComparePhoneNumber(char* phone1, char* phone2)
{
	uint8_t i = strlen(phone1);
	uint8_t j = strlen(phone2);
	uint8_t l1;
	uint8_t l2;
	uint8_t minL;
	uint8_t count = 0;
	
	l1 = i;
	if (phone1[0] == '+')
	{
		if (i > 4)
		{
			l1 = i - 4;
		}
	}
	else if (phone1[0] == '0')
	{
		if (i > 1)
		{
			l1 = i - 1;
		}
	}
	
	l2 = j;
	if (phone2[0] == '+')
	{
		if (j > 4)
		{
			l2 = j - 4;
		}
	}
	else if (phone2[0] == '0')
	{
		if (j > 1)
		{
			l2 = j - 1;
		}
	}
	
	minL = l1;
	if (l2 < l1)
	{
		minL = l2;
	}
	
	while ((i != 0) && (j != 0))
	{
		i--;
		j--;
		if (phone1[i] == phone2[j])
		{
			count++;
		}
		else
		{
			break;
		}
	}
	
	if ((count >= PHONE_DIGIT_THRESHOLD) && (count >= minL))
	{
		return SAME_NUMBER;
	}
	
	return DIFFERENT_NUMBER;
}

void SosTask_SendSms(void)
{
	SmsTask_ClearAllSms();
	SmsTask_AddPhoneNo((uint8_t*)sysCfg.whiteCallerNo);
	SmsTask_SetMessage(sosData);
	SmsTask_SendSms();
	InitTimeout(&tPromptStandByTimeOut, TIME_SEC(30));
	InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
	InitTimeout(&tSosTaskTimeOut, TIME_SEC(SEND_SOS_DATA_TIME_OUT));
	sosPhase = SOS_WAIT_SEND_SMS;
}

uint8_t Sos_Task(void)
{
	uint32_t i;
	uint8_t res;
	
	if (sosPhase == SOS_END)
	{
		if (sosAction != SOS_OFF)
		{
			sosDataLength += DSOS_Add((char*)&sosData[sosDataLength], SOS_OFF, sosSession);
			sosAction = SOS_OFF;
			GprsTask_StartTask(SEND_SOS_DATA_TASK);
			sendSosEndAddressSms = 1;
			//sendSosEndGGLinkSms = 1;
		}
		sosCancelCall = 0;
	}
	else if (sosPhase == SOS_CHECK)
	{
		switch (sysCfg.familyPalMode)
		{
			case MODE_CALL_CENTER:
				callingNumberIndex = nextNumberIndex;
				while (1)
				{
					if (sysCfg.userList[callingNumberIndex].phoneNo[0])
					{
						nextNumberIndex = callingNumberIndex + 1;
						if (nextNumberIndex >= MAX_CALLING_NUMBER_CALL_CENTER)
						{
							nextNumberIndex = 0;
						}
						if (sosStart)
						{
							sosStart = 0;
							sosPhase = SOS_START;
						}
						else
						{
							sosPhase = SOS_INIT;
						}
						break;
					}
					else
					{
						callingNumberIndex++;
						if (callingNumberIndex >= MAX_CALLING_NUMBER_CALL_CENTER)
						{
							callingNumberIndex = 0;
						}
						if (callingNumberIndex == nextNumberIndex)
						{
							AudioStopAll();
							AudioPlayFile("3_6", TOP_INSERT); //SOS FAIL
							sosPhase = SOS_END;
							break;
						}
					}
				}
				break;
			case MODE_FAMILY_VER1:
				callingNumberIndex = nextNumberIndex;
				while (1)
				{
					if (sysCfg.userList[callingNumberIndex].phoneNo[0])
					{
						nextNumberIndex = callingNumberIndex + 1;
						if (nextNumberIndex >= MAX_CALLING_NUMBER_FAMILY_VER1)
						{
							nextNumberIndex = 0;
						}
						if (sosStart)
						{
							sosStart = 0;
							sosPhase = SOS_START;
						}
						else
						{
							sosPhase = SOS_INIT;
						}
						break;
					}
					else
					{
						callingNumberIndex++;
						if (callingNumberIndex >= MAX_CALLING_NUMBER_FAMILY_VER1)
						{
							callingNumberIndex = 0;
						}
						if (callingNumberIndex == nextNumberIndex)
						{
							AudioStopAll();
							AudioPlayFile("3_6", TOP_INSERT); //SOS FAIL
							sosPhase = SOS_END;
							break;
						}
					}
				}
				break;
			case MODE_FAMILY_VER2:
				res = 0;
				for(i = 0; i < MAX_SEND_SMS_NUMBER_FAMILY_VER2; i++)
				{
					if(sysCfg.userList[i].phoneNo[0])
					{
						res = 1;
						break;
					}
				}
				if(res)
				{
					if (sosStart)
					{
						sosStart = 0;
						sosPhase = SOS_START;
					}
					else
					{
						sosPhase = SOS_INIT;
					}
				}
				else
				{
					AudioStopAll();
					AudioPlayFile("3_6", TOP_INSERT); //SOS FAIL
					sosPhase = SOS_END;
				}
				break;
		}
	}
	else if (sosPhase == SOS_START)
	{
		AudioStopAll();
		AudioPlayFile("3_1", TOP_INSERT);								// emergency call is being placed
		if (sysCfg.familyPalMode != MODE_FAMILY_VER2)
		{
			AudioPlayFile("3_2", BOT_INSERT);							// please stand by
		}
		else
		{
			AudioPlayFile("4_2", BOT_INSERT);							// please wait for call back
		}
		if (GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
		{
			GpsTask_EnableGps();
			SmsTask_Stop();
			SosTask_InitPromptTimeOut();
			InitTimeout(&tSosTaskTimeOut, TIME_SEC(SEND_SOS_DATA_TIME_OUT));
			sosPhase = SOS_WAIT_SEND_DATA;
		}
		else
		{
			SosTask_InitPromptTimeOut();
			InitTimeout(&tWaitModemInitTimeout, TIME_SEC(60));
			GpsTask_EnableGps();
			sosPhase = SOS_WAIT_MODEM_INIT;
		}
	}
	else if (sosPhase == SOS_WAIT_MODEM_INIT)
	{
		if (GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
		{
			GprsTask_StartTask(SEND_SOS_DATA_TASK);
			SosTask_InitPromptTimeOut();
			InitTimeout(&tSosTaskTimeOut, TIME_SEC(SEND_SOS_DATA_TIME_OUT));
			sosPhase = SOS_WAIT_SEND_DATA;
		}
		else if (CheckTimeout(&tWaitModemInitTimeout) == TIMEOUT)
		{
			AudioStopAll();
			AudioPlayFile("3_6", TOP_INSERT); //SOS FAIL
			sosPhase = SOS_END;
		}
		else 
		{
			SosTask_PromptForWaiting();
		}
	}
	else if (sosPhase == SOS_WAIT_SEND_DATA)
	{		
			sendSosAddressSms = 1;
//			sendSosGGLinkSms = 1;
//			sosPhase = SOS_INIT;
			sosPhase = SOS_WAIT_SEND_SMS;
	}
	else if (sosPhase == SOS_WAIT_SEND_SMS)
	{
		if (SmsTask_GetPhase() == SMS_IDLE)
		{
			if (SmsTask_IsSmsSent())
			{
				sosDataLength = 0;
			}
			//sendSosAddressSms = 1;
			//sendSosGGLinkSms = 1;
			sosPhase = SOS_INIT;
		}
		else
		{
			SosTask_PromptForWaiting();
			if (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT)
			{
				SmsTask_Stop();
				COMM_Putc(0x1B);
				if (sosSendSmsRetry++ < 2)
				{
					SosTask_SendSms();
				}
				else
				{
					//sendSosAddressSms = 1;
					//sendSosGGLinkSms = 1;
					sosPhase = SOS_INIT;
				}
			}
		}
	}
	else
	{
		switch(sysCfg.familyPalMode)
		{
			case MODE_CALL_CENTER:
				switch (sosPhase)
				{
					case SOS_INIT:
						// wait 3 sec to finish voice prompt
						SosTask_InitPromptTimeOut();
						InitTimeout(&tSosTaskTimeOut, TIME_SEC(3));
						sosPhase = SOS_WAIT_INIT;
						break;
					
					case SOS_WAIT_INIT:
						if (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT)
						{
							if (SmsTask_IsIdle())
							{
								// place the call
								VoiceCallTask_StartCall((char*)sysCfg.userList[callingNumberIndex].phoneNo, 0);
								InitTimeout(&tSosTaskTimeOut, TIME_SEC(20));
								sosPhase = CALL_CENTER_WAIT_CALLING;
							}
							else
							{
								SosTask_PromptForWaiting();
							}
						}
						break;
					
					case CALL_CENTER_WAIT_CALLING:
						if (VoiceCallTask_GetPhase() == IN_CALLING)
						{
							sosStartCallRetry = 0;
							sosPhase = CALL_CENTER_IN_CALLING;
						}
						else if ((VoiceCallTask_GetPhase() == UNSUCCESS_CALL) || (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT))
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioStopAll();
							AudioPlayFile("3_6", TOP_INSERT); 	// SOS failed
							if (sosStartCallRetry++ < MAX_SOS_START_CALL_RETRY)
							{
								AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
								sosPhase = SOS_CHECK;
							}
							else
							{
								sosStartCallRetry = 0;
								sosPhase = CALL_CENTER_SOS_END;
							}
						}
						break;
						
					case CALL_CENTER_IN_CALLING:
						// wait for end call
						if (VoiceCallTask_GetPhase() == BUSY_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						else if (VoiceCallTask_GetPhase() == NO_ANSWER_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						else if (VoiceCallTask_GetPhase() == UNSUCCESS_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						else if ((VoiceCallTask_GetPhase() == END_CALL) || (VoiceCallTask_GetPhase() == NO_CALL))
						{
							sosPhase = CALL_CENTER_SOS_END;
						}
						break;
					
					case CALL_CENTER_SOS_END:
						// reset calling number index
						callingNumberIndex = 0;
						nextNumberIndex = 0;
						ResetAllAlarmWarning();
						VoiceCallTask_ClearCallAction();
						sosPhase = SOS_END;
						break;
				}
				break;
			
			case MODE_FAMILY_VER1:
				switch (sosPhase)
				{
					case SOS_INIT:
						// wait 3 sec to finish voice prompt
						SosTask_InitPromptTimeOut();
						InitTimeout(&tSosTaskTimeOut, TIME_SEC(3));
						sosPhase = SOS_WAIT_INIT;
						break;
					
					case SOS_WAIT_INIT:
						if (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT)
						{
							if (SmsTask_IsIdle())
							{
								// place the call
								dtmfKeyPressed = 0;
								dtmfPromptRetry = 0;
								VoiceCallTask_StartCall((char*)sysCfg.userList[callingNumberIndex].phoneNo, '#');
								InitTimeout(&tSosTaskTimeOut, TIME_SEC(20));
								sosPhase = FAMILY_VER1_WAIT_CALLING;
							}
							else
							{
								SosTask_PromptForWaiting();
							}
						}
						break;
					
					case FAMILY_VER1_WAIT_CALLING:
						if (VoiceCallTask_GetPhase() == IN_CALLING)
						{
							sosStartCallRetry = 0;
							InitTimeout(&tDtmfPromptTimeout, TIME_SEC(10));
							sosPhase = FAMILY_VER1_IN_CALLING;
						}
						else if ((VoiceCallTask_GetPhase() == UNSUCCESS_CALL) || (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT))
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioStopAll();
							AudioPlayFile("3_6", TOP_INSERT); 	// SOS failed
							if (sosStartCallRetry++ < MAX_SOS_START_CALL_RETRY)
							{
								AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
								sosPhase = SOS_CHECK;
							}
							else
							{
								sosStartCallRetry = 0;
								sosPhase = CALL_CENTER_SOS_END;
							}
						}
						break;
						
					case FAMILY_VER1_IN_CALLING:
						// wait for end call
						if (VoiceCallTask_GetPhase() == BUSY_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						else if (VoiceCallTask_GetPhase() == NO_ANSWER_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						else if (VoiceCallTask_GetPhase() == UNSUCCESS_CALL)
						{
							VoiceCallTask_ClearEndCallStatus();
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}							
						else if ((VoiceCallTask_GetPhase() == END_CALL) || (VoiceCallTask_GetPhase() == NO_CALL))
						{
							//VoiceCallTask_ClearEndCallStatus();
							if ((sysCfg.featureSet & FEATURE_DTMF_ON) && (dtmfKeyPressed == 0))
							{
								AudioStopAll();
								AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessful
								AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
								sosPhase = SOS_CHECK;
							}
							else
							{
								sosPhase = FAMILY_VER1_SOS_END;
							}
						}
						else
						{
							if ((sysCfg.featureSet & FEATURE_DTMF_ON) && (dtmfKeyPressed == 0))
							{
								if (VoiceCallTask_IsDtmfKeyPressed())
								{
									dtmfKeyPressed = 1;
								}
								else if (CheckTimeout(&tDtmfPromptTimeout) == TIMEOUT)
								{
									if (VoiceCallTask_GetPhase() == IN_CALLING) // check this in case retry calling --> no prompt
									{
										if (dtmfPromptRetry++ < 4)
										{
											DIS_AU_CMU_EN_SET;
											AudioPlayFile("3_5", BOT_INSERT);				// this is emergency call, please press the pound key now
											InitTimeout(&tDtmfPromptTimeout, TIME_SEC(15));
										}
										else
										{
											dtmfPromptRetry = 0;
											VoiceCallTask_EndCall();
											sosPhase = FAMILY_VER1_NOT_PRESS_KEY;
										}
									}
								}
							}
						}
						break;
					
					case FAMILY_VER1_NOT_PRESS_KEY:
						if ((VoiceCallTask_GetPhase() == END_CALL) || (VoiceCallTask_GetPhase() == NO_CALL))
						{
							AudioStopAll();
							AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessful
							AudioPlayFile("3_4", BOT_INSERT);		// hanging up and dialing next number
							sosPhase = SOS_CHECK;
						}
						break;
					
					case FAMILY_VER1_SOS_END:
						// reset calling number index
						callingNumberIndex = 0;
						nextNumberIndex = 0;
						ResetAllAlarmWarning();
						VoiceCallTask_DeInitDtmf();
						VoiceCallTask_ClearCallAction();
						sosPhase = SOS_END;
						break;
				}
				break;
			
			case MODE_FAMILY_VER2:
				switch(sosPhase)
				{
					case SOS_INIT:
// 						SosTask_InitPromptTimeOut();
// 						InitTimeout(&tSosTaskTimeOut, TIME_SEC(1));
// 						sosPhase = SOS_WAIT_INIT;
						sendCallMeSms = 1;
						InitTimeout(&tWaitForCallTimeOut,TIME_SEC(180));
						sosPhase = FAMILY_VER2_WAIT_CALLING;
						break;
					
// 					case SOS_WAIT_INIT:
// 						if (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT)
// 						{
// 							if (SmsTask_IsIdle())
// 							{
// 								SmsTask_ClearAllSms();
// 								sosPhase = FAMILY_VER2_SEND_SMS;
// 							}
// 							else
// 							{
// 								SosTask_PromptForWaiting();
// 							}
// 						}
// 						break;			
					
// 					case FAMILY_VER2_SEND_SMS:
// 						SmsTask_AddAllContactPhoneNo();
// 						if (sysCfg.callMeSms[0])
// 						{
// 							SmsTask_SetMessage(sysCfg.callMeSms);
// 						}
// 						else
// 						{
// 							SmsTask_SetMessage(DEFAULT_CALL_ME_SMS);
// 						}
// 						SmsTask_SendSms();
// 						//InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
// 						InitTimeout(&tWaitForCallTimeOut,TIME_SEC(180));
// 						sosPhase = FAMILY_VER2_WAIT_CALLING;
// 						break;
						
					case FAMILY_VER2_WAIT_CALLING:
						if (VoiceCallTask_GetPhase() == INCOMING_CALL)
						{
							InitTimeout(&tSosTaskTimeOut, TIME_SEC(20));
							sosPhase = FAMILY_VER2_CHECK_PHONE_CALL;
						}
						else if (CheckTimeout(&tWaitForCallTimeOut) == TIMEOUT)
						{
							if (familyVer2Retry++ < 2)
							{
								AudioStopAll();
								AudioPlayFile("3_1", TOP_INSERT);			// emergency call is being placed
								AudioPlayFile("4_2", BOT_INSERT);			// please wait for call back
								InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
								sosPhase = SOS_INIT;
							}
							else
							{
								familyVer2Retry = 0;
								AudioStopAll();
								AudioPlayFile("3_6", TOP_INSERT);			// SOS failed
								sosPhase = FAMILY_VER2_SOS_END;
							}
						}
						else
						{
							SosTask_PromptForWaiting();
						}
						break;
						
					case FAMILY_VER2_CHECK_PHONE_CALL:
						if (VoiceCallTask_GetCallerPhoneNo() && (incomingCallPhoneNo[0] != 0))
						{
							memset(callerPhoneNo, 0, sizeof(callerPhoneNo));
							memcpy(callerPhoneNo, incomingCallPhoneNo, PHONE_NO_LENGTH);
							callerPhoneNo[PHONE_NO_LENGTH - 1] = 0;
							res = 0;
							for(i = 0; i < MAX_SEND_SMS_NUMBER_FAMILY_VER2; i++)
							{
								if(sysCfg.userList[i].phoneNo[0])
								{
									if (ComparePhoneNumber((char*)sysCfg.userList[i].phoneNo, (char*)callerPhoneNo) == SAME_NUMBER)
									{
										res = 1;
										break;
									}
								}
							}
							if (res == 1)
							{
								VoiceCallTask_AnswerCall();
								InitTimeout(&tSosTaskTimeOut, TIME_SEC(20));
								sosPhase = FAMILY_VER2_WAIT_ANSWER_CALL;
							}
							else
							{
								VoiceCallTask_EndCall();
								sosPhase = FAMILY_VER2_WAIT_CALLING;
							}
						}
						else if (CheckTimeout(&tSosTaskTimeOut) == TIMEOUT)
						{
							VoiceCallTask_EndCall();
							sosPhase = FAMILY_VER2_WAIT_CALLING;
						}
// 						else
// 						{
// 							SosTask_PromptForWaiting();
// 						}
						break;
						
					case FAMILY_VER2_WAIT_ANSWER_CALL:
						if (VoiceCallTask_GetPhase() == IN_CALLING)
						{
							SmsTask_ClearAllSms();
							SmsTask_AddAllContactPhoneNo();
							if (sysCfg.sosRespondedSms[0])
							{
								sprintf((char*)mainBuf1, (char*)sysCfg.sosRespondedSms, callerPhoneNo);
							}
							else
							{
								sprintf((char*)mainBuf1, DEFAULT_SOS_RESPONDED_SMS, callerPhoneNo);
							}
							SmsTask_SetMessage(mainBuf1);
							SmsTask_SendSms();
							sosPhase = FAMILY_VER2_WAIT_END_CALL;
						}
						else if (VoiceCallTask_IsIdle())
						{
							sosPhase = FAMILY_VER2_WAIT_CALLING;
						}
// 						else
// 						{
// 							SosTask_PromptForWaiting();
// 						}
						break;
						
					case FAMILY_VER2_WAIT_END_CALL:
						if (VoiceCallTask_GetPhase() == UNSUCCESS_CALL)
						{
							AudioStopAll();
							AudioPlayFile("6_5", TOP_INSERT);		// call unsuccessful
							AudioPlayFile("4_2", BOT_INSERT);				// please wait for call back
							InitTimeout(&tPromptCallBackTimeOut, TIME_SEC(10));
							sosPhase = FAMILY_VER2_WAIT_CALLING;
						}
						else if ((VoiceCallTask_GetPhase() == END_CALL) || (VoiceCallTask_GetPhase() == NO_CALL))
						{
							//VoiceCallTask_ClearEndCallStatus();
							sosPhase = FAMILY_VER2_SOS_END;
						}
						break;
						
					case FAMILY_VER2_SOS_END:
						ResetAllAlarmWarning();
						VoiceCallTask_ClearCallAction();
						sosPhase = SOS_END;
						break;
				}
				break;
			
			default:				
				break;
		}
	}
	
	if (sosPhase != SOS_END)
	{
		flagLoneWorkerReset = 1;
		flagNoMovementReset = 1;
		flagManDownReset = 1;
		return 0xff;
	}
	
	return 0;
}
