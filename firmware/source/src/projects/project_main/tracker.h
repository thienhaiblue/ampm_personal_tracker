#ifndef __TRACKER_H__
#define __TRACKER_H__

#include <stdint.h>
#include "ringbuf.h"
#include "db.h"


#define	M_PI		3.14159265358979323846	/* pi */
#define d2r (M_PI / 180.0)


extern uint32_t rpSampleCount;

typedef struct __attribute__((packed)){
	uint8_t oldMsgIsEmpty;
	uint8_t newMsgIsEmpty;
	uint8_t lastMsgIsEmpty;
	MSG_STATUS_RECORD	oldMsg;
	MSG_STATUS_RECORD	newMsg;
	MSG_STATUS_RECORD lastMsg;
} TRACKER_MSG_TYPE;



extern TRACKER_MSG_TYPE trackerMsg;
extern MSG_STATUS_RECORD	logRecord,logRecordTemp;
extern uint32_t serverSendDataFlag;
extern RINGBUF trackerRbOldMsg;
extern RINGBUF trackerRbNewMsg;
extern uint8_t trackerEnable;

extern uint8_t hasMoved;

void TrackerInit(void);
void TrackerTask_EnableGps(void);
void TrackerTask(void);
uint16_t CalculateCRC(const int8_t* buf, int16_t len);
//float haversine_km(float lat1, float long1, float lat2, float long2);
uint32_t GetTrackerMsg(char *buff,uint32_t *len);
uint32_t GetLastTrackerMsg(char *buff,uint32_t *len);
uint32_t CheckTrackerMsgIsReady(void);
uint32_t CheckTrackerMsgFill(void);

#endif

