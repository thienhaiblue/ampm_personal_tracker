/* 
 *
 * Copyright (C) AMPM ELECTRONICS EQUIPMENT TRADING COMPANY LIMITED.,
 * Email: thienhaiblue@ampm.com.vn or thienhaiblue@mail.com
 *
 * This file is part of ampm_open_lib
 *
 * ampm_open_lib is free software; you can redistribute it and/or modify.
 *
 * Add: 143/36/10 , Lien khu 5-6 street, Binh Hung Hoa B Ward, Binh Tan District , HCM City,Vietnam
 */
#include <stdint.h>
#include "common.h"
#include "ringbuf.h"
#include "project_common.h"
#include <string.h>
#include "dbg_cfg_app.h"
#include "hw_config.h"
#include "sysinit.h"
#include "power_management.h"
#include "audio.h"
#include "system_config.h"
#include "audio.h"

#define printfAudio(...)//	DbgCfgPrintf(__VA_ARGS__)

#define AUDIO_BUF_SIZE 64
#define MAX_SONG				20

uint8_t adcInit = 0;

typedef struct
{
	uint8_t fileName[4];
	uint8_t ptToNextSong;
} AUDIO_BUFF_TYPE;


typedef struct
{
	AUDIO_STATUS_TYPE status;
	uint32_t dataOffset;
	uint32_t fileOffset;
	uint32_t fileSize;
	uint32_t sampleRate;
	uint8_t *fileName;
	AUDIO_BUFF_TYPE	playList[MAX_SONG];
	uint8_t currentSong;
	uint8_t insertFlag;
	uint8_t *insertFileName;
	uint8_t insertType;
	uint8_t bufCnt;
	uint8_t bufLen;
	uint8_t buf[AUDIO_BUF_SIZE];
} AUDIO_TYPE;

AUDIO_TYPE audio;

RINGBUF audioPlayRingBuff;
uint8_t audioPlayBuf[128];


uint32_t ReadWavInfo(uint8_t *buf, uint32_t *offset);
uint32_t htons(uint8_t *data, uint32_t nbyte, uint32_t offset);

void DAC_Init(void)
{
	SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK;
	SIM_SCGC6 |= SIM_SCGC6_DAC0_MASK;
	PORTE_PCR30 = PORT_PCR_MUX(0);
	DAC0_DAT0H = 0x00;
	DAC0_DAT0L = 0x00;
	DAC0_C0 = 1 << 7; // enable DAC
	adcInit = 1;
}

void DAC_DeInit(void)
{
	SIM_SCGC6 |= SIM_SCGC6_DAC0_MASK;
	DAC0_C0 &= ~(1 << 7); // disable DAC
	SIM_SCGC6 &= ~SIM_SCGC6_DAC0_MASK;
	adcInit = 0;
}

void TPM1_Init(uint32_t clock/*Hz*/,uint32_t freq/*Hz*/)
{
	SIM_SCGC6 |= SIM_SCGC6_TPM1_MASK;
	TPM1_SC = 0; // disable timer
	TPM1_SC |= 1 << 7; // clear interrupt, prescaler divided by 1;
	TPM1_CNT = 0;
	TPM1_MOD = clock/freq - 1; // 8000Hz
	TPM1_SC |= 1 << 6; //enable timer overflow interrupt
	TPM1_CONF = TPM_CONF_TRGSEL(8);
	NVIC_SetPriority (TPM1_IRQn, (1<< 0) - 1);
	enable_irq(18);
	TPM1_SC |= TPM_SC_CMOD(1); // enable timer
}

void TPM1_DeInit(void)
{
	SIM_SCGC6 |= SIM_SCGC6_TPM1_MASK;
	TPM1_SC = 0;
	SIM_SCGC6 &= ~SIM_SCGC6_TPM1_MASK;
}
uint32_t tmp1_cnt,tmp1_cnt1;
void TPM1_IRQHandler(void)
{
	uint16_t data;
	uint8_t c;
	tmp1_cnt++;
	TPM1_SC |= 1 << 7; // clear interrup
	
//	RINGBUF_Get(&audioPlayRingBuff,&c);
//	data = 0;
//	DAC0_DAT0H = data >> 8;
//	DAC0_DAT0L = data;
	
	if(RINGBUF_Get(&audioPlayRingBuff,&c) == 0)
	{
		data = c;
		data *= 20;
		tmp1_cnt1++;
		DAC0_DAT0H = data >> 8;
		DAC0_DAT0L = data;
	}
//	else
//	{
//		data = 0;
//		DAC0_DAT0H = data >> 8;
//		DAC0_DAT0L = data;
//	}
}

void AUDIO_Init(void)
{
	uint32_t i;
	AUDIO_EN_INIT;
	audio.status = AUDIO_STOPED;
	for(i = 0;i <= MAX_SONG;i++ )
	{
		audio.playList[i].fileName[0] = NULL;
		audio.playList[i].ptToNextSong = MAX_SONG;
		audio.currentSong = 0;
	}
	RINGBUF_Init(&audioPlayRingBuff,audioPlayBuf,sizeof(audioPlayBuf));
	if((FRESULT)disk_initialize(MMC) == FR_OK)
	{
		f_mount(0, &sdfs);
	}
}

void AudioEnable(void)
{
	AUDIO_EN_SET;
}

void AudioStopAll(void)
{
	audio.insertType = STOP_ALL;
	audio.insertFlag = 1;
	while(audio.insertFlag);
}

uint8_t AudioStopCheck(void)
{
	if((audio.status == AUDIO_STOPED) && adcInit == 0)
		return 1;
	return 0;
}

AUDIO_STATUS_TYPE AudioTask_GetPhase(void)
{
	return audio.status;
}

void AudioPlayFile(uint8_t *fileName,uint8_t insertType)
{
	//DIS_AU_CMU_EN_CLR;
	SetLowPowerModes(LPW_NOMAL);
	DAC_Init();
	DelayMs(100);
	AudioEnable();
	audio.insertFileName = fileName;
	audio.insertType = insertType;
	audio.insertFlag = 1;
	while(audio.insertFlag);
}

void PlayVersion(char *versionStr, uint8_t insertType)
{
	int i;
	uint8_t c;
	uint8_t fileName[2];
	
	if (*versionStr != 0)
	{
		AudioPlayFile("6_7", insertType);
		for (i = 0; i < strlen(versionStr); i++)
		{
			c = versionStr[i];
			if ((c >= '0') && (c <= '9'))
			{
				fileName[0] = c;
				fileName[1] = 0;
				AudioPlayFile(fileName, insertType);
			}
			else if (c == '.')
			{
				AudioPlayFile("6_1", insertType);
			}
		}
	}
}

void AudioControler(void)
{
	uint32_t i,j;
	static FIL filFile;
	
	if(audio.insertFlag)
	{
		audio.insertFlag = 0;
		switch(audio.insertType)
		{
			case NOW_PLAY:
					strcpy((char *)audio.playList[audio.currentSong].fileName,(char *)audio.insertFileName);
					audio.status = AUDIO_STOPED;
			break;
			case TOP_INSERT:
					for(i = 0; i < MAX_SONG; i++)
					{
						if(audio.playList[i].fileName[0] == NULL)
						{
							strcpy((char *)audio.playList[i].fileName,(char *)audio.insertFileName);
							audio.playList[i].ptToNextSong = audio.playList[audio.currentSong].ptToNextSong;
							if(audio.playList[audio.currentSong].ptToNextSong != MAX_SONG)
							{
								audio.playList[audio.currentSong].ptToNextSong = i;		
								audio.playList[audio.currentSong].fileName[0] = NULL;
								audio.currentSong = audio.playList[audio.currentSong].ptToNextSong;
								if(audio.currentSong >= MAX_SONG)
								{
									audio.currentSong = 0;
									break;
								}
								audio.fileName = audio.playList[audio.currentSong].fileName;
								audio.status = AUDIO_GET_INFO;
							}
							break;
						}
					}
			break;
			
			case BOT_INSERT:
					j = audio.currentSong;
					for(i = 0; i < MAX_SONG; i++)
					{
						if(audio.playList[j].fileName[0] == NULL)
						{
							strcpy((char *)audio.playList[j].fileName,(char *)audio.insertFileName);
							audio.playList[j].ptToNextSong = MAX_SONG;
							break;
						}
						else if(audio.playList[j].ptToNextSong == MAX_SONG)
						{
							audio.playList[j].ptToNextSong = (j + 1) % MAX_SONG;
							j = (j + 1) % MAX_SONG;
							strcpy((char *)audio.playList[j].fileName,(char *)audio.insertFileName);
							audio.playList[j].ptToNextSong = MAX_SONG;
							break;
						}
						else
						{
							j = audio.playList[j].ptToNextSong;
						}
					}
			break;
			case STOP_ALL:
					TPM1_DeInit();
					DAC_DeInit();
					audio.status = AUDIO_STOPED;
					for(i = 0;i <= MAX_SONG;i++ )
					{
						audio.playList[i].fileName[0] = NULL;
						audio.playList[i].ptToNextSong = MAX_SONG;
						audio.currentSong = 0;
					}
				break;
			default:
				break;
		}
	}
	
	switch(audio.status)
	{
		case AUDIO_GET_INFO: 
		sprintf((char *)audio.buf,"audio/%02d/%s.wav",sysCfg.language,audio.fileName);
		f_close(&filFile);
		if(f_open(&filFile,(char *)audio.buf, FA_READ | FA_OPEN_EXISTING) == FR_OK)
		{
			if(f_read(&filFile,audio.buf,sizeof(audio.buf),&i) == FR_OK)
			{
				ReadWavInfo(audio.buf,&audio.dataOffset);
				audio.fileOffset = audio.dataOffset;
				audio.status = AUDIO_PLAYING;
				f_lseek(&filFile,audio.fileOffset);
				if(audio.fileSize >= (500*1024)) //500k
				{
					audio.status = AUDIO_STOP;
					break;
				}
				audio.bufCnt = 0;
				//DAC_Init();
				if(audio.sampleRate >= 4000 && audio.sampleRate <= 32000)
					TPM1_Init(mcg_clk_hz/*Hz*/,audio.sampleRate /*Hz*/);
				else
					audio.status = AUDIO_STOP;
				tmp1_cnt = 0;
				tmp1_cnt1 = 0;
			}
			else
				audio.status = AUDIO_STOP;
		}
		else
		{
			audio.status = AUDIO_STOP;
			audio.playList[audio.currentSong].fileName[0] = NULL;
			audio.playList[audio.currentSong].ptToNextSong = MAX_SONG;
		}
		break;
		case AUDIO_PLAYING:
			if(audio.bufCnt >= audio.bufLen)
			{
				if(f_read(&filFile,audio.buf,AUDIO_BUF_SIZE,&i) == FR_OK)
				{
					audio.bufLen = (uint8_t)i;
					audio.bufCnt = 0;
				}
				else
					audio.status = AUDIO_STOP;
			}
					
			while(RINGBUF_Put(&audioPlayRingBuff,audio.buf[audio.bufCnt]) == 0)
			{
				audio.bufCnt++;
				if(audio.bufCnt >= audio.bufLen)
					break;
			}
			if(f_tell(&filFile) >= f_size(&filFile))
			{
				audio.status = AUDIO_STOP;
			}
		break;
		case AUDIO_STOP:
			
			if((audio.playList[audio.currentSong].ptToNextSong != MAX_SONG))
			{
				audio.playList[audio.currentSong].fileName[0] = NULL;
				audio.currentSong = audio.playList[audio.currentSong].ptToNextSong;
				if(audio.currentSong >= MAX_SONG)
				{
					audio.currentSong = 0;
					break;
				}
				audio.fileName = audio.playList[audio.currentSong].fileName;
				audio.status = AUDIO_GET_INFO;
				break;
			}
			audio.playList[audio.currentSong].fileName[0] = NULL;
			TPM1_DeInit();
			DAC_DeInit();
			f_close(&filFile);
			audio.status = AUDIO_STOPED;
			DIS_AU_CMU_EN_CLR;
		break;
		case AUDIO_STOPED:
			if(audio.playList[audio.currentSong].fileName[0] != NULL)
			{
				audio.fileName = audio.playList[audio.currentSong].fileName;

				audio.status = AUDIO_GET_INFO;
				if(lpwMode != LPW_NOMAL)
				{
					SetLowPowerModes(LPW_NOMAL);
				}
				f_close(&filFile);	
				break;
			}
			audio.currentSong++;
			if(audio.currentSong >= MAX_SONG)
				audio.currentSong = 0;
		break;
		default:
			
		break;
	}
}

uint32_t ReadWavInfo(uint8_t *buf, uint32_t *offset)
{
	uint32_t res;
	
	*offset = 0;
	/* Start reading */
	/* Read first 4 bytes ChunkID "RIFF" */
	printfAudio("%c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);
	*offset += 4;

	/* Read ChunkSize */	
	res = htons(buf, 4, *offset);
	printfAudio("File size = %d\n", res + 8);
	*offset += 4;

	/* Read Format "WAVE" */
	printfAudio("%c%c%c%c\n", buf[*offset], buf[*offset+1], buf[*offset+2], buf[*offset+3]);
	*offset += 4;
	
	/* Read Subchunk1ID "fmt "*/
	printfAudio("%c%c%c%c\n", buf[*offset], buf[*offset+1], buf[*offset+2], buf[*offset+3]);
	*offset += 4;

	/* Read Subchunk1Size */
	res = htons(buf, 4, *offset);
	printfAudio("Subchunk1Size = %d\n", res + 8);
	*offset += 4;

	/* Read AudioFormat */
	res = htons(buf, 2, *offset);
	printfAudio("AudioFormat = %d\n", res);
	*offset += 2;

	/* Read NumChannels */
	res = htons(buf, 2, *offset);
	printfAudio("NumChannels = %d\n", res);
	*offset += 2;

	/* Read SampleRate */
	res = htons(buf, 4, *offset);
	printfAudio("SampleRate = %d\n", res);
	*offset += 4;
	audio.sampleRate = res;
	/* Read ByteRate */
	res = htons(buf, 4, *offset);
	printfAudio("ByteRate = %d\n", res);
	*offset += 4;

	/* Read BlockAlign */
	res = htons(buf, 2, *offset);
	printfAudio("BlockAlign = %d\n", res);
	*offset += 2;

	/* Read BitsPerSample */
	res = htons(buf, 2, *offset);
	printfAudio("BitsPerSample = %d\n", res);
	*offset += 2;
	
	*offset += 2; // advanced unused data
	/* Read Subchunk2ID "data" */
	printfAudio("%c%c%c%c\n", buf[*offset], buf[*offset+1], buf[*offset+2], buf[*offset+3]);
	*offset += 4;

	/* Read Subchunk2Size */
	res = htons(buf, 4, *offset);
	printfAudio("Subchunk2Size = %d\n", res);
	*offset += 4;
	
	return res;
}


uint32_t htons(uint8_t *data, uint32_t nbyte, uint32_t offset)
{
	uint32_t res = 0;
	int16_t i;

	nbyte = (nbyte > 4) ? 4 : nbyte;
	for (i = nbyte-1; i >= 0; i--)
	{
		res <<= 8;
		res |= data[i + offset];
	}

	return res;
}

