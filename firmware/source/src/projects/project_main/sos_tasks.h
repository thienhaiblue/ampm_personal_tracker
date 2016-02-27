#ifndef _SOS_TASKS_H_
#define _SOS_TASKS_H_

#define MODE_CALL_CENTER 	0
#define MODE_FAMILY_VER1 	1
#define MODE_FAMILY_VER2 	2
#define SOS_CANCELLED			9

#define PHONE_NO_LENGTH									16
#define SAME_NUMBER											0
#define DIFFERENT_NUMBER								1
#define PHONE_DIGIT_THRESHOLD						6

typedef enum {
	SOS_OFF = 0,
	SOS_KEY_ON,
	SOS_LONE_WORKER_ON,
	SOS_NO_MOVEMENT_ON,
	SOS_MAN_DOWN_ON
} SOS_ACTION;

typedef enum {
	SOS_CHECK = 0,
	SOS_START,
	SOS_WAIT_MODEM_INIT,
	SOS_WAIT_SEND_DATA,
	SOS_WAIT_SEND_SMS,
	SOS_INIT,
	SOS_WAIT_INIT,
	CALL_CENTER_CALL_INIT,
	CALL_CENTER_WAIT_CALLING,
	CALL_CENTER_IN_CALLING,
	CALL_CENTER_SOS_END,
	WAIT_DTMF_KEY_PRESS_INIT,
	WAIT_DTMF_KEY_PRESS,
	FAMILY_VER1_CALL_INIT,
	FAMILY_VER1_WAIT_CALLING,
	FAMILY_VER1_IN_CALLING,
	FAMILY_VER1_NOT_PRESS_KEY,
	FAMILY_VER1_SOS_END,
	FAMILY_VER2_CALL_INIT,
	FAMILY_VER2_SEND_SMS,
	FAMILY_VER2_WAIT_CALLING,
	FAMILY_VER2_CHECK_PHONE_CALL,
	FAMILY_VER2_WAIT_ANSWER_CALL,
	FAMILY_VER2_WAIT_END_CALL,
	FAMILY_VER2_SOS_END,
	SOS_END
} SOS_PHASE_TYPE;

uint8_t Sos_Task(void);
SOS_PHASE_TYPE SosTask_GetPhase(void);
void SosTask_Init(void);
void SosTask_Start(void);
void SosTask_End(void);
uint8_t SosTask_IsCancelCall(void);
uint8_t SosTask_IsAlarmActivated(void);
uint8_t SosTask_IsWaititngForCall(void);
void SosTask_Reset(void);
uint8_t ComparePhoneNumber(char* phone1, char* phone2);

#endif