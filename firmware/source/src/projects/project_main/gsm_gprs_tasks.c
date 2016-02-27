#include "sysinit.h"
#include "project_common.h"
#include "at_command_parser.h"
#include "hw_config.h"
#include "db.h"
#include "gprs_cmd_ctrl.h"
#include "tracker.h"
#include "power_management.h"
#include "sos_tasks.h"
#include "modem.h"
#include "tick.h"
#include "io_tasks.h"
#include "gsm_gprs_tasks.h"
#include "upload_config_task.h"
#include "gps_task.h"

#define GPRS_CHECK_ACK_TIMEOUT						20
#define DATA_SERVER_RECONNECT_TIME				10
#define FIRMWARE_SERVER_RECONNECT_TIME		15
#define DATA_SERVER_TIME_MAX_RESEND				600
#define FIRMWARE_SERVER_TIME_MAX_RESEND		600
#define TIMEOUT_SERVER_CONNECT						30

GPRS_TASK_TYPE gprs_Task = NO_TASK;
GPRS_PHASE_TYPE sendPowerCmd_TaskPhase = TASK_SYS_SLEEP;
GPRS_PHASE_TYPE sendGpsData_TaskPhase = TASK_SYS_SLEEP;

uint8_t svUseIP;
uint8_t priDServer = 1;
uint8_t priFServer = 1;
uint8_t *priServerIp;
uint16_t priServerPort;

GSM_PHASE_TYPE gsm_TaskPhase;

Timeout_Type tTaskPhaseTimeOut;
Timeout_Type tCheckAckTimeOut;
Timeout_Type tCollectDataTimeOut;
Timeout_Type tGsmTaskPhaseTimeOut;
Timeout_Type tWaitSendDataTimeOut;

uint8_t taskRetry = 0;
uint8_t createSocketConError = 0;
uint8_t msgSentCnt = 0;
uint8_t tcpAckCheckTimes;
uint8_t tcpResendDataTimes = 0;
uint32_t gprsDataLength = 0;
uint8_t gprsData[560];
uint8_t mainBuf1[256];
uint32_t sosDataLength = 0;
uint8_t sosData[256];
uint8_t enableGprsRetry = 0;
uint8_t serverConnectRetry = 0;
uint8_t gprsSendDataRetry = 0;
uint8_t registerNetworkRetry = 0;
uint8_t isPowerOffCmd = 0;
uint8_t gprsProfileEnabled = 0;
uint8_t sendFirstPacket = 0;

uint8_t SendPowerCmd_GprsTask(void);
uint8_t SendGpsData_GprsTask(void);

uint8_t Gprs_Task(void)
{
	if ((GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK) &&
			VoiceCallTask_IsIdle() &&
			SmsTask_IsIdle() &&
			(ringingDetectFlag == 0) &&
			(ringingFlag == 0))
	{
		switch (gprs_Task)
		{
			case SEND_POWER_CMD_TASK:
				SendPowerCmd_GprsTask();
				break;
			
			case SEND_SOS_DATA_TASK:
			case SEND_GPS_DATA_TASK:
				SendGpsData_GprsTask();
				break;
			
			case UPLOAD_CONFIG_TASK:
				UploadConfig_Task();
				break;
			
			case NO_TASK:
				if (sosDataLength > 0)
				{
					if (SosTask_GetPhase() == SOS_END)
					{
						GprsTask_StartTask(SEND_SOS_DATA_TASK);
					}
				}
				else if (flagPwrOn || flagPwrOff)
				{
					GprsTask_StartTask(SEND_POWER_CMD_TASK);
				}
				else if (sysUploadConfig)
				{
					if ((UploadConfigTask_IsWaitingForRetry() == 0) ||
							UploadConfigTask_IsWaitingForRetryTimedOut())
					{
						GprsTask_StartTask(UPLOAD_CONFIG_TASK);
					}
					else if (sysUploadConfig == 2)
					{
						// new config available 
						// --> abort waiting, start upload config immediately
						GprsTask_StartTask(UPLOAD_CONFIG_TASK);
					}
				}
				else if(sendFirstPacket)
				{
					sendFirstPacket = 0;
					GprsTask_StartTask(SEND_GPS_DATA_TASK);
				}
				break;
			
			default:
				break;
		}
		
		if (createSocketConError > 60)
		{
			createSocketConError = 0;
			GprsTask_Stop();
			GsmTask_Init();
		}
	}
	return 0;
}

void GprsTask_Init(void)
{
	gprs_Task = NO_TASK;
	enableGprsRetry = 0;
	serverConnectRetry = 0;
	gprsSendDataRetry = 0;
	isPowerOffCmd = 0;
	sendPowerCmd_TaskPhase = TASK_SYS_SLEEP;
	sendGpsData_TaskPhase = TASK_SYS_SLEEP;
	UploadConfigTask_Init();
}

GPRS_PHASE_TYPE GprsTask_GetPhase(void)
{
	switch (gprs_Task)
	{
		case SEND_POWER_CMD_TASK:
			return sendPowerCmd_TaskPhase;
		case SEND_SOS_DATA_TASK:
		case SEND_GPS_DATA_TASK:
			return sendGpsData_TaskPhase;
		case UPLOAD_CONFIG_TASK:
			return UploadConfigTask_GetPhase();
		default:
			return TASK_SYS_SLEEP;
	}
}

void GprsTask_SetPhase(GPRS_PHASE_TYPE taskPhase)
{
	switch (gprs_Task)
	{
		case SEND_POWER_CMD_TASK:
			sendPowerCmd_TaskPhase = taskPhase;
			break;
		case SEND_SOS_DATA_TASK:
		case SEND_GPS_DATA_TASK:
			sendGpsData_TaskPhase = taskPhase;
			break;
		case UPLOAD_CONFIG_TASK:
			UploadConfigTask_SetPhase(taskPhase);
			break;
		default:
			break;
	}
}

uint8_t GprsTask_StartTask(GPRS_TASK_TYPE task)
{
	uint8_t result = 0;
	GPRS_PHASE_TYPE currentTaskPhase;
	return 1;
	currentTaskPhase = GprsTask_GetPhase();
	
// 	if ((currentTaskPhase != TASK_GPRS_SEND_DATA_HEADER) &&
// 			(currentTaskPhase != TASK_GPRS_SEND_DATA))
// 	{
		switch (task)
		{
			case SEND_POWER_CMD_TASK:
				if ((gprs_Task != SEND_POWER_CMD_TASK) && (gprs_Task != SEND_SOS_DATA_TASK) &&
						(SosTask_GetPhase() == SOS_END))
				{
					GprsTask_Stop();
					gprs_Task = SEND_POWER_CMD_TASK;
					result = 1;
				}
				break;
				
			case SEND_SOS_DATA_TASK:
				if (gprs_Task != SEND_SOS_DATA_TASK)
				{
					GprsTask_Stop();
					gprs_Task = SEND_SOS_DATA_TASK;
					result = 1;
				}
				break;
				
			case SEND_GPS_DATA_TASK:
				if (SosTask_GetPhase() == SOS_END)
				{
					if (gprs_Task == NO_TASK)
					{
						if (flagPwrOn || flagPwrOff)
						{
							gprs_Task = SEND_POWER_CMD_TASK;
						}
						else
						{
							gprs_Task = SEND_GPS_DATA_TASK;
						}
						result = 1;
					}
				}
				break;
				
			case UPLOAD_CONFIG_TASK:
				result = UploadConfigTask_Start();
				break;
			
			default:
				break;
		}
		
		if (result)
		{
			SetLowPowerModes(LPW_NOMAL);
			if (currentTaskPhase >= TASK_GPRS_SERVER_DISCONECT)
			{
				GprsTask_WakeUp();
			}
			else if (currentTaskPhase >= TASK_GPRS_SERVER_CONNECT)
			{
				GprsTask_SetPhase(TASK_GPRS_SERVER_CONNECT);
			}
			else
			{
				GprsTask_SetPhase(currentTaskPhase);
			}
		}
// 	}
	
	return result;
}

void GprsTask_StopTask(GPRS_TASK_TYPE task)
{	
	if ((gprs_Task == task) && (task != NO_TASK))
	{
		if ((GprsTask_GetPhase() == TASK_GPRS_SEND_DATA_HEADER) || (GprsTask_GetPhase() == TASK_GPRS_SEND_DATA))
		{
			InitTimeout(&tWaitSendDataTimeOut, TIME_SEC(30));
			while (((GprsTask_GetPhase() == TASK_GPRS_SEND_DATA_HEADER) || (GprsTask_GetPhase() == TASK_GPRS_SEND_DATA)) &&
						(CheckTimeout(&tWaitSendDataTimeOut) != TIMEOUT))
			{
				ModemCmd_Task();
				Gprs_Task();
			}
		}
		gprs_Task = NO_TASK;
		sendPowerCmd_TaskPhase = TASK_SYS_SLEEP;
		sendGpsData_TaskPhase = TASK_SYS_SLEEP;
		UploadConfigTask_Stop();
	}
	ModemCmdTask_Cancel();
}

void GprsTask_Stop(void)
{
	GprsTask_StopTask(gprs_Task);
}

GPRS_TASK_TYPE GprsTask_GetTask(void)
{
	return gprs_Task;
}

void GprsTask_WakeUp(void)
{
	switch (gprs_Task)
	{
		case SEND_POWER_CMD_TASK:
			if (sendPowerCmd_TaskPhase > TASK_GPRS_SERVER_CONNECT)
			{
				sendPowerCmd_TaskPhase = TASK_SYS_WAKEUP;
			}
			break;
			
		case SEND_SOS_DATA_TASK:
			if (sendGpsData_TaskPhase > TASK_GPRS_SERVER_CONNECT)
			{
				sendGpsData_TaskPhase = TASK_SYS_WAKEUP;
			}
			break;
			
		case SEND_GPS_DATA_TASK:
			if (sendGpsData_TaskPhase > TASK_GPRS_SERVER_CONNECT)
			{
				sendGpsData_TaskPhase = TASK_SYS_WAKEUP;
			}
			break;
			
		case UPLOAD_CONFIG_TASK:
			UploadConfigTask_WakeUp();
			break;
		
		default:
			break;
	}
}

void SendPowerCmdTask_Retry(int sec)
{
	if (IoTask_IsModemBusy() == 0)
	{
		sendPowerCmd_TaskPhase = TASK_RETRY;
		InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(sec));
	}
	else
	{
		GprsTask_Stop();
	}
}

/**
* Send power command DPWO or DPWF to server.
*
*/
uint8_t SendPowerCmd_GprsTask(void)
{
	uint32_t i;
	uint32_t rDataLength;
	
	if (gprsDeactivationFlag)
	{
		gprsDeactivationFlag = 0;
		if (sendPowerCmd_TaskPhase != TASK_SYS_SLEEP)
		{
			sendPowerCmd_TaskPhase = TASK_GSM_ENABLE;
		}
	}
	
	switch(sendPowerCmd_TaskPhase)
	{
		case TASK_SYS_WAKEUP:
			taskRetry = 0;
			sendPowerCmd_TaskPhase = TASK_WAIT_GSM_MODEM_INIT;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(60));
			break;
		
		case TASK_RETRY:
			if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				COMM_Putc(0x1A);
				COMM_Putc(0x1A);
				if(MODEM_SendCommand("+CREG: 0,1","NOT CHECK",1000,3,"AT+CREG?\r"))
				{
					sendPowerCmd_TaskPhase = TASK_GSM_GPRS_DISABLE;
					GsmTask_Init();
				}
				if (taskRetry++ < 3)
				{
					GsmTask_Init();
					sendPowerCmd_TaskPhase = TASK_WAIT_RETRY;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					GprsTask_Stop();
				}
			}
			break;
			
		case TASK_WAIT_RETRY:
			if (!ModemCmdTask_IsCriticalCmd())
			{
				if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					sendPowerCmd_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
			}
			break;
		
		case TASK_WAIT_GSM_MODEM_INIT:
			if(GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
			{
				sendPowerCmd_TaskPhase = TASK_GSM_ENABLE;
			}
			else if(CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				GprsTask_Stop();
			}
			break;
			
		case TASK_GSM_ENABLE:
			// Wakeup module GSM
			GSM_RTS_PIN_CLR;
			DelayMs(200);
			enableGprsRetry = 0;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(1));
			sendPowerCmd_TaskPhase = TASK_GSM_GPRS_ENABLE;
			break;
		
		case TASK_GSM_GPRS_DISABLE:
			if (IoTask_IsModemBusy() == 0)
			{
				if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
				{
					COMM_Putc(0x1A);
					ModemCmdTask_SendCmd(1000, modemOk, modemError, 10000, 0, "AT+UPSDA=0,4\r");
					//ModemCmdTask_SetCriticalCmd();
					gprsProfileEnabled = 0;
					sendPowerCmd_TaskPhase = TASK_GSM_GPRS_ENABLE;
				}
// 				else
// 				{
// 					COMM_Putc(0x1B);
// 					DelayMs(100);
// 				}
			}
			else
			{
				GprsTask_Stop();
			}
			break;
			
		case TASK_GSM_GPRS_ENABLE:
			if (gprsProfileEnabled == 0)
			{
				if (IoTask_IsModemBusy() == 0)
				{
					if (ModemCmdTask_IsIdle())
					{
						COMM_Putc(0x1A);
						ModemCmdTask_SendCmd(2000, modemOk, modemError, 120000, 0,"AT+UPSDA=0,3\r");
						ModemCmdTask_SetCriticalCmd();
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(120));
						sendPowerCmd_TaskPhase = TASK_GSM_GPRS_CHECK;
					}
					else
					{
						COMM_Putc(0x1B);
						DelayMs(100);
					}
				}
				else
				{
					GprsTask_Stop();
				}
			}
			else
			{
				sendPowerCmd_TaskPhase = TASK_GSM_GPRS_OK;
			}
		break;
				
		case TASK_GSM_GPRS_CHECK:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				gprsProfileEnabled = 1;
				flagGsmStatus = GSM_GPRS_OK_STATUS;
				sendPowerCmd_TaskPhase = TASK_GSM_GPRS_OK;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (enableGprsRetry++ < 3)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					sendPowerCmd_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
				else
				{
					enableGprsRetry = 0;
					SendPowerCmdTask_Retry(1);
				}
			}
			break;
		
		case TASK_GSM_GPRS_OK:
			serverConnectRetry = 0;
			sendPowerCmd_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
			break;

		case TASK_GPRS_INIT_TCP_SOCKET:
			MODEM_GetSocket(0);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCL=%d\r", socketMask[0]);
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
			sendPowerCmd_TaskPhase = TASK_GPRS_CREAT_TCP_SOCKET;
			break;
		
		case TASK_GPRS_CREAT_TCP_SOCKET:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				socketMask[0] = 0xff;
				createSocketNow = 0;
				ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCR=6\r");
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				sendPowerCmd_TaskPhase = TASK_GPRS_WAIT_TCP_SOCKET;
			}
			break;
			
		case TASK_GPRS_WAIT_TCP_SOCKET:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				createSocketConError = 0;
				if (socketMask[0] == 0xff)
				{
					socketMask[0] = 0;
				}
				if(sysCfg.dServerUseIp)
				{
					priServerIp = (uint8_t*)sysCfg.priDserverIp;
					ModemCmdTask_SendCmd(100, modemOk, modemError, 15000, 0, 
							"AT+USOCO=%d,\"%d.%d.%d.%d\",%d\r", 
							socketMask[0], 
							priServerIp[0], 
							priServerIp[1], 
							priServerIp[2], 
							priServerIp[3], 
							sysCfg.priDserverPort);
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
					sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_CONNECT;
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 10000, 0, "AT+UDNSRN=0,\"%s\"\r", sysCfg.priDserverName);
					DNS_serverIp[0] = 0;
					InitTimeout(&tTaskPhaseTimeOut,TIME_SEC(10));
					sendPowerCmd_TaskPhase = TASK_GPRS_WAIT_GOT_SERVER_IP;
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				createSocketConError++;
				if (serverConnectRetry++ < 3)
				{
					sendPowerCmd_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendPowerCmdTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_WAIT_GOT_SERVER_IP:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (sizeof(DNS_serverIp) >= 7)
				{
					priServerIp = DNS_serverIp;
					ModemCmdTask_SendCmd(100, modemOk, modemError, 15000, 0, 
							"AT+USOCO=%d,\"%s\",%d\r", 
							socketMask[0], 
							priServerIp,
							sysCfg.priDserverPort);
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
					sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_CONNECT;
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					sendPowerCmd_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendPowerCmdTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_SERVER_CONNECT:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				tcpSocketStatus[0] = SOCKET_OPEN;
				sendPowerCmd_TaskPhase = TASK_GPRS_COLLECT_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					sendPowerCmd_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendPowerCmdTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_COLLECT_DATA:
			if(flagPwrOn)
			{
				isPowerOffCmd = 0;
				gprsDataLength = DPWO_Add((char *)gprsData, sizeof(gprsData));
				gprsSendDataRetry = 0;
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				sendPowerCmd_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
			}					
			else if(flagPwrOff)
			{
				isPowerOffCmd = 1;
				gprsDataLength = DPWF_Add((char *)gprsData, sizeof(gprsData));
				gprsSendDataRetry = 0;
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				sendPowerCmd_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
			}
			else
			{
				sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
			}
			break;
			
		case TASK_GPRS_SEND_DATA_HEADER:
			if (ModemCmdTask_IsIdle())
			{
				ModemCmdTask_SendCmd(500, "@", modemError, 5000, 0, "AT+USOWR=%d,%d\r", socketMask[0], gprsDataLength);
				ModemCmdTask_SetCriticalCmd();
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				sendPowerCmd_TaskPhase = TASK_GPRS_SEND_DATA;
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				SendPowerCmdTask_Retry(1);
			}
			break;
			
		case TASK_GPRS_SEND_DATA:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				ModemCmdTask_SendCmd(100, modemOk, modemError, 5000, 0, NULL);
				for(i = 0; i < gprsDataLength ;i++)
				{
					COMM_Putc(gprsData[i]);
				}
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
				sendPowerCmd_TaskPhase = TASK_GPRS_CHECK_ACK_SEND_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				SendPowerCmdTask_Retry(1);
			}
			break;
			
		case TASK_GPRS_CHECK_ACK_SEND_DATA:
			if (ModemCmdTask_IsIdle() && (CheckTimeout(&tTaskPhaseTimeOut) != TIMEOUT))
			{
				if (CheckTimeout(&tCheckAckTimeOut) == TIMEOUT)
				{
					i = MODEM_CheckGPRSDataOut(DATA_SOCKET_NUM);
					if(i >= 1600)
					{
						SendPowerCmdTask_Retry(1);
					}
					else if(i == 0) //data sent
					{
						flagPwrOn = 0;
						flagPwrOff = 0;
						gprsSendDataRetry = 0;
						if (isPowerOffCmd)
						{
							isPowerOffCmd = 0;
							sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
						}
						else
						{
							InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
							InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
							sendPowerCmd_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
							//sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
						}
					}
					else
					{
						InitTimeout(&tCheckAckTimeOut,TIME_MS(1000));
					}
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				if (gprsSendDataRetry++ < 3)
				{
					gprsSendDataRetry = 0;
					sendPowerCmd_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
				else
				{
					SendPowerCmdTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_CHECK_RECV_DATA:
			if (CheckTimeout(&tCheckAckTimeOut) == TIMEOUT)
			{
				InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
				i = GPRS_CmdRecvData(mainBuf, sizeof(mainBuf), &rDataLength);
				if (i == 0) 		// socket 0
				{
					MODEM_Info("\r\nGPRS->DATA RECV:%dBYTES\r\n", rDataLength);
					mainBuf[rDataLength] = 0;
					if (IsGprsDataPacket(mainBuf, "@SINF", 5))
					{
						memcpy(mainBuf1, mainBuf, sizeof(mainBuf1));
						CMD_CfgParse((char*)&mainBuf1[5], &mainBuf[5], sizeof(mainBuf) - 5, &gprsDataLength, 0);
					}
				}
				sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				sendPowerCmd_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
			}
			break;

		case TASK_GPRS_SERVER_DISCONECT:
			for (i = 0; i < SOCKET_NUM; i++)
			{
				if(tcpSocketStatus[i] == SOCKET_OPEN)
				{
					tcpSocketStatus[i] = SOCKET_CLOSE;
					MODEM_DisconectSocket(i);
				}
			}
			COMM_Putc(0x1A);
			ModemCmdTask_SendCmd(1000, modemOk, modemError, 10000, 0, "AT+UPSDA=0,4\r");
			//ModemCmdTask_SetCriticalCmd();
			gprsProfileEnabled = 0;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
			sendPowerCmd_TaskPhase = TASK_GSM_WAIT_GPRS_DISABLE;
			break;
		
		case TASK_GSM_WAIT_GPRS_DISABLE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				sendPowerCmd_TaskPhase = TASK_GSM_STOP;
			}
			break;
			
		case TASK_GSM_STOP:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				GprsTask_Stop();
			}
			break;
		
		case TASK_SYS_SLEEP:
			break;
			
		default:
			sendPowerCmd_TaskPhase = TASK_SYS_WAKEUP;
			break;
	}
	
	return 0;
}

void SendGpsDataTask_Retry(int sec)
{
	if (IoTask_IsModemBusy() == 0)
	{
		if (SosTask_GetPhase() != SOS_END)
		{
			// if sos is in place, stop the task
			// try to send only 1 time, if failed --> stop
			GprsTask_Stop();
		}
		else
		{
			sendGpsData_TaskPhase = TASK_RETRY;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(sec));
		}
	}
	else
	{
		GprsTask_Stop();
	}
}

/**
* Send GPS data to server. GPS data command is DGPS or DCLI.
*
*/
uint8_t SendGpsData_GprsTask(void)
{
	uint32_t i;
	uint32_t rDataLength;
	
	if (gprsDeactivationFlag)
	{
		gprsDeactivationFlag = 0;
		if (sendGpsData_TaskPhase != TASK_SYS_SLEEP)
		{
			sendGpsData_TaskPhase = TASK_GSM_ENABLE;
		}
	}
	
	switch(sendGpsData_TaskPhase)
	{
		case TASK_SYS_WAKEUP:
			taskRetry = 0;
			msgSentCnt = 0;
			sendGpsData_TaskPhase = TASK_WAIT_GSM_MODEM_INIT;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(60));
			break;
		
		case TASK_RETRY:
			if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				if (taskRetry++ < 3)
				{
					GsmTask_Init();
					sendGpsData_TaskPhase = TASK_WAIT_RETRY;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					GprsTask_Stop();
				}
			}
			break;
			
		case TASK_WAIT_RETRY:
			if (!ModemCmdTask_IsCriticalCmd())
			{
				if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					sendGpsData_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
			}
			break;
		
		case TASK_WAIT_GSM_MODEM_INIT:
			if(GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
			{
				sendGpsData_TaskPhase = TASK_GSM_ENABLE;
			}
			else if(CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				GprsTask_Stop();
			}
			break;
			
		case TASK_GSM_ENABLE:
			// Wakeup module GSM
			GSM_RTS_PIN_CLR;
			DelayMs(200);
			sendGpsData_TaskPhase = TASK_GET_CSQ;
			break;
		
		case TASK_GET_CSQ:
			MODEM_GetCSQ();
			sendGpsData_TaskPhase = TASK_GET_BATERY;
			break;
		
		case TASK_GET_BATERY:
			MODEM_SendCommand(modemOk,modemError,500,0,"AT+CCLK?\r"); //get rtc
			ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CBC\r");
			enableGprsRetry = 0;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(1));
			sendGpsData_TaskPhase = TASK_GSM_GPRS_ENABLE;
			break;
		
		case TASK_GSM_GPRS_DISABLE:
			if (IoTask_IsModemBusy() == 0)
			{
				COMM_Putc(0x1A);
				if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
				{
					ModemCmdTask_SendCmd(1000, modemOk, modemError, 10000, 0, "AT+UPSDA=0,4\r");
					//ModemCmdTask_SetCriticalCmd();
					gprsProfileEnabled = 0;
					sendGpsData_TaskPhase = TASK_GSM_GPRS_ENABLE;
				}
// 				else
// 				{
// 					COMM_Putc(0x1B);
// 					DelayMs(100);
// 				}
			}
			else
			{
				GprsTask_Stop();
			}
			break;
			
		case TASK_GSM_GPRS_ENABLE:
			if (gprsProfileEnabled == 0)
			{
				if (IoTask_IsModemBusy() == 0)
				{
					if (ModemCmdTask_IsIdle())
					{
						COMM_Putc(0x1A);
						ModemCmdTask_SendCmd(2000, modemOk, modemError, 120000, 0, "AT+UPSDA=0,3\r");
						ModemCmdTask_SetCriticalCmd();
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(120));
						sendGpsData_TaskPhase = TASK_GSM_GPRS_CHECK;
					}
					else
					{
						COMM_Putc(0x1B);
						DelayMs(100);
					}
				}
				else
				{
					GprsTask_Stop();
				}
			}
			else
			{
				sendGpsData_TaskPhase = TASK_GSM_GPRS_OK;
			}
		break;
				
		case TASK_GSM_GPRS_CHECK:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				gprsProfileEnabled = 1;
				flagGsmStatus = GSM_GPRS_OK_STATUS;
				sendGpsData_TaskPhase = TASK_GSM_GPRS_OK;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (enableGprsRetry++ < 3)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					sendGpsData_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
				else
				{
					enableGprsRetry = 0;
					SendGpsDataTask_Retry(1);
				}
			}
			break;
			
			case TASK_GSM_GPRS_OK:
				serverConnectRetry = 0;
				if (gprs_Task == SEND_GPS_DATA_TASK || gprs_Task == SEND_SOS_DATA_TASK)
				{
					if (nmeaInfo.fix >= 3)
					{
						trackerEnable = 1;
						sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
					}
					else
					{
						cellPass = 0;
						ModemCmdTask_SendCmd(500, modemOk, modemError, 10000, 0, "AT+ULOC=2,2,0,500,1000\r");
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
						sendGpsData_TaskPhase = TASK_GPRS_GET_CELL_LOCATION;
					}
				}
//				else if (gprs_Task == SEND_SOS_DATA_TASK)
//				{
//					sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
//				}
				else
				{
					sendGpsData_TaskPhase = TASK_GSM_STOP;
				}
				break;

			case TASK_GPRS_GET_CELL_LOCATION:
				if (ModemCmdTask_IsIdle() && ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || cellPass))
				{
					sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				break;

		case TASK_GPRS_INIT_TCP_SOCKET:
			MODEM_GetSocket(0);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCL=%d\r", socketMask[0]);
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
			sendGpsData_TaskPhase = TASK_GPRS_CREAT_TCP_SOCKET;
			break;
		
		case TASK_GPRS_CREAT_TCP_SOCKET:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				socketMask[0] = 0xff;
				createSocketNow = 0;
				ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCR=6\r");
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				sendGpsData_TaskPhase = TASK_GPRS_WAIT_TCP_SOCKET;
			}
			break;
			
		case TASK_GPRS_WAIT_TCP_SOCKET:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				createSocketConError = 0;
				if (socketMask[0] == 0xff)
				{
					socketMask[0] = 0;
				}
				if(sysCfg.dServerUseIp)
				{
					priServerIp = (uint8_t*)sysCfg.priDserverIp;
					ModemCmdTask_SendCmd(100, modemOk, modemError, 15000, 0, 
							"AT+USOCO=%d,\"%d.%d.%d.%d\",%d\r", 
							socketMask[0], 
							priServerIp[0], 
							priServerIp[1], 
							priServerIp[2], 
							priServerIp[3], 
							sysCfg.priDserverPort);
					sendGpsData_TaskPhase = TASK_GPRS_SERVER_CONNECT;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 10000, 0, "AT+UDNSRN=0,\"%s\"\r", sysCfg.priDserverName);
					DNS_serverIp[0] = 0;
					sendGpsData_TaskPhase = TASK_GPRS_WAIT_GOT_SERVER_IP;
					InitTimeout(&tTaskPhaseTimeOut,TIME_SEC(10));
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				createSocketConError++;
				if (serverConnectRetry++ < 3)
				{
					sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendGpsDataTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_WAIT_GOT_SERVER_IP:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (sizeof(DNS_serverIp) >= 7)
				{
					priServerIp = DNS_serverIp;
					ModemCmdTask_SendCmd(100, modemOk, modemError, 15000, 0, 
							"AT+USOCO=%d,\"%s\",%d\r", 
							socketMask[0], 
							priServerIp,
							sysCfg.priDserverPort);
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
					sendGpsData_TaskPhase = TASK_GPRS_SERVER_CONNECT;					
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendGpsDataTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_SERVER_CONNECT:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				tcpSocketStatus[0] = SOCKET_OPEN;
				gprsSendDataRetry = 0;
				if (gprs_Task == SEND_GPS_DATA_TASK)
				{
					InitTimeout(&tCollectDataTimeOut, TIME_MS(2));
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
					sendGpsData_TaskPhase = TASK_GPRS_COLLECT_DATA;
				}
				else if (gprs_Task == SEND_SOS_DATA_TASK)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
					sendGpsData_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					sendGpsData_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					SendGpsDataTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_COLLECT_DATA:
			if (CheckTimeout(&tCollectDataTimeOut) == TIMEOUT)
			{
				gprsDataLength = 0;
				if (CheckTrackerMsgIsReady())
				{
					while (GetTrackerMsg((char *)&gprsData[gprsDataLength], &i) == 0)
					{
						gprsDataLength += i;
						if(gprsDataLength >= 400)
						{
							break;
						}
						DelayMs(10);
					}
				}
				if((gprsDataLength > 0) && (gprsDataLength <  sizeof(gprsData)))
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
					sendGpsData_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
				else
				{
					InitTimeout(&tCollectDataTimeOut, TIME_MS(500));
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				gprsDataLength = 0;
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(1));
				sendGpsData_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
			}
			break;
			
		case TASK_GPRS_SEND_DATA_HEADER:
			if (ModemCmdTask_IsIdle())
			{
				if (gprs_Task == SEND_GPS_DATA_TASK)
				{
					ModemCmdTask_SendCmd(500, "@", modemError, 5000, 0, "AT+USOWR=%d,%d\r", socketMask[0], gprsDataLength);
				}
				else if (gprs_Task == SEND_SOS_DATA_TASK)
				{
					ModemCmdTask_SendCmd(500, "@", modemError, 5000, 0, "AT+USOWR=%d,%d\r", socketMask[0], sosDataLength);
				}
				ModemCmdTask_SetCriticalCmd();
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				sendGpsData_TaskPhase = TASK_GPRS_SEND_DATA;
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				SendGpsDataTask_Retry(1);
			}
			break;
			
		case TASK_GPRS_SEND_DATA:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				ModemCmdTask_SendCmd(100, modemOk, modemError, 5000, 0, NULL);
				if (gprs_Task == SEND_GPS_DATA_TASK)
				{
					for(i = 0; i < gprsDataLength ;i++)
					{
						COMM_Putc(gprsData[i]);
					}
				}
				else if (gprs_Task == SEND_SOS_DATA_TASK)
				{
					for (i = 0; i < sosDataLength; i++)
					{
						COMM_Putc(sosData[i]);
					}
				}
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
				sendGpsData_TaskPhase = TASK_GPRS_CHECK_ACK_SEND_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				SendGpsDataTask_Retry(1);
			}
			break;
			
		case TASK_GPRS_CHECK_ACK_SEND_DATA:
			if (ModemCmdTask_IsIdle() && (CheckTimeout(&tTaskPhaseTimeOut) != TIMEOUT))
			{
				if (CheckTimeout(&tCheckAckTimeOut) == TIMEOUT)
				{
					i = MODEM_CheckGPRSDataOut(DATA_SOCKET_NUM);
					if(i >= 1600)
					{
						SendGpsDataTask_Retry(1);
					}
					else if(i == 0) //data sent
					{
						if (gprs_Task == SEND_SOS_DATA_TASK)
						{
							sosDataLength = 0;
							if (SosTask_GetPhase() != SOS_END)
							{
								//sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
								InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
								sendGpsData_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
							}
							else
							{
								sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
							}
						}
						else
						{
							msgSentCnt++;
							if (msgSentCnt >= 3)
							{
								InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
								sendGpsData_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
							}
							else if (CheckTrackerMsgIsReady())
							{
								sendGpsData_TaskPhase = TASK_GPRS_COLLECT_DATA;
							}
							else
							{
								InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
								sendGpsData_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
							}
						}
					}
					else
					{
						InitTimeout(&tCheckAckTimeOut,TIME_MS(1000));
					}
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				if (gprsSendDataRetry++ < 3)
				{
					sendGpsData_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
				else
				{
					gprsSendDataRetry = 0;
					SendGpsDataTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_CHECK_RECV_DATA:
			if (CheckTimeout(&tCheckAckTimeOut) == TIMEOUT)
			{
				InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
				i = GPRS_CmdRecvData(mainBuf, sizeof(mainBuf), &rDataLength);
				if (i == 0xFF)
				{
					if (gprs_Task == SEND_GPS_DATA_TASK)
					{
						if (CheckTrackerMsgIsReady() && (msgSentCnt < 3))
						{
							sendGpsData_TaskPhase = TASK_GPRS_COLLECT_DATA;
						}
						else
						{
							sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
						}
					}
				}
				else if (i == 0) 		// socket 0
				{
					MODEM_Info("\r\nGPRS->DATA RECV:%dBYTES\r\n", rDataLength);
					mainBuf[rDataLength] = 0;
					if (IsGprsDataPacket(mainBuf, "@SINF", 5))
					{
						memcpy(mainBuf1, mainBuf, sizeof(mainBuf1));
						if (CMD_CfgParse((char*)&mainBuf1[5], &mainBuf[5], sizeof(mainBuf) - 5, &gprsDataLength, 0) == 0xA5)
						{
							// reset device
							NVIC_SystemReset();
						}
						else if (gprsDataLength)
						{
							mainBuf[0] = '@';
							mainBuf[1] = 'D';
							mainBuf[2] = 'I';
							mainBuf[3] = 'N';
							mainBuf[4] = 'F';
							DelayMs(200);
							gprsDataLength += 5;
							memcpy(gprsData, mainBuf, gprsDataLength);
							InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
							sendGpsData_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
						}
						else
						{
							sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
						}
					}
					else
					{
						sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
					}
				}
				else
				{
					sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				sendGpsData_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
			}
			break;

		case TASK_GPRS_SERVER_DISCONECT:
			for (i = 0; i < SOCKET_NUM; i++)
			{
				if(tcpSocketStatus[i] == SOCKET_OPEN)
				{
					tcpSocketStatus[i] = SOCKET_CLOSE;
					MODEM_DisconectSocket(i);
				}
			}
			COMM_Putc(0x1A);
			ModemCmdTask_SendCmd(1000, modemOk, modemError, 10000, 0, "AT+UPSDA=0,4\r");
			//ModemCmdTask_SetCriticalCmd();
			gprsProfileEnabled = 0;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(10));
			sendGpsData_TaskPhase = TASK_GSM_WAIT_GPRS_DISABLE;
			break;
		
		case TASK_GSM_WAIT_GPRS_DISABLE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				sendGpsData_TaskPhase = TASK_GSM_STOP;
			}
			break;
			
		case TASK_GSM_STOP:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				GprsTask_Stop();
			}
			break;
		
		case TASK_SYS_SLEEP:
			break;
			
		default:
			sendGpsData_TaskPhase = TASK_SYS_WAKEUP;
			break;
	}
	
	return 0;
}

GSM_PHASE_TYPE GsmTask_GetPhase(void)
{
	return gsm_TaskPhase;
}

void GsmTask_Init(void)
{
	gsm_TaskPhase = MODEM_TURN_ON;
}

void Gsm_Task(void)
{
	uint8_t lpwModeTemp;
	
	switch(gsm_TaskPhase)
	{
		case MODEM_TURN_ON:
			flagGsmStatus = GSM_NO_REG_STATUS;
			MODEM_Info("\r\nGSM->TURNING ON...\r\n");
			GSM_POWER_PIN_SET_OUTPUT;
			GSM_RESET_PIN_SET_OUTPUT;
			GSM_RTS_PIN_SET_OUTPUT;
			GSM_RTS_PIN_CLR;			
			GSM_RESET_PIN_SET; 
			GSM_POWER_PIN_SET;    // Turn on GSM
			DelayMs(5);           // Delay 5ms
			GSM_POWER_PIN_CLR;    // Turn off GSM
			DelayMs(200);         // Delay 200ms
			GSM_POWER_PIN_SET;    // Turn on GSM
			// ---------- Turn on GSM module ----------
			GSM_RESET_PIN_CLR;    // Reset GSM
			DelayMs(200);         // Delay 200ms
			GSM_RESET_PIN_SET;    // Start GSM module (Release reset)  
			lpwModeTemp = lpwMode;
			if (lpwMode != LPW_NOMAL)
			{
				SetLowPowerModes(LPW_NOMAL);
			}
			COMM_Open(UART1_BASE_PTR, periph_clk_khz, 115200);
			ModemCmdTask_SendCmd(10, modemOk, modemError, 1000, 3, "AT\r");
			InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
			gsm_TaskPhase = MODEM_INIT_COMM;
			break;
			
		case MODEM_INIT_COMM:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				MODEM_Info("GSM->AT:OK\n");
				COMM_Puts("AT+IPR=57600\r");
				MODEM_Info("GSM->Set baudrate is 57600\n");
				COMM_Open(UART1_BASE_PTR,periph_clk_khz,57600);
				ModemCmdTask_SendCmd(10, modemOk, modemError, 1000, 3, "AT\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_READ_IMEI;
			}
			else if (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT)
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
			
		case MODEM_WAIT_INIT_COMM:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (lpwModeTemp != LPW_NOMAL)
				{
					SetLowPowerModes(lpwModeTemp);
				}
				gsm_TaskPhase = MODEM_READ_IMEI;
			}
			else if (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT)
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
			
		case MODEM_READ_IMEI:
			// read modem IMEI
			GetIMEI(modemId, sizeof modemId);
			MODEM_Info("GSM-> IMEI:%s\n", modemId);
			if(strlen((char *)modemId) < 15) 
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			else
			{
				if((sysCfg.imei[0] == 0) || (strcmp((char*)sysCfg.imei, (char*)modemId) != 0))
				{
					strcpy((char *)sysCfg.imei, (char *)modemId);
					if(sysCfg.id[0] == 0)
					{
						strcpy((char *)sysCfg.id, (char *)modemId);
					}
					CFG_Save();
				}
				gsm_TaskPhase = MODEM_CONFIG_1;
			}
			break;
		
		case MODEM_CONFIG_1:			
			ringFlag = 0;
			//MODEM_SendCommand(modemOk,modemError,500,2,"AT+CLVL=100\r");
			//MODEM_SendCommand(modemOk,modemError,500,0,"AT+CTZU=1\r");
			ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CBC\r"); //Batery check status
			MODEM_Info("GSM->Battery Status:%d%\n", batteryPercent);
			InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
			gsm_TaskPhase = MODEM_CONFIG_2;
			break;
		
		case MODEM_CONFIG_2:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				MODEM_Info("\r\nGSM->DTE DISABLE\r\n");
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT&K0\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_CONFIG_3;
			}
			break;
		
		case MODEM_CONFIG_3:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				MODEM_Info("\r\nGSM->POWEWER SAVE MODE ENABLE:CONTROL BY RTS PIN\r\n");
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+UPSV=2\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_CONNECT_SIM;
// 				gsm_TaskPhase = MODEM_CONFIG_4;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
			
// 		case MODEM_CONFIG_4:
// 			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
// 			{
// 				GSM_DTR_PIN_SET;
// 				DelayMs(1000);
// 				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT&D2\r");
// 				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
// 				gsm_TaskPhase = MODEM_CONNECT_SIM;
// 			}
// 			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
// 			{
// 				// re-init modem
// 				gsm_TaskPhase = MODEM_TURN_ON;
// 			}
// 			break;
		
		case MODEM_CONNECT_SIM:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
// 				GSM_DTR_PIN_SET;
				// Set SMS format to text mode
				MODEM_Info("\r\nGSM->SMS SET TEXT MODE\r\n");
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CMGF=1\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_WAIT_CONNECT_SIM;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
				
		case MODEM_WAIT_CONNECT_SIM:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				// Set SMS DISPLAY  mode
				MODEM_SendCommand(modemOk,modemError,1000,1,"AT+CRSL=5\r");
				
				MODEM_Info("\r\nGSM->SET SMS DISPLAYMODE\r\n");
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CNMI=2,1,0,0,0\r");
				registerNetworkRetry = 0;
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(10));
				gsm_TaskPhase = MODEM_DELAY_REGISTER_NETWORK;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
			
		case MODEM_DELAY_REGISTER_NETWORK:
			if (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT)
			{
				gsm_TaskPhase = MODEM_REGISTER_NETWORK;
			}
			break;
			
		case MODEM_REGISTER_NETWORK:
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+COPS=0,2\r");
			InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
			gsm_TaskPhase = MODEM_SYS_WAIT_COVERAGE;
			break;
				
		case MODEM_SYS_WAIT_COVERAGE:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				MODEM_SendCommand(modemOk,modemError,500,0,"AT+CCLK?\r"); //get rtc
				MODEM_Info("\r\nGSM->AT+CLIP=1->(SHOW COMING PHONE NUMBER)\r\n");
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0," AT+CLIP=1\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_DELETE_ALL_SMS;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				if (registerNetworkRetry++ < 15)
				{
					gsm_TaskPhase = MODEM_REGISTER_NETWORK;
				}
				else
				{
					registerNetworkRetry = 0;
					// re-init modem
					gsm_TaskPhase = MODEM_TURN_ON;
				}
			}
			break;
		
		case MODEM_DELETE_ALL_SMS:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+CMGD=0,4\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_GET_CSQ;
			}
			break;
		
		case MODEM_GET_CSQ:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				MODEM_GetCSQ();
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(1));
				gsm_TaskPhase = MODEM_SET_GPRS_PROFILE;
			}
			break;
		
		case MODEM_SET_GPRS_PROFILE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 0, "AT+UPSD=0,0\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_SET_GPRS_APN;
			}
			break;
			
		case MODEM_SET_GPRS_APN:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				if (sysCfg.gprsApn[0])
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,1,\"%s\"\r", sysCfg.gprsApn);
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,1,\"%s\"\r", DEFAULT_GPSR_APN);
				}
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_SET_GPRS_USER;
			}
			break;
			
		case MODEM_SET_GPRS_USER:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (sysCfg.gprsUsr[0])
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,2,\"%s\"\r", sysCfg.gprsUsr);
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,2,\"%s\"\r", DEFAULT_GPRS_USR);
				}
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_SET_GPRS_PASS;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				gsm_TaskPhase = MODEM_WAIT_GPRS_PROFILE;
			}
			break;
			
		case MODEM_SET_GPRS_PASS:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				if (sysCfg.gprsPwd[0])
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,3,\"%s\"\r", sysCfg.gprsPwd);
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,3,\"%s\"\r", DEFAULT_GPRS_PWD);
				}
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_SET_GPRS_DYNAMIC_IP;
			}
			else if ((CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				gsm_TaskPhase = MODEM_WAIT_GPRS_PROFILE;
			}
			break;
			
		case MODEM_SET_GPRS_DYNAMIC_IP:
			if (ModemCmdTask_IsIdle())
			{
				ModemCmdTask_SendCmd(100, modemOk, modemError, 1000, 3, "AT+UPSD=0,7,\"0.0.0.0\"\r");
				InitTimeout(&tGsmTaskPhaseTimeOut, TIME_SEC(3));
				gsm_TaskPhase = MODEM_WAIT_GPRS_PROFILE;
			}
			else if (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT)
			{
				gsm_TaskPhase = MODEM_WAIT_GPRS_PROFILE;
			}
			break;
		
		case MODEM_WAIT_GPRS_PROFILE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tGsmTaskPhaseTimeOut) == TIMEOUT))
			{
				flagGsmStatus = GSM_REG_OK_STATUS;
				gsm_TaskPhase = MODEM_SYS_COVERAGE_OK;
			}
			break;
				
		case MODEM_SYS_COVERAGE_OK:
			if(sysCfg.imei[0] == '\0')
			{
				// re-init modem
				gsm_TaskPhase = MODEM_TURN_ON;
			}
			break;
			
		default:
			// re-init modem
			gsm_TaskPhase = MODEM_TURN_ON;
			break;
	}
}
