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
#include "download_config_task.h"

extern uint8_t *priServerIp;

extern uint8_t downloadRetry;
extern uint8_t tcpAckCheckTimes;
extern uint8_t tcpResendDataTimes;
extern uint32_t gprsDataLength;
extern uint8_t gprsData[560];
extern uint8_t mainBuf1[256];
extern uint8_t enableGprsRetry;
extern uint8_t serverConnectRetry;
extern uint8_t gprsSendDataRetry;
extern uint8_t registerNetworkRetry;
extern uint16_t cfgSize;
extern uint8_t createSocketConError;

extern Timeout_Type tTaskPhaseTimeOut;
extern Timeout_Type tCheckAckTimeOut;
extern Timeout_Type tCollectDataTimeOut;
extern Timeout_Type tGsmTaskPhaseTimeOut;

extern GPRS_TASK_TYPE gprs_Task;
extern uint8_t gprsProfileEnabled;

uint8_t downloadRetry = 0;
uint8_t waitingForDownloadRetry = 0;
uint8_t requestConfigRetry = 0;
uint8_t downloadSession;
uint16_t downloadOffset;
uint16_t downloadDataLen;
uint16_t expectedDownloadOffset = 0;
uint8_t downloadFinished = 0;
uint16_t lastDownloadOffset = 1;
uint16_t requestPacketRetry = 0;

Timeout_Type tDownloadConfigTimeout;

GPRS_PHASE_TYPE downConf_TaskPhase;

void DownloadConfigTask_Init(void)
{
	downloadRetry = 0;
	waitingForDownloadRetry = 0;
	requestConfigRetry = 0;
	downloadSession = rtcTimeSec;
	downloadOffset = 0;
	expectedDownloadOffset = 0;
	downloadDataLen = 0;
	downloadFinished = 0;
	lastDownloadOffset = 1;
	requestPacketRetry = 0;
	cfgSize = sysCfg.size;
	InitTimeout(&tDownloadConfigTimeout, TIME_SEC(300));			// 5 mins
	downConf_TaskPhase = TASK_SYS_SLEEP;
}

uint8_t DownloadConfigTask_Start(void)
{
	uint8_t result = 0;

	if ((gprs_Task != SEND_POWER_CMD_TASK) && (gprs_Task != SEND_SOS_DATA_TASK) &&
			(gprs_Task != DOWNLOAD_CONFIG_TASK) && (SosTask_GetPhase() == SOS_END))
	{
		GprsTask_Stop();
		gprs_Task = DOWNLOAD_CONFIG_TASK;
		if ((sysDownloadConfig == 2) || (CheckTimeout(&tDownloadConfigTimeout) == TIMEOUT))
		{
			sysDownloadConfig = 1;
			DownloadConfigTask_Init();
		}
		UploadConfigTask_StopAll();
		result = 1;
	}
	
	return result;
}

uint8_t DownloadConfigTask_IsWaitingForRetry(void)
{
	return waitingForDownloadRetry;
}

uint8_t DownloadConfigTask_IsWaitingForRetryTimedOut(void)
{
	if (CheckTimeout(&tDownloadConfigTimeout) == TIMEOUT)
	{
		return 1;
	}
	
	return 0;
}

GPRS_PHASE_TYPE DownloadConfigTask_GetPhase(void)
{
	return downConf_TaskPhase;
}

void DownloadConfigTask_SetPhase(GPRS_PHASE_TYPE taskPhase)
{
	downConf_TaskPhase = taskPhase;
}

void DownloadConfigTask_WakeUp(void)
{
	if (downConf_TaskPhase >= TASK_GPRS_SERVER_DISCONECT)
	{
		downConf_TaskPhase = TASK_SYS_WAKEUP;
	}
}

void DownloadConfigTask_Retry(int sec)
{
	if (IoTask_IsModemBusy() == 0)
	{
		downConf_TaskPhase = TASK_RETRY;
		InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(sec));
	}
	else
	{
		GprsTask_Stop();
	}
}

void DownloadConfigTask_Stop(void)
{
	downConf_TaskPhase = TASK_SYS_SLEEP;
}

void DownloadConfigTask_StopAll(void)
{
	DownloadConfigTask_Init();
	sysDownloadConfig = 0;
}

void DownloadConfigTask_WaitRetry(void)
{
	GprsTask_Stop();
	DownloadConfigTask_Init();
	waitingForDownloadRetry = 1;
	InitTimeout(&tDownloadConfigTimeout, TIME_SEC(300));				// 5 mins
}

uint8_t DownloadConfig_Task(void)
{
	uint32_t i;
	uint32_t rDataLength;
	uint8_t session;
	uint16_t size;
	uint16_t offset;
	
	if (gprsDeactivationFlag)
	{
		gprsDeactivationFlag = 0;
		if (downConf_TaskPhase != TASK_SYS_SLEEP)
		{
			downConf_TaskPhase = TASK_GSM_ENABLE;
		}
	}
	
	if (downConf_TaskPhase != TASK_SYS_SLEEP)
	{
		UploadConfigTask_StopAll();
	}
	
	switch(downConf_TaskPhase)
	{
		case TASK_SYS_WAKEUP:
			downloadRetry = 0;
			downConf_TaskPhase = TASK_WAIT_GSM_MODEM_INIT;
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(60));
			break;
		
		case TASK_RETRY:
			GSM_RTS_PIN_SET; /*Sleep module GSM*/
			if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				if (downloadRetry++ < 3)
				{
					//GsmTask_Init();
					downConf_TaskPhase = TASK_WAIT_RETRY;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					DownloadConfigTask_WaitRetry();
				}
			}
			break;
			
		case TASK_WAIT_RETRY:
			if (!ModemCmdTask_IsCriticalCmd())
			{
				if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					downConf_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
			}
			break;
		
		case TASK_WAIT_GSM_MODEM_INIT:
			if(GsmTask_GetPhase() == MODEM_SYS_COVERAGE_OK)
			{
				downConf_TaskPhase = TASK_GSM_GPRS_ENABLE;
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
					downConf_TaskPhase = TASK_GSM_GPRS_ENABLE;
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
						downConf_TaskPhase = TASK_GSM_GPRS_CHECK;
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
				downConf_TaskPhase = TASK_GSM_GPRS_OK;
			}
		break;
				
		case TASK_GSM_GPRS_CHECK:
			if (ModemCmdTask_GetPhase() == MODEM_CMD_OK)
			{
				gprsProfileEnabled = 1;
				flagGsmStatus = GSM_GPRS_OK_STATUS;
				downConf_TaskPhase = TASK_GSM_GPRS_OK;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (enableGprsRetry++ < 3)
				{
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(30));
					downConf_TaskPhase = TASK_GSM_GPRS_DISABLE;
				}
				else
				{
					GsmTask_Init();
					enableGprsRetry = 0;
					DownloadConfigTask_Retry(1);
				}
			}
			break;
			
		case TASK_GSM_GPRS_OK:
			serverConnectRetry = 0;
			downConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
			break;

		case TASK_GPRS_INIT_TCP_SOCKET:
			MODEM_GetSocket(0);
			ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCL=%d\r", socketMask[0]);
			InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
			downConf_TaskPhase = TASK_GPRS_CREAT_TCP_SOCKET;
			break;
		
		case TASK_GPRS_CREAT_TCP_SOCKET:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				socketMask[0] = 0xff;
				createSocketNow = 0;
				ModemCmdTask_SendCmd(100, modemOk, modemError, 3000, 0, "AT+USOCR=6\r");
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
				downConf_TaskPhase = TASK_GPRS_WAIT_TCP_SOCKET;
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
					downConf_TaskPhase = TASK_GPRS_SERVER_CONNECT;
					InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(15));
				}
				else
				{
					ModemCmdTask_SendCmd(100, modemOk, modemError, 10000, 0, "AT+UDNSRN=0,\"%s\"\r", sysCfg.priDserverName);
					DNS_serverIp[0] = 0;
					downConf_TaskPhase = TASK_GPRS_WAIT_GOT_SERVER_IP;
					InitTimeout(&tTaskPhaseTimeOut,TIME_SEC(10));
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				createSocketConError++;
				if (serverConnectRetry++ < 3)
				{
					downConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					DownloadConfigTask_Retry(1);
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
					downConf_TaskPhase = TASK_GPRS_SERVER_CONNECT;					
				}
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					downConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					DownloadConfigTask_Retry(1);
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
				downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				if (serverConnectRetry++ < 3)
				{
					downConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
				}
				else
				{
					serverConnectRetry = 0;
					DownloadConfigTask_Retry(1);
				}
			}
			break;
			
		case TASK_GPRS_COLLECT_DATA:
			if (CheckTimeout(&tCollectDataTimeOut) == TIMEOUT)
			{
				if (downloadOffset == lastDownloadOffset)
				{
					requestPacketRetry++;
				}
				else
				{
					requestPacketRetry = 0;
				}
				if (requestPacketRetry >= 5)
				{
					DownloadConfigTask_WaitRetry();
				}
				else
				{
					lastDownloadOffset = downloadOffset;
					cfgSize = sysCfg.size;
					downloadDataLen = cfgSize - downloadOffset;
					downloadDataLen = (downloadDataLen <= 256) ? downloadDataLen : 256;
					memcpy(mainBuf, (uint8_t*)&sysCfg + downloadOffset, downloadDataLen);
					gprsDataLength = 0;
					gprsDataLength = SCFG_Add(downloadSession, gprsData, cfgSize, downloadOffset);
					if((gprsDataLength > 0) && (gprsDataLength <  sizeof(gprsData)))
					{
						InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(3));
						downConf_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
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
				downConf_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
			}
			break;
			
		case TASK_GPRS_SEND_DATA_HEADER:
			if (ModemCmdTask_IsIdle())
			{
				ModemCmdTask_SendCmd(500, "@", modemError, 5000, 0, "AT+USOWR=%d,%d\r", socketMask[0], gprsDataLength);
				ModemCmdTask_SetCriticalCmd();
				InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
				downConf_TaskPhase = TASK_GPRS_SEND_DATA;
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				DownloadConfigTask_Retry(1);
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
				downConf_TaskPhase = TASK_GPRS_CHECK_ACK_SEND_DATA;
			}
			else if ((CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT) || (ModemCmdTask_GetPhase() == MODEM_CMD_ERROR))
			{
				DownloadConfigTask_Retry(1);
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
						DownloadConfigTask_Retry(1);
					}
					else if(i == 0) //data sent
					{
						if (downloadFinished == 0)
						{
							InitTimeout(&tCheckAckTimeOut, TIME_SEC(1));
							InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
							downConf_TaskPhase = TASK_GPRS_CHECK_RECV_DATA;
						}
						else
						{
							downConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
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
					downConf_TaskPhase = TASK_GPRS_SEND_DATA_HEADER;
				}
				else
				{
					gprsSendDataRetry = 0;
					DownloadConfigTask_Retry(1);
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
						downConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
					}
					else
					{
						downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
					}
				}
				else if (i == 0) 		// socket 0
				{
					MODEM_Info("\r\nGPRS->DATA RECV:%dBYTES\r\n", rDataLength);
					mainBuf[rDataLength] = 0;
					if (SCFG_Parser(mainBuf, gprsData, &session, &downloadDataLen, &size, &offset, rDataLength - 1))
					{
						if ((session == downloadSession) && (size == cfgSize) && (offset == expectedDownloadOffset))
						{
							requestConfigRetry = 0;
							APP_Write2FlashMem(CFG_SAVE_ADDR + downloadOffset, gprsData, downloadDataLen);
							expectedDownloadOffset = downloadOffset + downloadDataLen;
							if (expectedDownloadOffset == cfgSize)
							{
								APP_Read2FlashMem(CFG_SAVE_ADDR, mainBuf, sizeof(CONFIG_POOL));
								if(CFG_Check((CONFIG_POOL *)mainBuf))
								{
									// save new cfg
									memcpy(&sysCfg, mainBuf, sizeof(CONFIG_POOL));
									CFG_Save();
									downloadOffset = 0;
									expectedDownloadOffset = 0;
									sysUploadConfig = 0;
									sysDownloadConfig = 3;
									waitingForDownloadRetry = 0;
								}
								else
								{
									// config download failed
									downloadOffset = 0;
									expectedDownloadOffset = 0;
									if (requestConfigRetry++ < 5)
									{
										InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
										InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
										downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
									}
									else
									{
										DownloadConfigTask_WaitRetry();
									}
								}
							}
							else
							{
								downloadOffset = expectedDownloadOffset;
								InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
								downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
							}
						}
						else
						{
							if (offset == 0)
							{
								downloadOffset = 0;
							}
							if (requestConfigRetry++ < 5)
							{
								InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
								InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
								downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
							}
							else
							{
								DownloadConfigTask_WaitRetry();
							}
						}
					}
					else
					{
						if (requestConfigRetry++ < 5)
						{
							InitTimeout(&tCollectDataTimeOut, TIME_MS(10));
							InitTimeout(&tTaskPhaseTimeOut, TIME_SEC(5));
							downConf_TaskPhase = TASK_GPRS_COLLECT_DATA;
						}
						else
						{
							DownloadConfigTask_WaitRetry();
						}
					}
				}
				else
				{
					if (serverConnectRetry++ < 3)
					{
						downConf_TaskPhase = TASK_GPRS_INIT_TCP_SOCKET;
					}
					else
					{
						serverConnectRetry = 0;
						DownloadConfigTask_Retry(1);
					}
				}
			}
			else if (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT)
			{
				downConf_TaskPhase = TASK_GPRS_SERVER_DISCONECT;
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
			downConf_TaskPhase = TASK_GSM_WAIT_GPRS_DISABLE;
			break;
			
		case TASK_GSM_WAIT_GPRS_DISABLE:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				downConf_TaskPhase = TASK_GSM_STOP;
			}
			break;
			
		case TASK_GSM_STOP:
			if (ModemCmdTask_IsIdle() || (CheckTimeout(&tTaskPhaseTimeOut) == TIMEOUT))
			{
				GSM_RTS_PIN_SET; /*Sleep module GSM*/
				GprsTask_Stop();
			}
			break;
		
		case TASK_SYS_SLEEP:
			break;
			
		default:
			downConf_TaskPhase = TASK_SYS_WAKEUP;
			break;
	}
	
	return 0;
}
