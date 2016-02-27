/*****************************************************************************************************
* Include files
*****************************************************************************************************/
#include "common.h"
#include "llwu.h"
#include "smc.h"
#include "mcg.h"
#include "pmc.h"
#include "uart.h"
#include "sysinit.h"
#include "dbg_cfg_app.h"
#include "power_management.h"
#include "audio.h"
#include "sst25.h"
#include "hw_config.h"

#define MCG_OUT_FREQ 96000000
#define FAST_IRC_FREQ  4000000 //4000000; // default fast irc frequency is 4MHz
#define SLOW_IRC_FREQ  32768 // default slow irc frequency is 32768Hz



#define PwrInfo(...)	//DbgCfgPrintf(__VA_ARGS__)

void clockMonitor(unsigned char state);
void vlp_clock_config(char next_mode);




uint8_t lpwMode = LPW_NONE;

uint32_t SetLowPowerModes(uint8_t mode)
{
	if(lpwMode == mode)
		return 1;
	if((lpwMode != LPW_NOMAL) && (mode != LPW_NOMAL) &&  (mode != LPW_NONE) && (mode != LPW_LEVEL_3))
		LowPowerModes('0');
	lpwMode = mode;
	switch(lpwMode)
	{
		case LPW_NOMAL:		
			LowPowerModes('0');
			SIM_SCGC5 |= (	SIM_SCGC5_PORTA_MASK
											| SIM_SCGC5_PORTB_MASK
                      | SIM_SCGC5_PORTC_MASK
                      | SIM_SCGC5_PORTD_MASK
                      | SIM_SCGC5_PORTE_MASK );
			SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
			SIM_SOPT2 |= SIM_SOPT2_UART0SRC(0); // select the UART0 clock source
		
			SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
			SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1); // select the TPM clock source
		
			Flash_Init(56);
		
			uart0_init(UART0_BASE_PTR,uart0_clk_khz,9600);
			uart_init(UART1_BASE_PTR,periph_clk_khz,57600);
			TICK_Init(1,mcg_clk_hz);
			TPM0_Init(mcg_clk_hz,1000);
			SST25_Init();
			AUDIO_Init();
			USB_Init();                               // USB Initialization
			USB_Connect(TRUE);                        // USB Connect
		break;
		
		case LPW_LEVEL_1:
			USB_Connect(FALSE);                        // USB Disconnect
			LowPowerModes('2');
			SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
			SIM_SOPT2 |= SIM_SOPT2_UART0SRC(3); // select the UART0 clock source
		
			SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source field
			SIM_SOPT2 |= SIM_SOPT2_TPMSRC(3); // select the TPM clock source
			
			uart0_init(UART0_BASE_PTR,uart0_clk_khz,9600);
			uart_init(UART1_BASE_PTR,periph_clk_khz,2400);
			TICK_Init(1,mcg_clk_hz);
			TPM0_Init(mcg_clk_hz,1000);
			SST25_Init();
			AUDIO_Init();
		break;
		
		case LPW_LEVEL_2:
			LowPowerModes('3');
			SIM_SCGC5 &= ~(SIM_SCGC5_PORTB_MASK
                      | SIM_SCGC5_PORTC_MASK
                      | SIM_SCGC5_PORTD_MASK
                      | SIM_SCGC5_PORTE_MASK );
			SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field	
			SIM_SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK; // first clear the TPM clk source fielde
		break;
		
		case LPW_LEVEL_3:
			LowPowerModes('4');
		break;
		default:
		break;
	}
	return 0;
}

void LowPowerModes(uint8_t mode)
{
	uint8_t modeTest = mode;
  unsigned char op_mode;  
		
	//while(RINGBUF_Get(&USART0_RxRingBuff,&modeTest) != 0);
	switch(modeTest){
		
		case '0':// Exit VLPR
			PwrInfo("Exit VLPR\n\r ");
			__disable_irq();
			/*Exit VLPR*/
			exit_vlpr();
			SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK
                      | SIM_SCGC5_PORTB_MASK
                      | SIM_SCGC5_PORTC_MASK
                      | SIM_SCGC5_PORTD_MASK
                      | SIM_SCGC5_PORTE_MASK );
			op_mode = what_mcg_mode();
			if(op_mode==BLPE)
			{
				vlp_clock_config(PEE);    
			} else if (op_mode==BLPI)
			{
				vlp_clock_config(PEE);                  
			}
			clockMonitor(ON);
			if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
				PwrInfo("  in VLPR Mode !\n\r ");
			} else if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1){
				PwrInfo("  in Run Mode  !\n\r ");
					
			}
		break;
			
		case '1'://VLPR
			__disable_irq();
			if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4)
				exit_vlpr();
			 /*Maximum clock frequency for this mode is core 4MHz and Flash 1Mhz*/
			PwrInfo("Enter VLPR\n\r ");
			PwrInfo("Configure clock frequency to 4MHz core clock and 1MHz flash clock\n\r ");
			op_mode = what_mcg_mode();
			if(op_mode==PEE)
			{
				vlp_clock_config(BLPE);
			} else if (op_mode==FEE)
			{
				vlp_clock_config(BLPE);
			}
			clockMonitor(OFF);
			/*Go to VLPR*/
			enter_vlpr();   // get out of VLPR back to RUN
			if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
				PwrInfo("  in VLPR Mode !\n\r ");
			} else if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1){
				PwrInfo("  in Run Mode  !\n\r ");
					clockMonitor(ON);
			}
		break;
		
	 case '2'://VLPR in BLPI 4 MHZ
			__disable_irq();
			/*Maximum clock frequency for this mode is core 4MHz and Flash 1Mhz*/
			PwrInfo("Enter VLPR\n\r ");
			PwrInfo("Configure clock frequency to 4MHz core clock and 1MHz flash clock\n\r ");
			/*Get out of VLPR to change clock dividers*/
			exit_vlpr();  // get out of VLPR back to RUN
			vlp_clock_config(BLPI);
			/*Go to VLPR*/
			enter_vlpr();   // get into VLPR 
			if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
				PwrInfo("  in VLPR Mode !\n\r ");
			} else if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1){
				PwrInfo("  in Run Mode  !\n\r ");
				PwrInfo("  Clock Monitor On!\n\r ");
				clockMonitor(ON);
			}
			break;

		case '3'://VLPR in BLPI 2 MHZ
			__disable_irq();
			/*Maximum clock frequency for this mode is core 4MHz and Flash 1Mhz*/
			PwrInfo("Enter VLPR\n\r ");
			PwrInfo("Configure clock frequency to 2MHz core clock and 1MHz flash clock\n\r ");
			/*Get out of VLPR to change clock dividers*/
			exit_vlpr();  // get out of VLPR back to RUN
			vlp_clock_config(BLPI);    
			/*Go to VLPR*/
			enter_vlpr();   // get into VLPR 
			if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
				PwrInfo("  in VLPR Mode !\n\r ");
			} else if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1){
				PwrInfo("  in Run Mode  !\n\r ");
				PwrInfo("  Clock Monitor On!\n\r ");
				clockMonitor(ON);
			}
			SIM_SCGC5 &= ~(SIM_SCGC5_PORTB_MASK
                      | SIM_SCGC5_PORTC_MASK
                      | SIM_SCGC5_PORTD_MASK
                      | SIM_SCGC5_PORTE_MASK );
			SIM_SCGC7 = 0;
			break;   
			
		case '4':
			//SIM_SCGC6 &= ~SIM_SCGC6_TPM0_MASK;
			//PwrInfo("Enter LLS\n\r ");
			PORTC_PCR3 = PORT_PCR_MUX(0);
			PORTC_PCR4 = PORT_PCR_MUX(0);
			clockMonitor(OFF); 
			enter_lls();
			//SIM_SCGC6 |= SIM_SCGC6_TPM0_MASK;
			/*After LLS wake up that was enter from PEE the exit will be on PBE */ 
			/*  for this reason after wake up make sure to get back to desired  */
			/*  clock mode                                                      */
			op_mode = what_mcg_mode();
			if(op_mode==PBE)
			{
				pbe_pee(CLK0_FREQ_HZ);
//				while(1)
//				{
//					LED_RED_PIN_CLR;
//					LED_GREEN_PIN_CLR;
//					LED_BLUE_PIN_CLR;
//				}
				//mcg_clk_hz = (pbe_pee(CLK0_FREQ_HZ) / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV1_MASK) >> SIM_CLKDIV1_OUTDIV1_SHIFT) + 1));
				//uart0_clk_khz = ((mcg_clk_hz / 2) / 1000); // UART0 clock frequency will equal half the PLL frequency
			}
			clockMonitor(ON);
			break;
		default:
			break;
	}
	
//	if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
//		PwrInfo("  in VLPR Mode !  ");
//	} else if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1){
//		PwrInfo("  in Run Mode  !  ");
//	}
//	op_mode = what_mcg_mode();
//	if (op_mode ==1) 
//			PwrInfo("in BLPI mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==2) 
//			PwrInfo(" in FBI mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==3) 
//			PwrInfo(" in FEI mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==4) 
//			PwrInfo(" in FEE mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==5) 
//			PwrInfo(" in FBE mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==6) 
//			PwrInfo(" in BLPE mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==7) 
//			PwrInfo(" in PBE mode now at %d Hz\r\n",mcg_clk_hz );
//	if (op_mode ==8) 
//			PwrInfo(" in PEE mode now at %d Hz\r\n",mcg_clk_hz );
	
	__enable_irq();	
}

/*******************************************************************************************************/
void uart_configure(int clock_khz,int uart0_clk_src )
{	
//      int mcg_clock_khz;
//      int core_clock_khz;
// #ifdef USE_UART_DEBUG
//      SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
//      SIM_SOPT2 &= ~SIM_SOPT2_UART0SRC_MASK; // first clear the uart0 clk source field
//      SIM_SOPT2 |= uart0_clk_src ; // select the UART0 clock source
//      
//      if (TERM_PORT_NUM == 0){
//         uart0_init (UART0_BASE_PTR, clock_khz, TERMINAL_BAUD); 
//      }else {
//         mcg_clock_khz = clock_khz / 1000;
//         core_clock_khz = mcg_clock_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV1_MASK) >> 28)+ 1);
//         uart_init (UART1_BASE_PTR, core_clock_khz, TERMINAL_BAUD);
//      }
// #endif
}

/*******************************************************************************************************/
int PEE_to_BLPE(void)
{
     int mcg_clock_hz;    
     
     mcg_clock_hz = atc(FAST_IRC,FAST_IRC_FREQ,MCG_OUT_FREQ);

     mcg_clock_hz = pee_pbe(CLK0_FREQ_HZ);
     mcg_clock_hz = pbe_blpe(CLK0_FREQ_HZ);
      return mcg_clock_hz; 
}

/*******************************************************************************************************/
int BLPE_to_PEE(void)
{
      int mcg_clock_hz;    
  
      SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV4(0x1);
      
      /*After wake up back to the original clock frequency*/            
      mcg_clock_hz = blpe_pbe(CLK0_FREQ_HZ, PLL0_PRDIV,PLL0_VDIV);
      mcg_clock_hz = pbe_pee(CLK0_FREQ_HZ);
      
      return mcg_clock_hz;
}

/*******************************************************************************************************/
void clockMonitor(unsigned char state)
{
    if(state)
		{
			//PwrInfo("  Clock Monitor On!\n\r ");
      MCG_C6 |= MCG_C6_CME0_MASK;
		}
    else
		{
			//PwrInfo("  Clock Monitor Off!\n\r ");
      MCG_C6 &= ~MCG_C6_CME0_MASK;
		}
}
  
/****************************************************************/
/* vlp_clock_config(char next_mode)
 * 
 * input: next_mode = BLPI or BLPE
 *****************************************************************/
void vlp_clock_config(char next_mode)
{
  int current_mcg_mode;
  unsigned char return_code;
  current_mcg_mode = what_mcg_mode();
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
       PwrInfo("\n\rIn VLPR Mode ! Must go to RUN to change Clock Modes --> ");
       exit_vlpr();
  }
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 1)
      PwrInfo("In RUN Mode !\n\r");
  switch(current_mcg_mode){
              
  case 1:  //current_mcg_mode ==BLPI
    PwrInfo("\n\r in BLPI mode with fast IRC \n\r");
    if(next_mode == FEI){
        mcg_clk_hz = blpi_fbi(SLOW_IRC_FREQ, 0);
        mcg_clk_hz = fbi_fei(SLOW_IRC_FREQ);
        MCG_C4 |= (MCG_C4_DRST_DRS(1) | MCG_C4_DMX32_MASK);
        mcg_clk_hz = 48000000; //FEI mode
        
        SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0)
                    | SIM_CLKDIV1_OUTDIV4(1) );                
        SIM_SOPT2 &= ~SIM_SOPT2_PLLFLLSEL_MASK; // clear PLLFLLSEL to select the FLL for this clock source
    }
		if(next_mode == PEE)
		{
			mcg_clk_hz = blpi_fbi(FAST_IRC_FREQ, 1);
			mcg_clk_hz = fbi_fbe(CLK0_FREQ_HZ,0,CRYSTAL);
			mcg_clk_hz = fbe_pbe(CLK0_FREQ_HZ,PLL0_PRDIV,PLL0_VDIV);
			mcg_clk_hz = pbe_pee(CLK0_FREQ_HZ) / 2;
			 /* Configuration for PLL freq = 96MHz */
			SIM_CLKDIV1 = ( 0
											| SIM_CLKDIV1_OUTDIV1(1)
											| SIM_CLKDIV1_OUTDIV4(1) );
			SIM_SOPT2 |= SIM_SOPT2_PLLFLLSEL_MASK; // set PLLFLLSEL to select the PLL for this clock source
			mcg_clk_khz = mcg_clk_hz / 1000;
			core_clk_khz = mcg_clk_khz;
			periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
      uart0_clk_khz = mcg_clk_khz; // UART0 clock frequency will equal half the PLL frequency
			uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(0)); 
		}
    break;
  case 2:  // current_mcg_mode ==FBI 
    PwrInfo("\n\r in FBI mode now \n\r");
    break;
  case 3:  // current_mcg_mode ==FEI) 
    PwrInfo("\n\r in FEI mode ");
    if (next_mode == BLPE)
    {
        mcg_clk_hz =  fei_fbe( CLK0_FREQ_HZ,  1, CRYSTAL);
        mcg_clk_hz = fbe_blpe(CLK0_FREQ_HZ);   
        OSC0_CR |= OSC_CR_ERCLKEN_MASK;
    } else if (next_mode == BLPI)
    { //next_mode is BLPI
		 /* MCG_SC: FCRDIV=2 */
			MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(
								MCG_SC_FCRDIV(0x05)
							 )) | (uint8_t)(
								MCG_SC_FCRDIV(0x02)
							 ));    
      mcg_clk_hz = fei_fbi(FAST_IRC_FREQ,FAST_IRC);
      mcg_clk_hz = fbi_blpi(FAST_IRC_FREQ,FAST_IRC);
      /* Internal Fast IRC is 4 MHz so div by 1 for sysclk and div 4 for flash clock*/
      SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0)
                      | SIM_CLKDIV1_OUTDIV4(4) ); // div 5 for flash clk margin
			mcg_clk_khz = mcg_clk_hz / 1000;
			core_clk_khz = mcg_clk_khz;
			periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
      uart0_clk_khz = mcg_clk_khz; // UART0 clock frequency will equal half the PLL frequency
      MCG_C1 |= MCG_C1_IRCLKEN_MASK;
      MCG_C2 |= MCG_C2_IRCS_MASK;
    }
		/*Configure UART for the oscerclk frequency*/
    uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(3)); 
    /* External Reference is 8 MHz so div by 2 for sysclk and div 4 for flash clock*/
    PwrInfo("\n\r moved to BLPE mode \n\r");
    break;
  case 4:  // current_mcg_mode ==4) 
    PwrInfo("\n\r in FEE mode \n\r");
    break;
  case 5:  // current_mcg_mode ==FBE) 
    PwrInfo("\n\r in FBE mode \n\r");
    break;
  case 6:  // current_mcg_mode ==BLPE) 
    PwrInfo("\n\r in BLPE mode \n\r");
    if (next_mode == BLPI){
        MCG_C2 &= ~MCG_C2_IRCS_MASK;
        MCG_C1 |= MCG_C1_IRCLKEN_MASK;
        MCG_SC &=  ~MCG_SC_FCRDIV_MASK;  //set to div by 1  
        mcg_clk_hz = blpe_fbe(CLK0_FREQ_HZ);
        mcg_clk_hz = fbe_fbi(FAST_IRC_FREQ, FAST_IRC);
        mcg_clk_hz = fbi_blpi(FAST_IRC_FREQ,FAST_IRC);
        /* Internal Fast IRC is 4 MHz so div by 1 for sysclk and div 4 for flash clock*/
        SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0)
                        | SIM_CLKDIV1_OUTDIV4(4) ); // div 5 for flash clk margin       
				mcg_clk_khz = mcg_clk_hz / 1000;
				core_clk_khz = mcg_clk_khz;
				periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
				uart0_clk_khz = mcg_clk_khz; // UART0 clock frequency will equal half the PLL frequency
        uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(3)); 
        /* External Reference is 8 MHz so div by 2 for sysclk and div 4 for flash clock*/
        PwrInfo("\n\r moved to BLPI mode \n\r");
    }else if (next_mode == PEE){    
       //After wake up back to the original clock frequency          
       mcg_clk_hz = blpe_pbe(CLK0_FREQ_HZ, PLL0_PRDIV,PLL0_VDIV);
       mcg_clk_hz = pbe_pee(CLK0_FREQ_HZ) / 2;
			/* Configuration for PLL freq = 96MHz */
			SIM_CLKDIV1 = ( 0
											| SIM_CLKDIV1_OUTDIV1(1)
											| SIM_CLKDIV1_OUTDIV4(1) );
			SIM_SOPT2 |= SIM_SOPT2_PLLFLLSEL_MASK; // set PLLFLLSEL to select the PLL for this clock source
			mcg_clk_khz = mcg_clk_hz / 1000;
			core_clk_khz = mcg_clk_khz;
			periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
			uart0_clk_khz = mcg_clk_khz; // UART0 clock frequency will equal half the PLL frequency
			uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(0));
    }    
    break;
  case 7:  // current_mcg_mode ==PBE) 
    PwrInfo("\n\r in PBE mode \n\r");
    if (next_mode == PEE){
       SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|
                     SIM_CLKDIV1_OUTDIV4(0x1);     
       /*After wake up back to the original clock mode*/            
       mcg_clk_hz = pbe_pee(CLK0_FREQ_HZ);   
			/*Configure UART for the oscerclk frequency*/
       uart_configure(mcg_clk_hz,SIM_SOPT2_UART0SRC(1));   
    }
    PwrInfo("moved to PEE clock mode \n\r");

    break;
  case 8:  // current_mcg_mode ==8) PEE
    if (next_mode == BLPI)
    {
      // run ATC test to determine irc trim
        return_code = atc(FAST_IRC,FAST_IRC_FREQ,mcg_clk_hz);
        if (return_code != 0){
          PwrInfo("\n\rAutotrim Failed\n\r");
        }
				
        mcg_clk_hz =  pee_pbe(CRYSTAL);
        mcg_clk_hz = pbe_fbe(CRYSTAL); 
				if(lpwMode == LPW_LEVEL_1)
				{
							/* MCG_SC: FCRDIV=2 */
							MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(
								MCG_SC_FCRDIV(0x05)
							 )) | (uint8_t)(
								MCG_SC_FCRDIV(0x00)
							 ));
							SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0) //4MHz for core
													| SIM_CLKDIV1_OUTDIV4(3) );//1Mhz for Flash and peripherals
				}
				else
				{
				/* MCG_SC: FCRDIV=2 */
					MCG_SC = (uint8_t)((MCG_SC & (uint8_t)~(uint8_t)(
						MCG_SC_FCRDIV(0x05)
					 )) | (uint8_t)(
						MCG_SC_FCRDIV(0x02)
					 ));
					SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0) //1MHz for core
													| SIM_CLKDIV1_OUTDIV4(1) );//500khz for Flash and peripherals
				}
        
        mcg_clk_hz = fbe_fbi(FAST_IRC_FREQ, FAST_IRC);
        mcg_clk_hz = fbi_blpi(FAST_IRC_FREQ, FAST_IRC);
				mcg_clk_khz = mcg_clk_hz / 1000;
				core_clk_khz = mcg_clk_khz;
				periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
				uart0_clk_khz = mcg_clk_khz; // UART0 clock frequency will equal half the PLL frequency
// 				if(lpwMode == LPW_LEVEL_1)
// 				{
// 					SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0) //4MHz for core
// 													| SIM_CLKDIV1_OUTDIV4(3) );//1Mhz for Flash and peripherals
// 				}
// 				else
// 				{
// 					SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(0) //1MHz for core
// 													| SIM_CLKDIV1_OUTDIV4(1) );//500khz for Flash and peripherals
// 				}
        MCG_C1 |= MCG_C1_IRCLKEN_MASK;
        MCG_C2 |= MCG_C2_IRCS_MASK;
        clockMonitor(OFF);
				uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(3));// use IRCLK
        PwrInfo("\n\r Now moved to BLPI clock mode \n\r");
    } else
    {
      PwrInfo("\n\r in PEE clock mode moving to BLPE clock mode \n\r");
			return_code = atc(FAST_IRC,FAST_IRC_FREQ,mcg_clk_hz);
			if (return_code != 0){
						PwrInfo("\n\rAutotrim Failed\n\r");
			}
			mcg_clk_hz = PEE_to_BLPE();
			
			/* External Reference is 8 MHz so div by 2 for sysclk and div 4 for flash clock*/
			SIM_CLKDIV1 = (   SIM_CLKDIV1_OUTDIV1(1)
											| SIM_CLKDIV1_OUTDIV4(3) );            
			mcg_clk_hz =  mcg_clk_hz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV1_MASK) >> 28)+ 1);    
			MCG_C1 |= MCG_C1_IRCLKEN_MASK; // enable irc
			MCG_C2 |= MCG_C2_IRCS_MASK;    // select fast irc  
			MCG_SC &=  ~MCG_SC_FCRDIV_MASK;  //set to div by 1   
			/*Configure UART for the oscerclk frequency*/
			mcg_clk_khz = mcg_clk_hz / 1000;
			core_clk_khz = mcg_clk_khz;
			periph_clk_khz = core_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV4_MASK) >> 16)+ 1);
			uart0_clk_khz = ((FAST_IRC_FREQ ) / 1000); // UART0 clock frequency will equal fast irc
			uart_configure(uart0_clk_khz,SIM_SOPT2_UART0SRC(3));// use MCGIRCLK
			PwrInfo("\n\r Now moved to BLPE clock mode \n\r");
    }
    break;
   /* end of case statement*/
  }
}
