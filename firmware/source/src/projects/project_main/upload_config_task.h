#ifndef _UPLOAD_CONFIG_TASK_H_
#define _UPLOAD_CONFIG_TASK_H_

void UploadConfigTask_Init(void);
uint8_t UploadConfigTask_Start(void);
uint8_t UploadConfigTask_IsWaitingForRetry(void);
uint8_t UploadConfigTask_IsWaitingForRetryTimedOut(void);
GPRS_PHASE_TYPE UploadConfigTask_GetPhase(void);
void UploadConfigTask_SetPhase(GPRS_PHASE_TYPE taskPhase);
void UploadConfigTask_WakeUp(void);
void UploadConfigTask_Stop(void);
void UploadConfigTask_StopAll(void);
uint8_t UploadConfig_Task(void);

#endif
