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

#include <stdint.h>
#include "ambo_pedometer.h"

#define SAMPLE_NUM	50

uint16_t first_loop;
uint16_t step_cnt;
uint16_t cnt;
uint16_t prev_cnt;
uint16_t cnt_flg;
uint16_t cnt_trip;
uint16_t disp_trip;
uint16_t i;
uint16_t g_index;
uint16_t reset_time;
uint16_t length_g;
uint16_t pos_flg;
int16_t vcnt_up;
int16_t vcnt_dn;
uint16_t strt_trip;
uint16_t up_flg;
uint16_t dn_flg;
int16_t cnt_dn;
uint16_t up_flg;
uint16_t no_cnt_flg;

int16_t sample_x[SAMPLE_NUM];
int16_t sample_y[SAMPLE_NUM];
int16_t sample_z[SAMPLE_NUM];
int16_t sample_x_t[24];
int16_t sample_y_t[24];
int16_t sample_z_t[24];
int16_t g[38];
int16_t g_temp[12];
int16_t g_prev;

int16_t b_filter(int16_t *sample,int16_t *sample_t,uint16_t first_loop);
int16_t b2_filter(int16_t *sample,int16_t *sample_t,uint16_t first_loop);


const int16_t table[25] = {
-1601,
-873,
-208,
392,
930,
1405,
1817,
2165,
2450,
2672,
2830,
2925,
2957,
2925,
2830,
2672,
2450,
2165,
1817,
1405,
930,
392,
-208,
-873,
-1601
};


void ambo_pedometer_init(void)
{
	uint16_t index;
	for(index = 0;index < SAMPLE_NUM;index++)
	{
		sample_x[index] = 0;
		sample_y[index] = 0;
		sample_z[index] = 0;
	}
	index = 0;
	first_loop = 1;
	step_cnt = 0;
	cnt = 0;
	prev_cnt = 0;
	cnt_flg = 1;
	cnt_trip = 46;
	disp_trip = 0;
	i = 0;
	g_index = 0;
	reset_time = 99;
	length_g = 14;
	pos_flg = 1;
	vcnt_up = 0;
	vcnt_dn = 0;
	strt_trip = 0;
	up_flg = 0;
	dn_flg = 0;
}



uint16_t ambo_pedometer_sample_update(int16_t x,uint16_t y,uint16_t z)
{
	static uint16_t index = 0;
	sample_x[index] = x;
	sample_y[index] = y;
	sample_z[index] = z;
	index++;
	if(index >= SAMPLE_NUM)
	{
            index = 0x18;
            return 1;
	}
	return 0;
}



 uint16_t ambo_step_detect(void)
 {
	uint16_t j;
	int16_t i16Temp;
	
	b_filter(sample_x,sample_x_t,first_loop);
	b_filter(sample_y,sample_y_t,first_loop);
	b_filter(sample_z,sample_z_t,first_loop);
	//00449C
	for(j = 0;j < 0x1A;j++)
	{
		//Abs();
		if(sample_x[j] < 0)
		{
			sample_x[j] = -sample_x[j];
		}
		if(sample_y[j] < 0)
		{
			sample_y[j] = -sample_y[j];
		}
		if(sample_z[j] < 0)
		{
			sample_z[j] = -sample_z[j];
		}
		g[j + g_index] = sample_x[j] + sample_y[j]  + sample_z[j];
	}
	//004530
	b2_filter(g,g_temp,first_loop);
	
	if(first_loop)
	{
		g_prev = g[0];
	}
	for(j = 0;j < length_g;j++)
	{
		i16Temp = g[j] - g_prev;
		if(i16Temp >= 1)
		{
			pos_flg = 1;
		}
		//004570
		else if(i16Temp < 0)
		{
			pos_flg = 0;
		}
		//004586
		if((cnt_flg == 1) && (pos_flg == 1))
		{
			if((cnt_trip >= 0xC))
				if(!((vcnt_up < 0x5C)
				|| (cnt_trip >= 0x10 && vcnt_up < 0x48)
				|| (cnt_trip >= 0x14 && vcnt_up < 0x37)
				|| (cnt_trip >= 0x18 && vcnt_up < 0x2A)
				|| (cnt_trip >= 0x1E && vcnt_up < 0x22)
				))
				{
					cnt_flg = 0;
					cnt++;
					cnt_trip = 0;
					vcnt_up = 0;
					up_flg = 0;
					strt_trip = 1;
				}
		}
		//0045fc
		else if(cnt_flg == 0)
		{
			if((cnt_trip >= 6) && (vcnt_dn >= 4))
			{
				cnt_flg = 1;
				vcnt_dn = 0;
				dn_flg = 0;
			}
			//00461e
			else if(cnt_trip >= 0x3C) 
			{
				cnt_flg = 1;
				vcnt_dn = 0;
				dn_flg = 0;
			}
			
		}
		//004632
		if(pos_flg == 1 && cnt_flg ==1)
		{
			up_flg = 1;
		}
		//004644
		else if((pos_flg == 0) && (cnt_flg == 0))
		{
			dn_flg = 1;
			if(strt_trip == 1)
			{
				cnt_trip = 0;
				strt_trip = 0;
				vcnt_up = 0;
			}
		}
		//004666
		if(up_flg == 1)
		{
			vcnt_up = g[j] + vcnt_up - g_prev;
			
		}
		//004684
		else if(dn_flg == 1)
		{
			vcnt_dn = cnt_dn + g_prev - g[j];
		}
		//0046A0
		if(vcnt_up < 0)
		{
			vcnt_up = 0;
			up_flg = 0;
		}
		//0046B0
		else if(vcnt_dn < 0)
		{
			vcnt_dn = 0;
			dn_flg = 0;
		}
		//0046BE
		cnt_trip++;
		g_prev = g[j];
		if(prev_cnt == cnt)
		{
			no_cnt_flg = 1;
		}
		//0046DC
		else
		{
			no_cnt_flg = 0;
			prev_cnt = cnt;
		}
		//0046E6
		if(no_cnt_flg == 1)
		{
			if(cnt_trip > reset_time)
			{
				disp_trip = 0;
				cnt = step_cnt;
				reset_time = 0x63;
			}
		}
	}
	//00470A
	if(disp_trip >= 15)
	{
		step_cnt = cnt;
		reset_time = 0x55;
	}
	else
	{
		disp_trip++;
	}
	first_loop = 0;
	length_g = 0x1A;
	g_index = 0x0C;
	return step_cnt;
 }
 
int16_t i16Temp = 0;
int32_t i32Temp;
 
int16_t b2_filter(int16_t *sample,int16_t *sample_t,uint16_t first_loop)
{
	uint16_t i,j,k;
	if(first_loop == 1)
	{
		for(i = 0;i < 12;i++)
		{
			g_temp[i] = g[i + 14];
		}
		k = 26;
	}
	else
	{
		for(i = 0;i < 12;i++)
		{
			g[i] = g_temp[j];
			g_temp[i] = g[i + 26];
		}
		k = 38;
	}
	for(j = 12;j < k;j++)
	{
		i16Temp = 0;
		for(i = 0;i < 13;i++)
		{
			i32Temp = (int32_t)g[j - i] * 2520;
			i16Temp += (int16_t)(i32Temp>>16);
		}
		g[j-12] = i16Temp;
	}
	
	return 0;
}



int16_t b_filter(int16_t *sample,int16_t *sample_t,uint16_t first_loop)
{
	uint16_t i,j;
	
	if(first_loop == 1)
	{
		for(i = 0;i < 0x18;i++)
		{
			sample_t[i] = sample[0x1a + i];
		}
	}
	//4830
	else
	{
		for(i = 0;i < 0x18;i++)
		{	
			sample[i] = sample_t[i];
			sample_t[i] = sample[i + 0x1a];
		}
	}
	//004866
	for(j = 0x18;j < 0x32;j++)
	{
                i16Temp = 0;
		for(i = 0;i < 0x19;i++)
		{
				i32Temp = (int32_t)table[i] * (int32_t)sample[j - i];
				i16Temp += (int16_t)(i32Temp >> 16);
		}
		//00489A
		sample[j-0x18] = i16Temp;
	}
	return 0;
}
 
 
