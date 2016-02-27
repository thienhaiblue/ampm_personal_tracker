/* 

accelerometer apps 
Process information from accelerometer
and calculates tilt angle


*/
#include <math.h>
#include "common.h"
#include "sqrt16.h"
#include "median.h"
#include "asin.h"
#include "accelerometer.h"
#include "dbg_cfg_app.h"

signed short accel_x, accel_y, accel_z;
signed short resultx, resulty, resultz;

/*Acceleration  RAM */
signed int X_acc;
signed int Y_acc;
signed int Z_acc;

unsigned int xy_mag;
unsigned int xz_mag;
unsigned int yz_mag;

signed  int xy_angle;
signed  int xz_angle;
signed  int yz_angle;


struct tipo_mediana arr_medianas[3];
//unsigned char ADC_buffer[3];
unsigned int cat;
unsigned int accl_offset;
unsigned char fall_input;

#define ACCL_printf(...)//	DbgCfgPrintf(__VA_ARGS__)



/*
 The angle caluclation 
 
   mag = sqrt(X^2 + Y^2)
   
   sin(alpha) = CO/hip;
   
   alpha = asin(CO/hip);
   
   where CO  cateto opuesto
   hip = hipotenusa= magnitud
   the other way can be
   
   
   // to check what is better
   atan = y/x
   
   where x/y can take values from -inf to + inf
   
     and for´
     
     
   Note: Direct calculos based on  angle = asin(x/g) 
   is possible but is only valid when plane XY is the in the same line of G
   
   no accept small tilt of the board.
    

*/

void angle_calculation(void)
 {
   signed int nv, x2, y2, z2;

 /*  if (accel_count != 0) return;
   accel_count = 60; //60 msec
   */
   
   nv = (signed char)(resultx);
   X_acc = median(nv, &arr_medianas[0]);
   
   
   nv = (signed char)(resulty);
   Y_acc = median(nv, &arr_medianas[1]);
   
   nv = (signed char)(resultz);  
   Z_acc = median(nv, &arr_medianas[2]);

   x2        = X_acc*X_acc;
   y2        = Y_acc*Y_acc;

   xy_mag   = sqrt_16(x2 + y2);
  
   if (Y_acc<0) cat = -Y_acc; else cat = Y_acc;
   
   accl_offset = (unsigned int)(cat<<7)/(unsigned int)xy_mag;
   if (accl_offset>127) accl_offset = 127;    
   xy_angle = my_asin[accl_offset];
   
   if (Y_acc>0)  xy_angle = -xy_angle;
   
 
   /////////   
   
   z2        = Z_acc*Z_acc;   
   xz_mag    = sqrt_16(x2 + z2);
   if (X_acc<0) cat = -X_acc; else cat = X_acc;
   accl_offset = (unsigned int)(cat<<7)/(unsigned int)xz_mag;
   if (accl_offset>127) accl_offset = 127;    
   xz_angle = my_asin[accl_offset];
   
   if (X_acc>0)  xz_angle = -xz_angle;
   

   yz_mag    = sqrt_16(y2 + z2);
   if (Y_acc<0) cat = -Y_acc; else cat = Y_acc;
   accl_offset = (unsigned int)(cat<<7)/(unsigned int)yz_mag;
   if (accl_offset>127) accl_offset = 127;    
   yz_angle = my_asin[accl_offset];
   if (Y_acc>0)  yz_angle = -yz_angle;
    
 }


   /*Fall detection*/
#define FALL_LIMIT  45//50
void detect_fall_detection(void)
{
   if (xy_mag<FALL_LIMIT && xz_mag < FALL_LIMIT && yz_mag < FALL_LIMIT) 
    fall_input=1;
   else 
   {
     fall_input = 0;
    }
   
}

void accel_read(void)
{
	uint8_t res;
	res = ACCL_Read(0x00);
	if(res & 0x20)
		accel_x = (signed short)res - 64;
	else
		accel_x = res;
	res = ACCL_Read(0x01);
	if(res & 0x20)
		accel_y = (signed short)res - 64;
	else
		accel_y = res;
	res = ACCL_Read(0x02);
	if(res & 0x20)
		accel_z = (signed short)res - 64;
	else
		accel_z = res;
	ACCL_printf("%d:%d:%d\r\n",accel_x,accel_y,accel_z);
}


// void accel_read(void)
// {
//   
// 		accel_x   = hal_dev_mma8451_read_reg(0x01)<<8;
// 		accel_x  |= hal_dev_mma8451_read_reg(0x02);
// 		accel_x >>= 2;

// 		accel_y   = hal_dev_mma8451_read_reg(0x03)<<8;
// 		accel_y  |= hal_dev_mma8451_read_reg(0x04);
// 		accel_y >>= 2;

// 		accel_z   = hal_dev_mma8451_read_reg(0x05)<<8;
// 		accel_z  |= hal_dev_mma8451_read_reg(0x06);
// 		accel_z >>= 2;

// 		resultx   = hal_dev_mma8451_read_reg(0x01)<<8;
// 		resultx  |= hal_dev_mma8451_read_reg(0x02);
// 		resultx >>= 8;

// 		resulty   = hal_dev_mma8451_read_reg(0x03)<<8;
// 		resulty  |= hal_dev_mma8451_read_reg(0x04);
// 		resulty >>= 8;

// 		resultz   = hal_dev_mma8451_read_reg(0x05)<<8;
// 		resultz  |= hal_dev_mma8451_read_reg(0x06);
// 		resultz >>= 8;
//    
// 		angle_calculation(); //-900  to  900            
// 		detect_fall_detection();
// }



