#ifndef __GPRS_CMD_CTRL_H__
#define __GPRS_CMD_CTRL_H__
#include <stdint.h>

uint16_t DPWO_Add(char *data,uint32_t len);
uint16_t DGPS_Add(MSG_STATUS_RECORD *logRecordSend,char *data,uint32_t *len);
uint16_t DCLI_Add(MSG_STATUS_RECORD *logRecordSend,char *data,uint32_t *len);
uint32_t DSOS_Add(char *data, uint8_t sosAction, uint8_t sosSession);
uint16_t DSRT_Add(char *data,uint32_t len);
uint16_t DPWF_Add(char *data,uint32_t len);
uint16_t CFG_Add(uint8_t *header,uint8_t *buf,uint8_t *data,uint16_t dataLen,uint16_t cfgSize,uint16_t cfgOffset);
uint16_t CFG_Parser(uint8_t *header,uint8_t *buf,uint8_t *data,uint16_t *dataLen,uint16_t *cfgSize,uint16_t *cfgOffset);
uint16_t DCFG_Add(uint8_t session, uint8_t *buf, uint8_t *cfgData, uint16_t dataLen, uint16_t cfgSize, uint16_t cfgOffset);
uint8_t DCFG_Parser(uint8_t *buf, uint8_t *session, uint16_t *cfgSize, uint16_t *cfgOffset);
uint16_t SCFG_Add(uint8_t session, uint8_t *buf, uint16_t cfgSize, uint16_t cfgOffset);
uint8_t SCFG_Parser(uint8_t *buf, uint8_t *data, uint8_t *session, uint16_t *dataLen, uint16_t *cfgSize, uint16_t *cfgOffset, uint16_t receivedLen);
uint8_t IsGprsDataPacket(uint8_t *buf, char *header, uint8_t headerLen);

#endif

