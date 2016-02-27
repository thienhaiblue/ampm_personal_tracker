

#ifndef __PEDOMETER_TASK_H__
#define __PEDOMETER_TASK_H__


typedef struct
{
      unsigned short stepCount;
      unsigned short lastStepCount;
      unsigned char updateReadyFlag;

}PEDOMETER_TYPE;

typedef struct __attribute__((packed)){
	int8_t x;
	int8_t y;
	int8_t z;
	uint8_t status;
	uint16_t fifo_samples;
	int16_t x16;
	int16_t y16;
	int16_t z16;
	int16_t temp;
	int16_t adc;
} XL362_Type;

typedef enum
{
	FACE_UP = 0,
	FACE_DOWN,
	SIDE_UP,
	SIDE_DOWN,
	OTHER_POSITION
}DEVICE_POSITION;

extern XL362_Type xl362;
extern PEDOMETER_TYPE Pedometer;
extern DEVICE_POSITION devicePosition;
void PedometerTaskInit(void);
void PedometerTask(void);

#endif

