#include "project_common.h"
#include "at_command_parser.h"
#include "hw_config.h"
#include "db.h"
#include "gprs_cmd_ctrl.h"
#include "sst25.h"
#include "sos_tasks.h"
#include "modem.h"
#include "io_tasks.h"
#include "gsm_gprs_tasks.h"
#include "upload_config_task.h"

extern uint8_t *priServerIp;

extern uint8_t uploadRetry;
extern uint8_t tcpAckCheckTimes;
extern uint8_t tcpResendDataTimes;
extern uint32_t gprsDataLength;
extern uint8_t gprsData[560];
extern uint8_t mainBuf1[256];
extern uint8_t enableGprsRetry;
extern uint8_t serverConnectRetry;
extern uint8_t gprsSendDataRetry;
extern uint8_t registerNetworkRetry;
extern uint8_t createSocketConError;

extern Timeout_Type tTaskPhaseTimeOut;
extern Timeout_Type tCheckAckTimeOut;
extern Timeout_Type tCollectDataTimeOut;
extern Timeout_Type tGsmTaskPhaseTimeOut;

extern GPRS_TASK_TYPE gprs_Task;
extern uint8_t gprsProfileEnabled;

uint8_t uploadRetry = 0;
uint8_t waitingForUploadRetry = 0;
uint8_t sendConfigRetry = 0;
uint8_t uploadSession;
uint16_t uploadOffset;
uint16_t uploadDataLen;
uint16_t cfgSize;
uint16_t expectedUploadOffset = 0;
uint16_t lastUploadOffset = 1;
uint8_t sendPacketRetry = 0;

Timeout_Type tUploadConfigTimeout;

GPRS_PHASE_TYPE upConf_TaskPhase;

void UploadConfigTask_Init(void)
{
	uploadRetry = 0;
	waitingForUploadRetry = 0;
	sendConfigRetry = 0;
	uploadSession = rtcTimeSec;
	uploadOffset = 0;
	expectedUploadOffset = 0;
	lastUploadOffset = 1;
	uploadDataLen = 0;
	sendPacketRetry = 0;
	cfgSize = sysCfg.size;
	InitTimeout(&tUploadConfigTimeout, TIME_SEC(300));			// 5 mins
	upConf_TaskPhase = TASK_SYS_SLEEP;
}

uint8_t UploadConfigTask_Start(void)
{
	uint8_t result = 0;
	
	if (sysUploadConfig)
	{
		if ((gprs_Task != SEND_POWER_CMD_TASK) && (gprs_Task != SEND_SOS_DATA_TASK) &&
				(gprs_Task != UPLOAD_CONFIG_TASK) && (SosTask_GetPhase() == SOS_END))
		{
			GprsTask_Stop();
			gprs_Task = UPLOAD_CONFIG_TASK;
			if ((sysUploadConfig == 2) || (CheckTimeout(&tUploadConfigTimeout) == TIMEOUT))
			{
				sysUploadConfig = 1;
				UploadConfigTask_Init();
			}
			result = 1;
		}
	}
	
	return result;
}

uint8_t UploadConfigTask_IsWaitingForRetry(void)
{
	return waitingForUploadRetry;
}

uint8_t UploadConfigTask_IsWaitingForRetryTimedOut(void)
{
	if (CheckTimeout(&tUploadConfigTimeout) == TIMEOUT)
	{
		return 1;
	}
	
	return 0;
}

GPRS_PHASE_TYPE UploadConfigTask_GetPhase(void)
{
	return upConf_TaskPhase;
}

void UploadConfigTask_SetPhase(GPRS_PHASE_TYPE taskPhase)
{
	upConf_TaskPhase = taskPhase;
}

void UploadConfigTask_WakeUp(void)
{
	if (upConf_TaskPhase >= TASK_GPRS_SERVER_DISCONECT)
	{
		upConf_TaskPhase = TASK_SYS_WAKEUP;
	}
}

void UploadConfigTask_Retry(int sec)
{
	if (IoTask_IsModemBusy() == 0)
	{
		upConf_TaskPhase = TASK_RETRY;
		InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(sec));
	}
	else
	{
		GprsTask_Stop();
	}
}

void UploadConfigTask_Stop(void)
{
	upConf_TaskPhase = TASK_SYS_SLEEP;
}

void UploadConfigTask_StopAll(void)
{
	UploadConfigTask_Init();
	sysUploadConfig = 0;
}

void UploadConfigTask_WaitRetry(void)
{
	GprsTask_Stop();
	UploadConfigTask_Init();
	waitingForUploadRetry = 1;
	InitTimeout(&tUploadConfigTimeout, TIME_SEC(300));
}

uint8_t UploadConfig_Task(void)
{
	uint32_t i;
	uint32_t rDataLength;
	uint8_t session;
	uint16_t size;
	uint16_t offset;
	
	if (gprsDeactivationFlag)
	{
		gprsDeactivationFlag = 0;
		if (upConf_TaskPhase != TASK_SYS_SLEEP)
		{
			upConf_TaskPhase = TASK_GSM_ENABLE;
		}
	}
	
	if (sysUploadConfig == 2)		// new config available
	{
		//GprsTask_StartTask(UPLOAD_CONFIG_TASK);
		sysUploadConfig = 1;
		uploadOffset = 0;
		expectedUploadOffset = 0;
		uploadSession = rtcTimeSec;
	}
	
	switch(upConf_TaskPhase)
	{
		case TASK_SYS_WAKEUP:
			uploadRetry = 0;
			upConf_TaskPhase = TASK_WAIT_GSM_MODEM_INIT;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(60));
			break;
		
		case TASK_RETRY:
			if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				if(MODEM_SendCommand("+CREG: 0,1",modemError,1000,3,"AT+CREG?\r"))
				{
					upConf_TaskPhase = TASK_GSM_GPRS_DISABLE;
					GsmTask_Init();
				}
				if (uploadRetry++ < 3)
				{
					//GsmTask_Init();
					upConf_TaskPhase = TASK_WAIT_RETRY;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					UploadConfigTask_WaitRetry();
				}
			}
			break;
			
		case TASK_WAIT_RETRY:
			if (!ModemCmdTask_IsCriticalCmd())
			{
				if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					upConf_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
			}
			break;
		
		case TASK_WAIT_GSM_MODEM_INIT:
			if(GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
			{
				upConf_TaskPhase = TASK_GSM_GPRS_ENABLE;
			}
			else if(CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				GprsTask_Stop();
			}
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
					upConf_TaskPhase = TASK_GSM_GPRS_ENABLE;
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
						upConf_TaskPhase = TASK_GSM_GPRS_CHECK;
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
				upConf_TaskPhase = TASK_GSM_GPRS_OK;
			}
		break;
				
		case TASK_GSM_GPRS_CHECK:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				gprsProfileEnabled = 1;
				flagGsmStatus = GSM_GPRS_OK_STATUS;
				upConf_TaskPhase = TASK_GSM_GPRS_OK;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (enableGprsRetry++ < 3)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					upConf_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
				else
				{
					enableGprsRetry = 0;
					UploadConfigTask_Retry(1);
				}
			}
			break;

		case TASK_GSM_GPRS_OK:
			serverConnectRetry = 0;
			upConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
			break;
			
		case TASK_GPRS_INIT_TCP_SOCKET:
			MODEM_GetSocket(0);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCL=%d\r", socketMask[0]);
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
			upConf_TaskPhase = TASK_GPRS_CREAT_TCP_SOCKET;
			break;
		
		case TASK_GPRS_CREAT_TCP_SOCKET:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				socketMask[0] = 0xff;
				createSocketNow = 0;
				ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCR=6\r");
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				upConf_TaskPhase = TASK_GPRS_WAIT_TCP_SOCKET;
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
					upConf_TaskPhase = TASK_GPRS_SERVER_CONNECT;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 10000, 0, "AT+UDNSRN=0,\"%s\"\r", sysCfg.priDserverName);
					DNS_serverIp[0] = 0;
					upConf_TaskPhase = TASK_GPRS_WAIT_GOT_SERVER_IP;
					InitTimeout(&tTaskPhaseTimeOut,TIME_SEC(10));
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				createSocketConError++;
				if (serverConnectRetry++ < 3)
				{
					upConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					UploadConfigTask_Retry(1);
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
					upConf_TaskPhase = TASK_GPRS_SERVER_CONNECT;					
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					upConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					UploadConfigTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_SERVER_CONNECT:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				tcpSocketStatus[0] = SOCKET_OPEN;
				gprsSendDataRetry = 0;
				InitTimeout(&tCollectDataTimeOut, TIME_MS(500));
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				upConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					upConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					UploadConfigTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_COLLECT_DATA:
			if (CheckTimeout(&tCollectDataTimeOut) == TIMEOUT)
			{
				if (uploadOffset == lastUploadOffset)
				{
					sendPacketRetry++;
				}
				else
				{
					sendPacketRetry = 0;
				}
				if (sendPacketRetry >= 5)
				{
					UploadConfigTask_WaitRetry();
				}
				else
				{
					lastUploadOffset = uploadOffset;
					cfgSize = sysCfg.size;
					uploadDataLen = cfgSize - uploadOffset;
					uploadDataLen = (uploadDataLen <= 256) ? uploadDataLen : 256;
					memcpy(mainBuf, (uint8_t*)&sysCfg + uploadOffset, uploadDataLen);
					gprsDataLength = 0;
					gprsDataLength = DCFG_Add(uploadSession, gprsData, mainBuf, uploadDataLen, cfgSize, uploadOffset);
					if((gprsDataLength > 0) && (gprsDataLength <  sizeof(gprsData)))
					{
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
						upConf_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
					}
					else
					{
						InitTimeout(&tCollectDataTimeOut, TIME_MS(500));
					}
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				gprsDataLength = 0;
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(1));
				upConf_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
			}
			break;
			
		case TASK_GPRS_SEND_DATA_HEADER:
			if (ModemCmdTask_IsIdle())
			{
				ModemCmdTask_SendCmd(500, "@", modemError, 5000, 0, "AT+USOWR=%d,%d\r", socketMask[0], gprsDataLength);
				ModemCmdTask_SetCriticalCmd();
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				upConf_TaskPhase = TASK_GPRS_SEND_DATA;
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				UploadConfigTask_Retry(1);
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
				upConf_TaskPhase = TASK_GPRS_CHECK_ACK_SEND_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				UploadConfigTask_Retry(1);
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
						UploadConfigTask_Retry(1);
					}
					else if (i == 0) //data sent
					{
						expectedUploadOffset = uploadOffset + uploadDataLen;
						InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
						upConf_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
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
					upConf_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
				else
				{
					gprsSendDataRetry = 0;
					UploadConfigTask_Retry(1);
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
					if(CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
					{
						upConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
					}
					else
					{
						upConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
					}
				}
				else if (i == 0) 		// socket 0
				{
					MODEM_Info("\r\nGPRS->DATA RECV:%dBYTES\r\n", rDataLength);
					mainBuf[rDataLength] = 0;
					if (DCFG_Parser(mainBuf, &session, &size, &offset))
					{
						if ((session == uploadSession) && (size == cfgSize) && (offset == expectedUploadOffset))
						{
							sendConfigRetry = 0;
							if (offset == cfgSize)
							{
								// send config successfully
								sysUploadConfig = 0;
								waitingForUploadRetry = 0;
								upConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
							}
							else
							{
								uploadOffset = expectedUploadOffset;
								InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
								upConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
							}
						}
						else
						{
							if (offset == 0)
							{
								uploadOffset = 0;
							}
							if (sendConfigRetry++ < 5)
							{
								InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
								upConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
							}
							else
							{
								UploadConfigTask_WaitRetry();
							}
						}
					}
					else
					{
						if (sendConfigRetry++ < 5)
						{
							InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
							InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
							upConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
						}
						else
						{
							UploadConfigTask_WaitRetry();
						}
					}
				}
				else
				{
					if (serverConnectRetry++ < 3)
					{
						upConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
					}
					else
					{
						serverConnectRetry = 0;
						UploadConfigTask_Retry(1);
					}
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				upConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
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
			upConf_TaskPhase = TASK_GSM_WAIT_GPRS_DISABLE;
			break;
			
		case TASK_GSM_WAIT_GPRS_DISABLE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				upConf_TaskPhase = TASK_GSM_STOP;
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
			upConf_TaskPhase = TASK_SYS_WAKEUP;
			break;
	}
	
	return 0;
}
