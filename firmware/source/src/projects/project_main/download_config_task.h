#ifndef _DOWNLOAD_CONFIG_TASK_H_
#define _DOWNLOAD_CONFIG_TASK_H_

void DownloadConfigTask_Init(void);
uint8_t DownloadConfigTask_Start(void);
uint8_t DownloadConfigTask_IsWaitingForRetry(void);
uint8_t DownloadConfigTask_IsWaitingForRetryTimedOut(void);
GPRS_PHASE_TYPE DownloadConfigTask_GetPhase(void);
void DownloadConfigTask_SetPhase(GPRS_PHASE_TYPE taskPhase);
void DownloadConfigTask_WakeUp(void);
void DownloadConfigTask_Stop(void);
uint8_t DownloadConfig_Task(void);

#endif
