#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include "sys_time.h"
#include "system_config.h"

#define LOG_WRITE_BYTES 4
#define RINGLOG_MAX 	(LOG_DATA_SIZE_MAX / sizeof(MSG_STATUS_RECORD))

typedef struct
{
	uint32_t value;
	uint32_t oldValue;
	uint32_t cnt;
}DB_U32;


/*
* log db layout:
* year_month/mday/hour.log		speed log every second
*/

typedef struct __attribute__((packed)){
	uint32_t head;
	uint32_t tail;
	uint8_t crc;
}LOG_RING_TYPE;

typedef struct __attribute__((packed)){
	LOG_RING_TYPE ring;
	uint32_t logBeginAddr;
	uint32_t logSectorCnt;
	uint32_t dataBeginAddr;
	uint32_t dataLogSize;
	uint32_t dataLogAreaSize;
	uint32_t cnt;
	uint8_t crc;
}LOG_TYPE;



typedef struct __attribute__((packed)){
	DATE_TIME currentTime; //8 thoi gian hien tai
	double GPS_lat;//16 kinh do
	double GPS_lon;//24 vi do
	float GPS_hdop;// 28  sai so trong don vi met
	float speed;//32 toc do
	float dir;//36 huong di chuyen
	double CELL_lat;//44 kinh do
	double CELL_lon;//52 vi do
	float CELL_hdop;// 56  sai so trong don vi met
	float mileage; //60 so km hien thi tren dong ho do cua xe
	uint8_t GPS_ns;	//61
	uint8_t GPS_ew; //62
	uint8_t CELL_ns;	//63
	uint8_t CELL_ew; //64
	//status Alert
	uint8_t SOS_action; //65
	/*
	1st byte Detection SOS button
		0	Default, ( SOS off )
		1	SOS Key on
		2	SOS Key off
		3	Lone worker SOS
		4	Man Down
	*/
	uint8_t geoFenceCheck; //66
	/*
		0	Default, No report on Geo fence
		1	Exclusion zone
		2	Inclusion zone
	*/
	uint8_t GSM_csq; //67
	
	/*
		0~99	CSQ : 0~99
		4		> CSQ = 99	No signal
		4 	< CSQ <	10	Level 1
		10 	< CSQ <	16	Level 2
		16 	<	CSQ <	22	Level 3
		22 	< CSQ <	28	Level 4
		28 	< CSQ				Level 5
	*/
	
	uint8_t batteryPercent;  //68
	uint32_t status; //72
	
	/*
	Bits 0: Charging status
	0 = Not under Charging
	1 Under Charging
	Bits 2-3: Battery check
	0 = Default, Battery normal
	1 = Low Battery < 25%
	2 = No Battery 5% remained
	Bits 4: Man Down Check
	0 = Default, No report
	1 =  Man down Active
	Bits 5:Lone worker check
	0 =  Default, no report
	1 =  Lone worker Alert Active
	Bits 6: No Movement check
	0 = Default, no report
	1 =  No Movement Alert Active
	*/

	
	uint8_t serverSent; //73
	uint8_t gpsFixed;
	uint8_t dummy[1];
	uint8_t crc;//74
}MSG_STATUS_RECORD;



typedef struct __attribute__((packed))
{
	uint32_t sysResetTimes;
	uint32_t sysHaltFailTimes;
}SYSTEM_STATUS_TYPE;

extern uint32_t resetNum;
extern float mileage;
extern uint32_t totalPulse;


extern LOG_TYPE trackerLog;

extern DB_U32 dbTimeZone;
extern DB_U32 pwrStatus;
extern DB_U32 gpsLat;
extern DB_U32 gpsLon;
extern DB_U32 gpsHDOP;
extern DB_U32 gpsDir;
extern DB_U32 cellLat;
extern DB_U32 cellLon;
extern DB_U32 cellHDOP;
extern DB_U32 cellDir;
extern DB_U32 timeSave;


uint8_t DbCalcCheckSum(uint8_t *buff, uint32_t length);

//flash log
uint32_t DB_DataLogFill(LOG_TYPE *log);
void DB_InitLog(LOG_TYPE *log,uint32_t logBeginAddr,uint32_t logSectorCnt,uint32_t dataBeginAddr,uint32_t dataLogSize,uint32_t dataLogAreaSize);
char DB_SaveDataLog(uint8_t *data,LOG_TYPE *log);
char DB_LoadDataLogTail(uint8_t *data,LOG_TYPE *log);
char DB_LoadDataInfo(uint8_t *data,LOG_TYPE *log);
void DB_LogRingReset(LOG_TYPE *log);
void DB_LogRingSave(LOG_TYPE *log);
void DB_ResetCntLoad(void);
void DB_ResetCntSave(void);
uint32_t DB_FlashMemInit(void);
void DB_U32Save(DB_U32 *dbu32,uint32_t addr);
void DB_U32Load(DB_U32 *dbu32,uint32_t addr);
uint32_t DB_FloatToU32(double lf);
float DB_U32ToFloat(uint32_t *u32);

//mileage
void DB_MileageLoad(void);
void DB_MileageSave(void);

#endif

