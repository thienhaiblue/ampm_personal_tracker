/*
 * File:        uart.c
 * Purpose:     Provide common uart routines for serial IO
 *
 * Notes:       
 *              
 */

#include "common.h"
#include "uart.h"
#include <stdarg.h>
#include "gps.h"
#include "at_command_parser.h"

#ifdef USE_UART0_RINGBUFF
uint8_t USART0_RxBuff[64] = {0};
RINGBUF USART0_RxRingBuff;
#endif

#ifdef USE_UART1_RINGBUFF
uint8_t USART1_RxBuff[256] = {0};
RINGBUF USART1_RxRingBuff;
#endif

extern RINGBUF comRingBuf;
extern uint8_t logEnable;

/********************************************************************/
/*
 * Initialize the uart for 8N1 operation, interrupts disabled, and
 * no hardware flow-control
 *
 * NOTE: Since the uarts are pinned out in multiple locations on most
 *       Kinetis devices, this driver does not enable uart pin functions.
 *       The desired pins should be enabled before calling this init function.
 *
 * Parameters:
 *  uartch      uart channel to initialize
 *  sysclk      uart module Clock in kHz(used to calculate baud)
 *  baud        uart baud rate
 */
void uart_init (UART_MemMapPtr uartch, uint32 sysclk, uint32 baud)
{
    register uint16 sbr;
    uint8 temp;
			NVIC_DisableIRQ(UART1_IRQn); 
			#ifdef USE_UART1_RINGBUFF
			RINGBUF_Init(&USART1_RxRingBuff,USART1_RxBuff,sizeof(USART1_RxBuff));
			#endif
          
			//GT1022
			PORTC_PCR3 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x3);
			PORTC_PCR4 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x3); 
			
      if (uartch == UART1_BASE_PTR)
        SIM_SCGC4 |= SIM_SCGC4_UART1_MASK;
      else
				SIM_SCGC4 |= SIM_SCGC4_UART2_MASK;
      /* Make sure that the transmitter and receiver are disabled while we 
       * change settings.
       */
      UART_C2_REG(uartch) &= ~(UART_C2_TE_MASK
				| UART_C2_RE_MASK );

      /* Configure the uart for 8-bit mode, no parity */
      UART_C1_REG(uartch) = 0;	/* We need all default settings, so entire register is cleared */
    
      /* Calculate baud settings */
      sbr = (uint16)((sysclk*1000)/(baud * 16));
        
      /* Save off the current value of the uartx_BDH except for the SBR field */
      temp = UART_BDH_REG(uartch) & ~(UART_BDH_SBR(0x1F));
    
      UART_BDH_REG(uartch) = temp |  UART_BDH_SBR(((sbr & 0x1F00) >> 8));
      UART_BDL_REG(uartch) = (uint8)(sbr & UART_BDL_SBR_MASK);
  
      /* Enable receiver and transmitter */
      UART_C2_REG(uartch) |= (UART_C2_TE_MASK
	    		  | UART_C2_RE_MASK );
			
			NVIC_EnableIRQ   (UART1_IRQn); 
			NVIC_SetPriority (UART1_IRQn, (1<< 1) - 1);
			//enable interrupt
			UART_C2_REG(uartch) |= UART_C2_RIE_MASK;
    
}
/********************************************************************/
/*
 * Wait for a character to be received on the specified uart
 *
 * Parameters:
 *  channel      uart channel to read from
 *
 * Return Values:
 *  the received character
 */
char uart_getchar (UART_MemMapPtr channel)
{
      /* Wait until character has been received */
      while (!(UART_S1_REG(channel) & UART_S1_RDRF_MASK));
    
      /* Return the 8-bit data from the receiver */
      return UART_D_REG(channel);
}
/********************************************************************/
/*
 * Wait for space in the uart Tx FIFO and then send a character
 *
 * Parameters:
 *  channel      uart channel to send to
 *  ch			 character to send
 */ 
void uart_putchar (UART_MemMapPtr channel, char ch)
{
	/* Wait until space is available in the FIFO */
	while(!(UART_S1_REG(channel) & UART_S1_TDRE_MASK));

	/* Send the character */
	UART_D_REG(channel) = (uint8)ch;
 }


/********************************************************************/
/*
 * Wait for space in the uart Tx FIFO and then send a character
 *
 * Parameters:
 *  channel      uart channel to send to
 *  ch			 character to send
 */ 
void uart_putstr(UART_MemMapPtr channel, uint8 *s)
{
  while(*s)
	{
		uart_putchar(channel,*s++);
	}
 }
/********************************************************************/
/*
 * Check to see if a character has been received
 *
 * Parameters:
 *  channel      uart channel to check for a character
 *
 * Return values:
 *  0       No character received
 *  1       Character has been received
 */
int uart_getchar_present (UART_MemMapPtr channel)
{
    return (UART_S1_REG(channel) & UART_S1_RDRF_MASK);
}
/********************************************************************/

/***************************************************************************
 * Begin UART0 functions
 **************************************************************************/
/********************************************************************/
/*
 * Initialize the uart for 8N1 operation, interrupts disabled, and
 * no hardware flow-control
 *
 * NOTE: Since the uarts are pinned out in multiple locations on most
 *       Kinetis devices, this driver does not enable uart pin functions.
 *       The desired pins should be enabled before calling this init function.
 *
 * Parameters:
 *  uartch      uart channel to initialize
 *  sysclk      uart module Clock in kHz(used to calculate baud)
 *  baud        uart baud rate
 */
void uart0_init (UART0_MemMapPtr uartch, uint32 sysclk, uint32 baud)
{
    uint8 i;
    uint32 calculated_baud = 0;
    uint32 baud_diff = 0;
    uint32 osr_val = 0;
    uint32 sbr_val, uart0clk;
    uint32 baud_rate;
    uint32 reg_temp = 0;
    uint32 temp = 0;
		NVIC_DisableIRQ(UART0_IRQn); 
		#ifdef USE_UART0_RINGBUFF
		RINGBUF_Init(&USART0_RxRingBuff,USART0_RxBuff,sizeof(USART0_RxBuff));
		#endif
#ifdef FRDM_BOARD
		SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;
		PORTA_PCR1 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x2);
		PORTA_PCR2 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x2);  
#else
		SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;
		PORTB_PCR16 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(3);
		PORTB_PCR17 = PORT_PCR_ISF_MASK | PORT_PCR_MUX(3); 
#endif
		SIM_SOPT2 |= SIM_SOPT2_UART0SRC(1); // select the PLLFLLCLK as UART0 clock source
    SIM_SCGC4 |= SIM_SCGC4_UART0_MASK;
	
		NVIC_EnableIRQ   (UART0_IRQn); 
		NVIC_SetPriority (UART0_IRQn, (1<< 1) - 1);
    
    // Disable UART0 before changing registers
    UART0_C2 &= ~(UART0_C2_TE_MASK | UART0_C2_RE_MASK);
  
    // Verify that a valid clock value has been passed to the function 
    if ((sysclk > 50000) || (sysclk < 32))
    {
        sysclk = 0;
        reg_temp = SIM_SOPT2;
        reg_temp &= ~SIM_SOPT2_UART0SRC_MASK;
        reg_temp |= SIM_SOPT2_UART0SRC(0);
        SIM_SOPT2 = reg_temp;
			
			  // Enter inifinite loop because the 
			  // the desired system clock value is 
			  // invalid!!
			  while(1)
				{}
    }
    
//     // Verify that a valid value has been passed to TERM_PORT_NUM and update
//     // uart0_clk_hz accordingly.  Write 0 to TERM_PORT_NUM if an invalid 
//     // value has been passed.  
//     if (TERM_PORT_NUM != 0)
//     {
//         reg_temp = SIM_SOPT2;
//         reg_temp &= ~SIM_SOPT2_UART0SRC_MASK;
//         reg_temp |= SIM_SOPT2_UART0SRC(0);
//         SIM_SOPT2 = reg_temp;
// 			
// 			  // Enter inifinite loop because the 
// 			  // the desired terminal port number 
// 			  // invalid!!
// 			  while(1)
// 				{}
//     }
    
    
    
    // Initialize baud rate
    baud_rate = baud;
    
    // Change units to Hz
    uart0clk = sysclk * 1000;
    // Calculate the first baud rate using the lowest OSR value possible.  
    i = 4;
    sbr_val = (uint32)(uart0clk/(baud_rate * i));
    calculated_baud = (uart0clk / (i * sbr_val));
        
    if (calculated_baud > baud_rate)
        baud_diff = calculated_baud - baud_rate;
    else
        baud_diff = baud_rate - calculated_baud;
    
    osr_val = i;
        
    // Select the best OSR value
    for (i = 5; i <= 32; i++)
    {
        sbr_val = (uint32)(uart0clk/(baud_rate * i));
        calculated_baud = (uart0clk / (i * sbr_val));
        
        if (calculated_baud > baud_rate)
            temp = calculated_baud - baud_rate;
        else
            temp = baud_rate - calculated_baud;
        
        if (temp <= baud_diff)
        {
            baud_diff = temp;
            osr_val = i; 
        }
    }
    
    if (baud_diff < ((baud_rate / 100) * 3))
    {
        // If the OSR is between 4x and 8x then both
        // edge sampling MUST be turned on.  
        if ((osr_val >3) && (osr_val < 9))
            UART0_C5|= UART0_C5_BOTHEDGE_MASK;
        
        // Setup OSR value 
        reg_temp = UART0_C4;
        reg_temp &= ~UART0_C4_OSR_MASK;
        reg_temp |= UART0_C4_OSR(osr_val-1);
    
        // Write reg_temp to C4 register
        UART0_C4 = reg_temp;
        
        reg_temp = (reg_temp & UART0_C4_OSR_MASK) + 1;
        sbr_val = (uint32)((uart0clk)/(baud_rate * (reg_temp)));
        
         /* Save off the current value of the uartx_BDH except for the SBR field */
        reg_temp = UART0_BDH & ~(UART0_BDH_SBR(0x1F));
   
        UART0_BDH = reg_temp |  UART0_BDH_SBR(((sbr_val & 0x1F00) >> 8));
        UART0_BDL = (uint8)(sbr_val & UART0_BDL_SBR_MASK);
        
        /* Enable receiver and transmitter */
				UART0_C2 |= UART0_C2_TE_MASK| UART0_C2_RE_MASK | UART0_C2_RIE_MASK;
    }
    else
		{
        // Unacceptable baud rate difference
        // More than 3% difference!!
        // Enter infinite loop!
        while(1)
				{}
		}					
    
}
/********************************************************************/
/*
 * Wait for a character to be received on the specified uart
 *
 * Parameters:
 *  channel      uart channel to read from
 *
 * Return Values:
 *  the received character
 */
char uart0_getchar (UART0_MemMapPtr channel)
{
      /* Wait until character has been received */
      while (!(UART0_S1_REG(channel) & UART0_S1_RDRF_MASK));
    
      /* Return the 8-bit data from the receiver */
      return UART0_D_REG(channel);
}
/********************************************************************/
/*
 * Wait for space in the uart Tx FIFO and then send a character
 *
 * Parameters:
 *  channel      uart channel to send to
 *  ch			 character to send
 */ 
void uart0_putchar (UART0_MemMapPtr channel, char ch)
{
      /* Wait until space is available in the FIFO */
      while(!(UART0_S1_REG(channel) & UART0_S1_TDRE_MASK));
    
      /* Send the character */
      UART0_D_REG(channel) = (uint8)ch;
    
 }

/********************************************************************/
/*
 * Wait for space in the uart Tx FIFO and then send a character
 *
 * Parameters:
 *  channel      uart channel to send to
 *  ch			 character to send
 */ 
void uart0_putstr(UART0_MemMapPtr channel, uint8 *s)
{
  while(*s)
	{
		uart0_putchar(channel,*s++);
	}
 }
/********************************************************************/
/*
 * Check to see if a character has been received
 *
 * Parameters:
 *  channel      uart channel to check for a character
 *
 * Return values:
 *  0       No character received
 *  1       Character has been received
 */
int uart0_getchar_present (UART0_MemMapPtr channel)
{
    return (UART0_S1_REG(channel) & UART0_S1_RDRF_MASK);
}
/********************************************************************/

//#if UART0_MODE == INTERRUPT_MODE
/***************************************************************************//*!
 * @brief   UART0 read data register full interrupt service routine.
 ******************************************************************************/
void UART0_IRQHandler(void)
{
	uint8 c = 0xff;
	if((UART0_S1 & UART_S1_OR_MASK) || 
			(UART0_S1 & UART_S1_NF_MASK) ||
			(UART0_S1 & UART_S1_FE_MASK) ||
			(UART0_S1 & UART_S1_PF_MASK)
	)
	{
		UART0_S1 |= UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK;
		c = UART0_D;
	}
  if (UART0_S1 & UART_S1_RDRF_MASK)
  {
    c = UART0_D;
		GPS_ComnandParser(c);
		if(logEnable == 2)
		{
			RINGBUF_Put(&comRingBuf,c);
		}
		#ifdef USE_UART0_RINGBUFF
		RINGBUF_Put(&USART0_RxRingBuff,c);
		#endif
  }
	
}
//#endif

//#if UART2_MODE == INTERRUPT_MODE
/***************************************************************************//*!
 * @brief   UART0 read data register full interrupt service routine.
 ******************************************************************************/
void UART1_IRQHandler(void)
{
	uint8 c = 0xff;
	if((UART1_S1 & UART_S1_OR_MASK) || 
			(UART1_S1 & UART_S1_NF_MASK) ||
			(UART1_S1 & UART_S1_FE_MASK) ||
			(UART1_S1 & UART_S1_PF_MASK)
	)
	{
		UART1_S1 |= UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK;
		c = UART1_D;
	}
  if (UART1_S1 & UART_S1_RDRF_MASK)
  {
    c = UART1_D;
		AT_ComnandParser(c);
		if(logEnable == 3)
		{
			RINGBUF_Put(&comRingBuf,c);
		}
		#ifdef USE_UART1_RINGBUFF
		RINGBUF_Put(&USART1_RxRingBuff,c);
		#endif
  }
}
//#endif


#if (defined(CW))
/*
** ===================================================================
**     Method      :  CsIO1___read_console (component ConsoleIO)
**
**     Description :
**         __read_console
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
int __read_console(__file_handle handle, unsigned char* buffer, size_t * count)
{
  size_t CharCnt = 0x00;

  (void)handle;                        /* Parameter is not used, suppress unused argument warning */
  if (TERM_PORT_NUM == 0)
	  *buffer = (unsigned char)uart0_getchar(UART0_BASE_PTR);
  else if (TERM_PORT_NUM == 1)
	  *buffer = (unsigned char)uart_getchar(UART1_BASE_PTR);
  else
	  *buffer = (unsigned char)uart_getchar(UART2_BASE_PTR);
  CharCnt = 1;                         /* Increase char counter */
  
  *count = CharCnt;
  return (__no_io_error);
}

/*
** ===================================================================
**     Method      :  CsIO1___write_console (component ConsoleIO)
**
**     Description :
**         __write_console
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count)
{
  size_t CharCnt = 0x00;

  (void)handle;                        /* Parameter is not used, suppress unused argument warning */
  for (;*count > 0x00; --*count) {
    /* Wait until UART is ready for saving a next character into the transmit buffer */
    out_char((unsigned char)*buffer);
    buffer++;                          /* Increase buffer pointer */
    CharCnt++;                         /* Increase char counter */
  }
  *count = CharCnt;
  return(__no_io_error);
}

/*
** ===================================================================
**     Method      :  CsIO1___close_console (component ConsoleIO)
**
**     Description :
**         __close_console
**         This method is internal. It is used by Processor Expert only.
** ===================================================================
*/
int __close_console(__file_handle handle)
{
  (void)handle;                        /* Parameter is not used, suppress unused argument warning */
  return(__no_io_error);
}

#endif
