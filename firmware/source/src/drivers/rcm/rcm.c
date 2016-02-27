/*
 * File:        rcm.c
 * Purpose:     Provides routines for the reset controller module
 *              
 */

#include "common.h"
#include "rcm.h"
#include "dbg_cfg_app.h"
#define RCM_Info(...)		//DbgCfgPrintf(__VA_ARGS__)
/* OutSRS routine - checks the value in the SRS registers and sends
 * messages to the terminal announcing the status at the start of the 
 * code.
 */
void outSRS(void){                         //[outSRS]
// 	uint8_t buffer[128];
//   
// 	if (RCM_SRS1 & RCM_SRS1_SACKERR_MASK)
// 		sprintf((char *)buffer,"\n\rStop Mode Acknowledge Error Reset");
// 	if (RCM_SRS1 & RCM_SRS1_MDM_AP_MASK)
// 		sprintf((char *)buffer,"\n\rMDM-AP Reset");
// 	if (RCM_SRS1 & RCM_SRS1_SW_MASK)
// 		sprintf((char *)buffer,"\n\rSoftware Reset");
// 	if (RCM_SRS1 & RCM_SRS1_LOCKUP_MASK)
// 		sprintf((char *)buffer,"\n\rCore Lockup Event Reset");
// 	
// 	if (RCM_SRS0 & RCM_SRS0_POR_MASK)
// 		sprintf((char *)buffer,"\n\rPower-on Reset");
// 	if (RCM_SRS0 & RCM_SRS0_PIN_MASK)
// 		sprintf((char *)buffer,"\n\rExternal Pin Reset");
// 	if (RCM_SRS0 & RCM_SRS0_WDOG_MASK)
// 		sprintf((char *)buffer,"\n\rWatchdog(COP) Reset");
// 	if (RCM_SRS0 & RCM_SRS0_LOC_MASK)
// 		sprintf((char *)buffer,"\n\rLoss of External Clock Reset");
// 	if (RCM_SRS0 & RCM_SRS0_LOL_MASK)
// 		sprintf((char *)buffer,"\n\rLoss of Lock in PLL Reset");
// 	if (RCM_SRS0 & RCM_SRS0_LVD_MASK)
// 		sprintf((char *)buffer,"\n\rLow-voltage Detect Reset");
// 	if (RCM_SRS0 & RCM_SRS0_WAKEUP_MASK)
//         {
//           sprintf((char *)buffer,"\n\r[outSRS]Wakeup bit set from low power mode ");
//           if ((SMC_PMCTRL & SMC_PMCTRL_STOPM_MASK)== 3)
//             sprintf((char *)buffer,"LLS exit ") ;
//           if (((SMC_PMCTRL & SMC_PMCTRL_STOPM_MASK)== 4) && ((SMC_STOPCTRL & SMC_STOPCTRL_VLLSM_MASK)== 0))
//             sprintf((char *)buffer,"VLLS0 exit ") ;
//           if (((SMC_PMCTRL & SMC_PMCTRL_STOPM_MASK)== 4) && ((SMC_STOPCTRL & SMC_STOPCTRL_VLLSM_MASK)== 1))
//             sprintf((char *)buffer,"VLLS1 exit ") ;
//           if (((SMC_PMCTRL & SMC_PMCTRL_STOPM_MASK)== 4) && ((SMC_STOPCTRL & SMC_STOPCTRL_VLLSM_MASK)== 2))
//             sprintf((char *)buffer,"VLLS2 exit") ;
//           if (((SMC_PMCTRL & SMC_PMCTRL_STOPM_MASK)== 4) && ((SMC_STOPCTRL & SMC_STOPCTRL_VLLSM_MASK)== 3))
//             sprintf((char *)buffer,"VLLS3 exit ") ; 
// 	}

//         if ((RCM_SRS0 == 0) && (RCM_SRS1 == 0)) 
//         {
// 	       sprintf((char *)buffer,"[outSRS]RCM_SRS0 is ZERO   = %#02X \r\n\r", (RCM_SRS0))  ;
// 	       sprintf((char *)buffer,"[outSRS]RCM_SRS1 is ZERO   = %#02X \r\n\r", (RCM_SRS1))  ;	 
//         }
// 				
// 		uart_putstr(UART1_BASE_PTR,buffer);
  }

