/*
 * File:		LQRUG_uart_ex1.c
 * Purpose:		Main process
 *
 */

#include "common.h"
#include "start.h"

/********************************************************************/
int main (void)
{
#ifdef CMSIS  // If we are conforming to CMSIS, we need to call start here
	start();
#endif
	
	while (1);
}
/********************************************************************/
