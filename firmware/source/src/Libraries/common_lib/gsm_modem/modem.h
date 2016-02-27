#ifndef _MODEM_H_
#define _MODEM_H_

#include <string.h>
#include <stdio.h>
#include "tick.h"
#include "uart.h"
#include "ringbuf.h"
#include "dbg_cfg_app.h"
#include "system_config.h"
#include "sys_time.h"
#include "uart.h"

#define COMM_Open		uart_init //(UART1_BASE_PTR,115200)
#define commBuf			USART1_RxRingBuff
#define COMM_Puts(x)		uart_putstr(UART1_BASE_PTR,x)
#define COMM_Putc(x)		uart_putchar(UART1_BASE_PTR,x)


#define MODEM_REGESTERED														1
#define MODEM_NOT_REGESTERED												2


#define MODEM_POWER_ON	1		
#define MODEM_POWER_OFF	0


#define MODEM_Info(...)		//DbgCfgPrintf(__VA_ARGS__)
#define MODEM_Debug(...) //DbgCfgPrintf(__VA_ARGS__)

#define GSM_NO_REG_STATUS	0
#define GSM_REG_OK_STATUS 1
#define GSM_GPRS_OK_STATUS	2
#define GSM_CONNECTED_SERVER_STATUS	3

typedef enum{
	MODEM_CMD_SEND = 0,
	MODEM_CMD_WAIT,
	MODEM_CMD_ERROR,
	MODEM_CMD_OK,
	MODEM_CMD_FINISH
} MODEM_CMD_PHASE_TYPE;

/**
 *CELL LOCATE
 */
typedef struct _cellLocate
{
    DATE_TIME time;       /**< UTC of position */
		double  lat;        /**< Latitude in NDEG - [degree][min].[sec/60] */
		double  lon;        /**< Longitude in NDEG - [degree][min].[sec/60] */
    double  speed;      /**< Speed over the ground in knots */
    double  dir;  /**< Track angle in degrees True */
    double  uncertainty; 
		double alt;
		char ew;
		char ns;
} cellLocateType;


extern uint8_t flagGsmStatus;
extern cellLocateType cellLocate;
extern const uint8_t *modemOk;
extern const uint8_t *modemError;

extern uint8_t gsmDebugBuff[256];
extern volatile uint8_t modemRinging;
extern uint8_t csqBuff[16];
extern float csqValue;
extern uint8_t gsmSignal;
extern uint32_t batteryPercent;
extern uint32_t agpsRestartCnt;
extern uint32_t cellPass;
extern uint8_t modemId[16];

uint8_t MODEM_PowerOff(void);
uint8_t MODEM_Init(void);
void ModemCmdTask_SetCriticalCmd(void);
uint8_t ModemCmdTask_IsCriticalCmd(void);
uint8_t MODEM_Dial(void);
uint8_t MODEM_Ringing(void);
uint8_t MODEM_CarrierDetected(void);
void MODEM_DataMode(void);
void MODEM_CommandMode(void);
void MODEM_MonitorInput(void);
void MODEM_EndRing(void);
void MODEM_Hangup(void);
uint8_t MODEM_SendCommand(const uint8_t *resOk,const uint8_t *resFails,uint32_t timeout,uint8_t tryAgainNum,const uint8_t *format, ...);
void ModemCmdTask_SetWait(const uint8_t *resOk, const uint8_t *resFails, uint32_t timeout);
uint8_t ModemCmdTask_SendCmd(uint32_t delay,const uint8_t *resOk,const uint8_t *resFails,uint32_t timeout,uint8_t tryAgainNum,const uint8_t *format, ...);
void ModemCmdTask_Reset(void);
uint8_t MODEM_CheckResponse(uint8_t *str,uint32_t t);
uint8_t MODEM_Wait(const uint8_t *res, uint32_t time);
uint8_t MODEM_Gets(uint8_t *buf, uint16_t bufSize, uint8_t match);
uint8_t MODEM_SendSMS(const uint8_t *recipient, uint8_t *data,uint16_t sizeMax);
uint8_t SendSMS(const uint8_t *recipient, const uint8_t *data);
uint8_t MODEM_GetCellId(uint32_t *lac, uint32_t *ci);
uint8_t MODEM_GetCSQ(void);
uint8_t InternetSetProfiles(uint8_t *apn,uint8_t *usr,uint8_t *pwd);
uint8_t MODEM_ConnectSocket(uint8_t socket,uint8_t *ip,uint16_t port,uint8_t useIp);
uint8_t MODEM_DisconectSocket(uint8_t socket);
uint8_t MODEM_CloseSocket(uint8_t socket);
uint8_t MODEM_EnableGPRS(void);
uint8_t MODEM_DisableGPRS(void);
uint8_t MODEM_CreateSocket(uint8_t socket);
uint16_t MODEM_CheckGPRSDataOut(uint8_t socket);
uint8_t MODEM_GprsSendData(uint8_t socket,uint8_t *data,uint32_t length);
uint8_t MODEM_Started(void);
uint8_t GetCellLocate(uint8_t *buff,uint8_t waitEn);
uint8_t ParserCellLocate(void);
uint8_t CallingProcess(uint8_t callAction);
uint8_t VoiceSetup(void);
void GetIMEI(uint8_t *buf, uint16_t bufSize);
void MODEM_FlushBuffer(void);
uint8_t MODEM_GetSocket(uint8_t socket);
MODEM_CMD_PHASE_TYPE ModemCmdTask_GetPhase(void);
uint8_t ModemCmdTask_IsIdle(void);
void ModemCmd_Task(void);
void ModemCmdTask_Cancel(void);

#endif
