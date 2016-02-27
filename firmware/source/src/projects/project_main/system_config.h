#ifndef __SYSTEM_CONFIG_H__
#define __SYSTEM_CONFIG_H__
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define DEBUG_MODE	1

#define PAGE_SIZE                         (1024)    /* 4 Kbyte */
#define FLASH_SIZE                        (0x20000)  /* 128 KBytes */

#define CONFIG_AREA_SIZE		4096
#define CONFIG_AREA_START ((uint32_t)(FLASH_SIZE - PAGE_SIZE*(CONFIG_AREA_SIZE/PAGE_SIZE)))

#define POWER_OFF	0x0FF0FF0F
#define POWER_ON	0x01101101

#define FIRMWARE_VERSION	"4.0"

#define HARDWARE_V3



#define WARNING_SMS							4
#define WARNING_CALL						8


#define DEFAULT_BOSS_NUMBER	"+886975398002"
#define DEFAULT_SMS_PWD			"12345678"
#define DEFAULT_GPSR_APN		"internet"
#define DEFAULT_GPRS_USR		"mms"
#define DEFAULT_GPRS_PWD		"mms"
#define DEFAULT_SMS_MASTER_PWD	"ZOTA"

#define DEFAULT_DSERVER_NAME	"http://trackingatoz.vn"
#define DEFAULT_DSERVER_IP0		210
#define DEFAULT_DSERVER_IP1		71
#define DEFAULT_DSERVER_IP2		190
#define DEFAULT_DSERVER_IP3		170
#define DEFAULT_DSERVER_PORT	4051
#define DEFAULT_DSERVER_USEIP	1


#define DEFAULT_FSERVER_NAME	"http://atoztracking.vn"
#define DEFAULT_FSERVER_IP0		210
#define DEFAULT_FSERVER_IP1		71
#define DEFAULT_FSERVER_IP2		190
#define DEFAULT_FSERVER_IP3		170
#define DEFAULT_FSERVER_PORT	50000
#define DEFAULT_FSERVER_USEIP	1

#define DEFAULT_ISERVER_NAME	"http://atoztracking.vn"
#define DEFAULT_ISERVER_IP0		210
#define DEFAULT_ISERVER_IP1		71
#define DEFAULT_ISERVER_IP2		190
#define DEFAULT_ISERVER_IP3		170
#define DEFAULT_ISERVER_PORT	12345
#define DEFAULT_ISERVER_USEIP	1

#define DEFAULT_INFO_SERVER_NAME	"http://atoztracking.vn"
#define DEFAULT_INFO_SERVER_IP0		118
#define DEFAULT_INFO_SERVER_IP1		69
#define DEFAULT_INFO_SERVER_IP2		60
#define DEFAULT_INFO_SERVER_IP3		174
#define DEFAULT_INFO_SERVER_PORT	2802
#define DEFAULT_INFO_SERVER_USEIP	1


#define DEFAULT_REPORT_INTERVAL				10 	//min
#define DEFAULT_LONE_WORKER_INTERVAL	30 	//min
#define DEFAULT_ALARM_COUNTDOWN_TIME 	8		//sec
#define DEFAULT_NO_MOVEMENT_INTERVAL	60 	//min
#define DEFAULT_OPERATION_MODE				3

#define DEFAULT_CELL_NS								'N'
#define DEFAULT_CELL_EW								'E'
#define DEFAULT_GPS_NS								'N'
#define DEFAULT_GPS_EW								'E'

#define DEFAULT_CALL_ME_SMS						"Call Me!"
#define DEFAULT_SOS_RESPONDED_SMS			"Emergency call responded by %s."
#define DEFAULT_SOS_CANCELLED_SMS			"Emergency call is cancelled."

#define DEFAULT_SLEEP_TIMER		60


// system definitions
#define CONFIG_HW_VERSION			1
#define CONFIG_FW_VERSION			1
#define CONFIG_RELEASE_DATE			__TIMESTAMP__
#define CONFIG_MAX_USER			10


#define CONFIG_SIZE_GPRS_APN		16
#define CONFIG_SIZE_GPRS_USR		32
#define CONFIG_SIZE_GPRS_PWD		16
#define CONFIG_SIZE_SERVER_NAME		31
#define CONFIG_SIZE_SMS_PWD			16
#define CONFIG_SIZE_PLATE_NO		12
#define CONFIG_SIZE_USER_NAME		32
#define CONFIG_SIZE_PHONE_NUMBER	16
#define CONFIG_SIZE_SMS						80


#define FEATURE_GPS_AUTO_AIDING						0x0001
#define FEATURE_GPS_ASSISTNOW_OFFLINE			0x0002
#define FEATURE_GPS_ASSISTNOW_ONLINE			0x0004
#define FEATURE_GPS_ASSISTNOW_AUTONOMOUS	0x0008
#define FEATURE_LONE_WORKER_SET						0x0010
#define FEATURE_NO_MOVEMENT_WARNING_SET		0x0020
#define FEATURE_MAN_DOWN_SET							0x0040
#define FEATURE_SUPPORT_FIFO_MSG					0x0080
#define FEATURE_SMS_WARNING_ACTION_SET		0x0100
#define FEATURE_CALL_WARNING ACTION_SET		0x0200
#define FEATURE_AUTO_ANSWER								0x0400
#define FEATURE_DTMF_ON										0x0800

#define DEFAULT_MAN_DOWN_INTERVAL					30


#define CONFIG_SIZE_IP				32

#define FULL_RUNNING_MODE	0
#define GPS_ONLY_MODE			1
#define GSM_ONLY_MODE			2
#define STANDBY_MODE			3
#define SLEEPING_MODE			4

#define CONFIG_SPEED_STOP	5 //km/h

#define UPDATE_FIRMWARE_ENABLE	0xa5


typedef struct __attribute__((packed)){
	int8_t userName[CONFIG_SIZE_USER_NAME];
	int8_t phoneNo[CONFIG_SIZE_PHONE_NUMBER];
}USER_INFO;

typedef struct __attribute__((packed)){
	uint8_t geoFenceEnable;
	double lat;
	double lon;
	float radius;
	uint16_t time;
}GEO_FENCE_TYPE;

typedef struct __attribute__((packed))
{
	uint16_t size;
	int8_t imei[18];
	int8_t id[18];
	int8_t smsPwd[CONFIG_SIZE_SMS_PWD];					/**< SMS config password */
	int8_t whiteCallerNo[CONFIG_SIZE_PHONE_NUMBER];		/**< */
	// GPRS config
	int8_t gprsApn[CONFIG_SIZE_GPRS_APN];
	int8_t gprsUsr[CONFIG_SIZE_GPRS_USR];
	int8_t gprsPwd[CONFIG_SIZE_GPRS_PWD];
	uint16_t reportInterval; /*1 min to 30 mins or 1 hour to 24 hours*/
	// primary server config
	uint16_t priDserverIp[2];		/**< ip addr */
	uint8_t  priDserverName[CONFIG_SIZE_SERVER_NAME];	/**< domain name */
	uint16_t priDserverPort;	/**< port */	
	uint16_t secDserverIp[2];
	uint8_t  secDserverName[CONFIG_SIZE_SERVER_NAME];
	uint16_t secDserverPort;
	uint8_t  dServerUseIp;	/**<>**/
	// primary server config
	uint16_t priFserverIp[2];		/**< ip addr */
	uint8_t  priFserverName[CONFIG_SIZE_SERVER_NAME];	/**< domain name */
	uint16_t priFserverPort;	/**< port */	
	uint16_t secFserverIp[2];
	uint8_t  secFserverName[CONFIG_SIZE_SERVER_NAME];
	uint16_t secFserverPort;
	uint8_t  fServerUseIp;	/**<>**/
//  current use info
	uint8_t userIndex;
	USER_INFO userList[CONFIG_MAX_USER];
	
	GEO_FENCE_TYPE geoFence[10];
	
	uint16_t featureSet;			/**< feature set mapping 
														bit0: GPS Automatic local aiding
														bit1: GPS AssistNow offline
														bit2: GPS AssistNow online
														bit3: GPS AssistNow autonomous
														bit4: Lone Worker On/Off 
														bit5: No movement warning enable
														bit6: Man Down Enable
														nit7: FIFO Message enable
														bit8: Warning by Call action
														bit9: Warning by SMS action 
														bit10: Auto Answer
														bit11: DTMF On
														*/
	uint16_t loneWorkerInterval;	/*1 to 1000min*/
	uint8_t alarmCountdownTime; /*5s to 30s*/
	uint16_t noMovementInterval; /*1min to 1000min*/
	uint8_t operationMode; /*include: 
																	+0 = Full Running Mode GPS On, GSM On
																	+1 = GPS Only mode GPS on all time, GSM off.
																	+2 = GSM Only mode GSM on all time. GPS Off.
																	+3 = Standby mode	GSM Sleep,GPS in standby. GSM and GPS wake up by Report inverval time
																	+4 = Sleeping mode GSM Off, GPS Off  .GSM and GPS wake up by Report inverval time
																	*/
	uint8_t SOS_detection;
												/*
													0	Default, ( SOS off )
													1	SOS Key on
													2	Lone worker SOS
													3	No Movement SOS
													4	Man Down
												*/
	uint8_t geoFenceType;
												/*
												0	Default, No report on Geo fence
												1	Exclusion zone
												2	Inclusion zone
												*/
	uint8_t firmwareUpdateEnable;
	uint8_t language;
	uint8_t familyPalMode;
	
	uint8_t callMeSms[CONFIG_SIZE_SMS];
	uint8_t sosRespondedSms[CONFIG_SIZE_SMS];
	uint8_t	sosCancelledSms[CONFIG_SIZE_SMS];
	
	uint32_t crc;
}CONFIG_POOL;

extern CONFIG_POOL sysCfg;
extern uint8_t sysUploadConfig;

void CFG_Save(void);
void CFG_Load(void);
void CFG_Write(uint8_t *buff, uint32_t address, int offset, int length);
uint8_t CFG_CheckSum(CONFIG_POOL *sysCfg);
uint8_t CFG_Check(CONFIG_POOL *sysCfg);
#endif
