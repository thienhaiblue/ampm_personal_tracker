#ifndef __DBG_CFG_APP_H__
#define __DBG_CFG_APP_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "usbreg.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "hid.h"
#include "hiduser.h"
#include "uart.h"
#include "ringbuf.h"



									
#define USB_PACKET_SIZE		36
#define UART_PACKET_SIZE		36
#define FLASH_PACKET_SIZE		32
#define GPRS_PACKET_SIZE	516


typedef struct
{
	uint32_t packetNo;
	uint16_t packetLengthToSend;
	uint8_t  *dataPt;
} CFG_DATA_TYPE;

typedef enum{
	CFG_CMD_NEW_STATE,
	CFG_CMD_GET_LENGTH,
	CFG_CMD_GET_OPCODE,
	CFG_CMD_GET_DATA,
	CFG_CMD_CRC_CHECK,
	CFG_CMD_WAITING_SATRT_CODE
}CFG_CMD_STATE_TYPE;


typedef struct
{
	uint8_t start;
	uint16_t length;
	uint8_t opcode;
	uint8_t *dataPt;
	uint8_t crc;
} CFG_PROTOCOL_TYPE;

typedef struct
{
	CFG_CMD_STATE_TYPE state;
	uint16_t len;
	uint16_t lenMax;
	uint16_t cnt;
	uint8_t opcode;
	uint8_t crc;
} PARSER_PACKET_TYPE;


extern uint32_t flagCalling;

extern uint32_t firmwareStatus;

extern uint32_t firmwareFileSize;

extern uint32_t firmwareFileOffSet;
 
void LoadFirmwareFile(void);
uint8_t CfgParserPacket(PARSER_PACKET_TYPE *parserPacket,CFG_PROTOCOL_TYPE *cfgProtoRecv,uint8_t c);
uint8_t CfgProcessData(CFG_PROTOCOL_TYPE *cfgProtoRecv,CFG_PROTOCOL_TYPE *cfgProtoSend,uint8_t *cfgSendDataBuff,uint32_t maxPacketSize,uint8_t logSendEnable);
uint32_t  DbgCfgPrintf(const uint8_t *format, ...);
uint32_t DbgCfgPutChar(uint8_t c);
uint8_t CfgCalcCheckSum(uint8_t *buff, uint32_t length);
uint8_t APP_Write2FlashMem(uint32_t addr,uint8_t *buf, uint16_t len);
uint8_t APP_Read2FlashMem(uint32_t addr,uint8_t *buf, uint16_t len);

extern uint32_t enableUSB_Manager;
void TPM0_Init(uint32_t clock/*Hz*/,uint32_t freq/*Hz*/);
void vHID_Task(void);
void FlashReadWriteCtrl(void);
void TPM0_DeInit(void);
#endif

