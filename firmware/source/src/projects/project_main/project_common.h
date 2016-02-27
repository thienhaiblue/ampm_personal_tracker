#ifndef __PROJECT_COMMON_H__
#define __PROJECT_COMMON_H__
#include <stdlib.h>
#include <stdint.h>
#include "ff.h"
#include "diskio.h"

extern uint8_t mainBuf[1280];
extern uint8_t flagPwrOn;
extern uint8_t flagPwrOff;

extern FATFS sdfs;

#endif
