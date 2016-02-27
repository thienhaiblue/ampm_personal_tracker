

#ifndef __POWER_MANAGEMENT_H__
#define __POWER_MANAGEMENT_H__
#include <stdint.h>

#define LPW_NONE	0xff
#define LPW_NOMAL 0
#define LPW_LEVEL_1 1
#define LPW_LEVEL_2 2
#define LPW_LEVEL_3	3

extern uint8_t lpwMode;
uint32_t SetLowPowerModes(uint8_t mode);
void LowPowerModes(uint8_t mode);

#endif


