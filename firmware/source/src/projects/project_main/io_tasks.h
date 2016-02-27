#ifndef _IO_TASKS_H_
#define _IO_TASKS_H_

typedef enum {
	NO_CALL = 0,
	INCOMING_CALL,
	SETUP_START_CALL,
	START_CALL,
	WAIT_START_CALL,
	SETUP_ANSWER_CALL,
	ANSWER_CALL,
	WAIT_ANSWER_CALL,
	IN_CALLING,
	HANG_UP_CALL,
	WAIT_HANG_UP_CALL,
	ENSURE_HANG_UP_CALL,
	CHECK_END_CALL,
	END_CALL,
	MISS_CALL,
	BUSY_CALL,
	NO_ANSWER_CALL,
	UNSUCCESS_CALL
} VOICE_CALL_PHASE_TYPE;

typedef enum {
	SMS_IDLE = 0,
	SMS_FINISH_SEND,
	SMS_CHECK,
	SMS_DELAY_START_SEND,
	SMS_START_SEND,
	SMS_WAIT_MODEM_INIT,
	SMS_WAIT_MODEM_ANSWER,
	SMS_SEND,
	SMS_WAIT_SEND,
	SMS_SENT,
	SMS_SEND_FAIL,
	SMS_PAUSED
} SMS_PHASE_TYPE;


extern uint8_t powerAndSosKeyHold3Sec;
extern uint8_t powerAndSosKeyHold1Sec;
extern uint8_t sosKeyPressed;
extern uint8_t powerKeyHold3Sec;
extern uint8_t powerKeyHold2Sec;
extern uint8_t powerKeyHold1Sec;
extern uint8_t powerKeyPressed;



void IoTask_Init(void);
uint8_t Io_Task(void);
void Keys_Init(void);
void PlayBatteryStatus(void);
void PlayAlarmWarningSound(uint8_t firstBeep);

uint8_t VoiceCall_Task(void);
void VoiceCallTask_Init(void);
void VoiceCallTask_StartCall(char* phoneNo, uint8_t dtmfKey);
void VoiceCallTask_AnswerCall(void);
void VoiceCallTask_EndCall(void);
uint8_t VoiceCallTask_GetCallerPhoneNo(void);
uint8_t VoiceCallTask_IsModemBusy(void);
VOICE_CALL_PHASE_TYPE VoiceCallTask_GetPhase(void);
void VoiceCallTask_ClearEndCallStatus(void);
void VoiceCallTask_ClearCallAction(void);
uint8_t VoiceCallTask_IsIdle(void);
void VoiceCallTask_DeInitDtmf(void);
uint8_t VoiceCallTask_IsDtmfKeyPressed(void);

uint8_t Sms_Task(void);
void SmsTask_Init(void);
void SmsTask_ClearAllSms(void);
void SmsTask_AddPhoneNo(uint8_t *phoneNo);
void SmsTask_AddAllContactPhoneNo(void);
void SmsTask_SetMessage(uint8_t *message);
void SmsTask_SendSms(void);
SMS_PHASE_TYPE SmsTask_GetPhase(void);
uint8_t SmsTask_IsSmsSent(void);
uint8_t SmsTask_IsIdle(void);
uint8_t SmsTask_IsModemBusy(void);
void SmsTask_Stop(void);
void SmsTask_Pause(void);

uint8_t IoTask_IsModemBusy(void);

#endif
