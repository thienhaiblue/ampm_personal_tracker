/*
 * File:		uart.h
 * Purpose:     Provide common ColdFire uart routines for polled serial IO
 *
 * Notes:
 */

#ifndef __uart_H__
#define __uart_H__
#include "common.h"
#include "MemMapPtr_KL25Z4.h"
#include <stdio.h>
#include "ringbuf.h"
#include "gps.h"

#define USE_UART0_RINGBUFF
#define USE_UART1_RINGBUFF

#ifdef USE_UART0_RINGBUFF
extern RINGBUF USART0_RxRingBuff;
#endif

#ifdef USE_UART1_RINGBUFF
extern RINGBUF USART1_RxRingBuff;
#endif

/********************************************************************/


void uart_init (UART_MemMapPtr uartch, uint32 sysclk, uint32 baud);
char uart_getchar (UART_MemMapPtr channel);
void uart_putchar (UART_MemMapPtr channel, char ch);
void uart_putstr(UART_MemMapPtr channel, uint8 *s);
int uart_getchar_present (UART_MemMapPtr channel);
void uart0_init (UART0_MemMapPtr uartch, uint32 sysclk, uint32 baud);
char uart0_getchar (UART0_MemMapPtr channel);
void uart0_putchar (UART0_MemMapPtr channel, char ch);
void uart0_putstr(UART0_MemMapPtr channel, uint8 *s);
int uart0_getchar_present (UART0_MemMapPtr channel);

/********************************************************************/

#endif /* __uart_H__ */
