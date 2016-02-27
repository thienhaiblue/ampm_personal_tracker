/** 
 * 	ambo_pedometer.h
 *  blue@ambo.com.vn
 *  AMBO TECH Inc.
 */

#ifndef __AMBO_PEDOMETER_H__
#define __AMBO_PEDOMETER_H__

#include <stdint.h>

#ifndef NULL
#define NULL							(0)
#endif
extern uint16_t step_cnt;
extern uint16_t disp_trip;
extern uint16_t cnt_flg;
extern uint16_t no_cnt_flg;
void ambo_pedometer_init(void);
uint16_t ambo_step_detect(void);
uint16_t ambo_pedometer_sample_update(int16_t x,uint16_t y,uint16_t z);
#endif
