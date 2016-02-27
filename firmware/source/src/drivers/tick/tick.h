#ifndef __TICK_H__
#define __TICK_H__
#include <stdint.h>

#define TICK_SECOND 1000

#define TIME_MS(x)	x
#define TIME_SEC(x)	(x*1000)

#define TIMEOUT		0
#define TIMEIN		1

#define KEY_DOWN 	0
#define KEY_UP 		1

#define WATCHDOG_NUM	3 /*MAIN + Timer0 + Timer1*/

typedef struct {
	uint32_t start_time; 		
	uint32_t timeout;
	uint32_t crc; 
} Timeout_Type;

typedef struct {
	uint8_t *buff;
	uint8_t index;
	uint8_t len;
}COMPARE_TYPE;


typedef struct
{
	uint8_t lastStatus;
	uint32_t lastTime;
	uint8_t status;
	uint32_t time;
	uint8_t saveBit;
	uint32_t pressTime;
	uint32_t pressed;
} _KEY_TYPE;

typedef struct
{
	_KEY_TYPE sos;
	_KEY_TYPE pwr;
} KEY_TYPE;

extern KEY_TYPE key;

extern volatile uint32_t rtcTimeSec;
extern uint8_t ledControlEnable;
extern uint8_t loneWorkerWarningFlag, flagLoneWorkerReset;
extern uint8_t noMovementWarningFlag, flagNoMovementReset;
extern uint8_t manDownWarningFlag, flagManDownReset;
extern uint8_t ringingDetectFlag;


void LPTimerTask(void);
void LPTMR_init(int count, int clock_source);
void TICK_Init(uint32_t timeMs,uint32_t coreClock);
void TICK_DeInit(void);
uint32_t TICK_Get(void);
uint32_t TICK_Get64(void);
void UpdateRtcTime(uint32_t updateTimeSec);
void DelayMs(uint32_t ms);
void InitTimeout(Timeout_Type *t,uint32_t timeout);
uint32_t CheckTimeout(Timeout_Type *t);
void InitFindData(COMPARE_TYPE *cmpData,uint8_t *data);
uint8_t FindData(COMPARE_TYPE *cmpData,uint8_t c);
uint8_t GetKeyPressed(_KEY_TYPE *key);
void ClearLastKeyStatus(_KEY_TYPE *key);
#endif

