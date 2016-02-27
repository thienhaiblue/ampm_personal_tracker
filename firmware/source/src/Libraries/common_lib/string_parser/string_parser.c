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
#include "tick.h"
#include "string_parser.h"


void StrParserInit(STR_PARSER_Type *process,uint8_t *numCharParsed,uint8_t strParserCntMax,uint16_t dataLengthMax)
{
	uint8_t i;
	process->state = STR_FINISH;
	process->dataRecvLength = 0;
	process->strParserCntMax = strParserCntMax;
	process->timeout = 0;
	process->timeoutSet = 3;// 3sec
	process->dataRecvMaxLength = dataLengthMax;
	for(i = 0; i < process->strParserCntMax;i++)
	{
		numCharParsed[i] = 0;
	}
}

void StrParserCtl(STR_PARSER_Type *process,uint8_t *numCharParsed)
{	
	uint8_t i;
	if(process->state == STR_FINISH) 
		process->timeout = TICK_Get();
	else
	{
		if(TICK_Get() - process->timeout >= TIME_SEC(process->timeoutSet))
		{
			process->timeout = TICK_Get();
			process->state = STR_FINISH;
			for(i = 0; i < process->strParserCntMax;i++)
			{
				numCharParsed[i] = 0;
			}
		}
	}
}

void StrComnandParser(const STR_INFO_Type *info,STR_PARSER_Type *process,uint8_t *numCharParsed,uint8_t c)
{
	uint32_t i; 
	static uint8_t lastChar = 0;
	StrParserCtl(process,numCharParsed);
	switch(process->state)
	{
		case STR_FINISH:
			for(i = 0; i < process->strParserCntMax;i++)
			{
				if(c == info[i].strInfo[numCharParsed[i]])
				{
						lastChar = c;
						numCharParsed[i]++;
						if(info[i].strInfo[numCharParsed[i]] == '\0')
						{
								process->strInProcess = i;
								process->state = STR_WAIT_FINISH;
								process->dataRecvLength = 0;
						}
				}
				else if(lastChar == info[i].strInfo[0])
				{
						numCharParsed[i] = 1;
				}
				else
					numCharParsed[i] = 0;
			}
		break;
		case STR_WAIT_FINISH:				
			if(info[process->strInProcess].func(c) == 0)
			{
				process->state = STR_FINISH;
				process->timeoutSet = 3;
				for(i = 0; i < process->strParserCntMax;i++)
				{
					numCharParsed[i] = 0;
				}
			}
			process->dataRecvLength++;
			if(process->dataRecvLength >= process->dataRecvMaxLength)
			{
				process->state = STR_FINISH;
				for(i = 0; i <  process->strParserCntMax;i++)
				{
					numCharParsed[i] = 0;
				}
			}
			break;
		default:
			process->state = STR_FINISH;
			for(i = 0; i <  process->strParserCntMax;i++)
			{
				numCharParsed[i] = 0;
			}
			break;
	}	
}

