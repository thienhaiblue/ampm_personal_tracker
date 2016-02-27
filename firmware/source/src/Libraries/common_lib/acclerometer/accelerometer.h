#ifndef __ACCELEROMETER_H__
#define __ACCELEROMETER_H__

#include <stdint.h>

#define MMA7660_ADR			(0x4C << 1)

/* REG MAP */
#define	XOUT_REG		0x00
#define YOUT_REG		0x01
#define ZOUT_REG		0x02
#define TILT_REG		0x03
#define SRST_REG		0x04
#define SPCNT_REG		0x05
#define INTSU_REG		0x06
#define	MODE_REG		0x07
#define	SR_REG			0x08
#define PDET_REG		0x09
#define PD_REG			0x0A

extern uint32_t acclInit;
extern uint8_t acclFlag;
uint8_t ACCL_Read(uint8_t reg);
uint8_t AccelerometerInit(void);
uint8_t ACCL_ReadStatus(void);
uint8_t accel_read(void);

#endif

