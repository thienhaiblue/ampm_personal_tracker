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
#include "xl362.h"
#include "spi.h"
#include "tick.h"
#include "ambo_pedometer.h"
#include "pedometer_task.h"
#include "io_tasks.h"
#include "xl362_io.h"
#include "gps_task.h"
#include "system_config.h"
#include "gps_task.h"
#include "tracker.h"

#define ACCL_printf(...)	//DbgCfgPrintf(__VA_ARGS__)

#define FACE_X_LOWER_BOUND -30
#define FACE_X_UPPER_BOUND 30
#define FACE_Y_LOWER_BOUND -30
#define FACE_Y_UPPER_BOUND 30
#define FACE_UP_Z_LOWER_BOUND 60
#define FACE_UP_Z_UPPER_BOUND 80
#define FACE_DOWN_Z_LOWER_BOUND -80
#define FACE_DOWN_Z_UPPER_BOUND -45

// #define SIDE_X_LOWER_BOUND -40
// #define SIDE_X_UPPER_BOUND 40
// #define SIDE_UP_Y_LOWER_BOUND 60
// #define SIDE_UP_Y_UPPER_BOUND 80
// #define SIDE_DOWN_Y_LOWER_BOUND -80
// #define SIDE_DOWN_Y_UPPER_BOUND 60
#define SIDE_UP_X_LOWER_BOUND 55
#define SIDE_UP_X_UPPER_BOUND 80
#define SIDE_DOWN_X_LOWER_BOUND -80
#define SIDE_DOWN_X_UPPER_BOUND -55
#define SIDE_Y_LOWER_BOUND -30
#define SIDE_Y_UPPER_BOUND 30
#define SIDE_Z_LOWER_BOUND -30
#define SIDE_Z_UPPER_BOUND 45

PEDOMETER_TYPE Pedometer;
XL362_Type xl362;

uint32_t acclInit = 0;
uint8_t acclFlag = 0;
DEVICE_POSITION devicePosition;

uint8_t accelCheckManDown(int8_t accel_x,int8_t accel_y,int8_t accel_z)
{
	//convert side
	if (sysCfg.featureSet & FEATURE_MAN_DOWN_SET)
	{
		devicePosition = OTHER_POSITION;
		// check face up or down
		if ((accel_x >= FACE_X_LOWER_BOUND) && (accel_x <= FACE_X_UPPER_BOUND) && (accel_y >= FACE_Y_LOWER_BOUND) && (accel_y <= FACE_Y_UPPER_BOUND))
		{
			// face up
			if ((accel_z >= FACE_UP_Z_LOWER_BOUND) && (accel_z <= FACE_UP_Z_UPPER_BOUND))
			{
				devicePosition = FACE_UP;
			}
			// face down
			else if ((accel_z >= FACE_DOWN_Z_LOWER_BOUND) && (accel_z <= FACE_DOWN_Z_UPPER_BOUND))
			{
				devicePosition = FACE_DOWN;
			}
		}
		// check side up or down
		else if ((accel_y >= SIDE_Y_LOWER_BOUND) && (accel_y <= SIDE_Y_UPPER_BOUND) && (accel_z >= SIDE_Z_LOWER_BOUND) && (accel_z <= SIDE_Z_UPPER_BOUND))
		{
			// side up
			if ((accel_x >= SIDE_UP_X_LOWER_BOUND) && (accel_x <= SIDE_UP_X_UPPER_BOUND))
			{
				devicePosition = SIDE_UP;
			}
			// side down
			else if ((accel_x >= SIDE_DOWN_X_LOWER_BOUND) && (accel_x <= SIDE_DOWN_X_UPPER_BOUND))
			{
				devicePosition = SIDE_DOWN;
			}
		}
		
		if (devicePosition == OTHER_POSITION)
		{
			return 0;
		}
	}
	return 0xff;
}

void PedometerTaskInit(void)
{
	SPI_Init();
	xl362Init();
	Pedometer.lastStepCount = 0;
	Pedometer.stepCount = 0;
	Pedometer.updateReadyFlag = 0;
	ambo_pedometer_init();
}
void PedometerTask(void)
{
	uint32_t i;
	uint16_t x_cnt;
	uint16_t y_cnt;
	uint16_t z_cnt;
	int16_t x_axis[52];
	int16_t y_axis[52];
	int16_t z_axis[52];
	uint16_t fifoBuf[156];
	uint16_t samples;
	xl362Read(sizeof(XL362_Type),XL362_XDATA8,(uint8_t *)&xl362);
	
	xl362FifoRead(312,(uint8_t *)fifoBuf);
	if(xl362.fifo_samples == 512)
	{
		xl362FifoRead(312,(uint8_t *)fifoBuf);
	}
	
	x_cnt = 0;
	y_cnt = 0;
	z_cnt = 0;

	if(xl362.fifo_samples < 156)
		samples = xl362.fifo_samples;
	else
		samples = 156;
	for(i = 0;i < samples;i++)
	{
		switch(fifoBuf[i] & 0xc000)
		{
			case 0x0000:
					fifoBuf[i] <<= 4;
					x_axis[x_cnt] = (int16_t)fifoBuf[i];
					x_axis[x_cnt] >>= 3;
					x_cnt++;
			break;
			case 0x4000:
					fifoBuf[i] <<= 4;
					y_axis[y_cnt] = (int16_t)fifoBuf[i];
					y_axis[y_cnt] >>= 3;
					y_cnt++;
			break;
			case 0x8000:
					fifoBuf[i] <<= 4;
					z_axis[z_cnt] = (int16_t)fifoBuf[i];
					z_axis[z_cnt] >>= 3;
					z_cnt++;
			break;
			default:
				
			break;
		}
	}
	samples = x_cnt;
	if(x_cnt < y_cnt && x_cnt < z_cnt)
		samples = x_cnt;
	else if(y_cnt < z_cnt && y_cnt < x_cnt)
		samples = y_cnt;
	else if(z_cnt < y_cnt && z_cnt < x_cnt)
		samples = z_cnt;
	for(i = 0;i < samples;i++)
	{
		if(ambo_pedometer_sample_update(x_axis[i],y_axis[i],z_axis[i]) == 1)
		{
			Pedometer.stepCount = ambo_step_detect();
			if(Pedometer.stepCount != Pedometer.lastStepCount)
			{
				Pedometer.updateReadyFlag = 1;
				Pedometer.lastStepCount = Pedometer.stepCount;
			}
		}
	}
	
	if(movementToCheckGps) movementToCheckGps--;

	if(xl362.status & 0x40)
	{
		flagNoMovementReset = 1;
		flagManDownReset = 1;
		movementToCheckGps = 30;
		hasMoved = 1;
	}
	else
	{
		i = accelCheckManDown(xl362.x, xl362.y, xl362.z);
		if (i == 0)
		{
			flagNoMovementReset = 1;
			flagManDownReset = 1;
			movementToCheckGps = 30;
			hasMoved = 1;
		}
	}		
}
