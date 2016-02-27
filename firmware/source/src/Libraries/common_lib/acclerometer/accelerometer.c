#include "accelerometer.h"
#include "common.h"
#include "hal_i2c.h"
#include "dbg_cfg_app.h"
#include "system_config.h"

#define BIT_0	0x01
#define BIT_1	0x02
#define BIT_2	0x04
#define BIT_3	0x08
#define BIT_4	0x10
#define BIT_5	0x20
#define BIT_6	0x40
#define BIT_7	0x80

typedef enum
{
	FACE_UP = 0,
	FACE_DOWN,
	SIDE_UP,
	SIDE_DOWN,
	OTHER_POSITION
}
DEVICE_POSITION;

uint32_t acclInit = 0;
uint8_t acclFlag = 0;
DEVICE_POSITION devicePosition;

#define I2C0_B  I2C0_BASE_PTR

#define ACCL_printf(...)	DbgCfgPrintf(__VA_ARGS__)




#define DELTA_LIMIT 10
#define DELTA_LIMIT1 5

#define FACE_X_LOWER_BOUND -9
#define FACE_X_UPPER_BOUND 9
#define FACE_Y_LOWER_BOUND -9
#define FACE_Y_UPPER_BOUND 9
#define FACE_UP_Z_LOWER_BOUND 18
#define FACE_UP_Z_UPPER_BOUND 24
#define FACE_DOWN_Z_LOWER_BOUND -24
#define FACE_DOWN_Z_UPPER_BOUND -18

#define SIDE_X_LOWER_BOUND -9
#define SIDE_X_UPPER_BOUND 9
#define SIDE_UP_Y_LOWER_BOUND -24
#define SIDE_UP_Y_UPPER_BOUND -18
#define SIDE_DOWN_Y_LOWER_BOUND 18
#define SIDE_DOWN_Y_UPPER_BOUND 24
#define SIDE_Z_LOWER_BOUND -9
#define SIDE_Z_UPPER_BOUND 9

uint8_t accel_read(int8_t accel_x,int8_t accel_y,int8_t accel_z)
{
	uint8_t res;
	signed short delta_x,delta_y,delta_z;
	static signed short accel_x1, accel_y1, accel_z1;
	if(delta_x >= DELTA_LIMIT || delta_y >= DELTA_LIMIT || delta_z >= DELTA_LIMIT)
	{
		return 1;
	}
	if (sysCfg.featureSet & FEATURE_MAN_DOWN_SET)
	{
		// check if device has moved
		if(delta_x >= DELTA_LIMIT1 || delta_y >= DELTA_LIMIT1 || delta_z >= DELTA_LIMIT1)
		{		
			return 0;
		}
		
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
		else if ((accel_x >= SIDE_X_LOWER_BOUND) && (accel_x <= SIDE_X_UPPER_BOUND) && (accel_z >= SIDE_Z_LOWER_BOUND) && (accel_z <= SIDE_Z_UPPER_BOUND))
		{
			// side up
			if ((accel_y >= SIDE_UP_Y_LOWER_BOUND) && (accel_y <= SIDE_UP_Y_UPPER_BOUND))
			{
				devicePosition = SIDE_UP;
			}
			// side down
			else if ((accel_y >= SIDE_DOWN_Y_LOWER_BOUND) && (accel_y <= SIDE_DOWN_Y_UPPER_BOUND))
			{
				devicePosition = SIDE_DOWN;
			}
		}
		
		if (devicePosition == OTHER_POSITION)
		{
			return 0;
		}
	}
	//ACCL_printf("ACCEL:%d:%d:%d-ACCEL1:%d:%d:%d-DELTA:%d:%d:%d\r\n",accel_x,accel_y,accel_z,accel_x1,accel_y1,accel_z1,delta_x,delta_y,delta_z);
	return 0xff;
}


uint8_t ACCL_Write(uint8_t reg,uint8_t data)
{
	i2c_start(I2C0_B);
	i2c_write_byte(I2C0_B, MMA7660_ADR | I2C_WRITE);
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_write_byte(I2C0_B, reg);
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_write_byte(I2C0_B, data);
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_stop(I2C0_B);
	pause();
	return 0;
}
uint8_t ACCL_Read(uint8_t reg)
{
	uint8_t res;
	i2c_start(I2C0_B);
	i2c_write_byte(I2C0_B, MMA7660_ADR | I2C_WRITE);
	
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_write_byte(I2C0_B, reg);
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_repeated_start(I2C0_B);
	i2c_write_byte(I2C0_B, MMA7660_ADR | I2C_READ);
	i2c_wait(I2C0_B);
	i2c_get_ack(I2C0_B);

	i2c_set_rx_mode(I2C0_B);

	i2c_give_nack(I2C0_B);
	res = i2c_read_byte(I2C0_B);
	i2c_wait(I2C0_B);

	i2c_stop(I2C0_B);
	res = i2c_read_byte(I2C0_B);
	pause();
	return res;
}

uint8_t AccelerometerInit(void)
{
	uint8_t res;
	hal_i2c_init(I2C0_B);
	/*	Enter standby mode to configurate */
	ACCL_Write(MODE_REG, 0x00);
	res = ACCL_Read(MODE_REG);
	
	ACCL_Write(MODE_REG, 0x00);
	res = ACCL_Read(MODE_REG);
	if(res != 0) return 0xff;
	
	ACCL_Write(SPCNT_REG, 0x00);
	res = ACCL_Read(SPCNT_REG);
	if(res != 0) return 0xff;
	
	ACCL_Write(INTSU_REG, 0xe0);
	res = ACCL_Read(INTSU_REG);
	if(res != 0xe0) return 0xff;	
	
	ACCL_Write(PDET_REG, 0x01);
	res = ACCL_Read(PDET_REG);
	if(res != 0x01) return 0xff;	
	
	ACCL_Write(SR_REG, 0x02);
	res = ACCL_Read(SR_REG);
	if(res != 0x02) return 0xff;
	
	ACCL_Write(PD_REG, 0x00);
	res = ACCL_Read(PD_REG);
	if(res != 0x00) return 0xff;
	
	ACCL_Write(MODE_REG,0x01);
	res = ACCL_Read(MODE_REG);
	if(res != 0x01) return 0xff;
	acclFlag = 0;
	ACCL_ReadStatus();
	return 0;
}


uint8_t ACCL_ReadStatus(void)
{ 
	return ACCL_Read(TILT_REG);
}

