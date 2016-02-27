

#ifndef __AUDIO_H__
#define __AUDIO_H__
#include <stdint.h>

#define NOW_PLAY	0
#define TOP_INSERT 1
#define BOT_INSERT 2
#define STOP_ALL 3

typedef enum{
	AUDIO_GET_INFO = 0,
	AUDIO_PLAYING,
	AUDIO_STOP,
	AUDIO_STOPED
} AUDIO_STATUS_TYPE;


AUDIO_STATUS_TYPE AudioTask_GetPhase(void);
void AUDIO_Init(void);
void AudioControler(void);
void AudioPlayFile(uint8_t *fileName,uint8_t insertType);
void PlayVersion(char *versionStr, uint8_t insertType);
uint8_t AudioStopCheck(void);
void AudioStopAll(void);
void AudioEnable(void);

#endif
