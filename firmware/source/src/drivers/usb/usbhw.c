/*----------------------------------------------------------------------------
 *      RL-ARM - USB
 *----------------------------------------------------------------------------
 *      Name:    usbd_MKL25Z4.c
 *      Purpose: Hardware Layer module for Freescale MKL25Z4
 *      Rev.:    V4.70
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <MKL25Z4.H>
#include "usb.h"
#include "usbcfg.h"
#include "usbreg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "usbuser.h"



typedef struct __BUF_DESC {
  uint8_t    stat;
  uint8_t    reserved;
  uint16_t   bc;
  uint32_t   buf_addr;
}BUF_DESC;

BUF_DESC __align(512) BD[(USB_EP_NUM + 1) * 2 * 2];
uint8_t EPBuf[(USB_EP_NUM + 1)* 2 * 2][64];
uint8_t OutEpSize[USB_EP_NUM + 1];

uint32_t Data1  = 0x55555555;

#define BD_OWN_MASK        0x80 
#define BD_DATA01_MASK     0x40
#define BD_KEEP_MASK       0x20
#define BD_NINC_MASK       0x10
#define BD_DTS_MASK        0x08
#define BD_STALL_MASK      0x04

#define TX    1
#define RX    0
#define ODD   0
#define EVEN  1
#define IDX(Ep, dir, Ev_Odd) ((((Ep & 0x0F) * 4) + (2 * dir) + (1 *  Ev_Odd)))

#define SETUP_TOKEN    0x0D
#define IN_TOKEN       0x09
#define OUT_TOKEN      0x01
#define TOK_PID(idx)   ((BD[idx].stat >> 2) & 0x0F)

/*
 *  USB Device Interrupt enable
 *   Called by USB_Init to enable the USB Interrupt
 *    Return Value:    None
 */

#ifdef __RTX
void __svc(1) USB_IntrEna (void);
void __SVC_1               (void) {
#else
void          USB_IntrEna (void) {
#endif
	NVIC_SetPriority (USB0_IRQn, (1<< 2) - 1);
  NVIC_EnableIRQ   (USB0_IRQn);         /* Enable OTG interrupt               */
}


/*
 *  USB Device Initialize Function
 *   Called by the User to initialize USB
 *   Return Value:    None
 */

void USB_Init (void) {
  OutEpSize[0] = USB_MAX_PACKET0;

  /* Enable all clocks needed for USB to function                             */
  /* Set USB clock to 48 MHz                                                  */
  SIM->SOPT2   |=   SIM_SOPT2_USBSRC_MASK     | /* MCGPLLCLK used as src      */
                    SIM_SOPT2_PLLFLLSEL_MASK  ; /* Select MCGPLLCLK as clock  */
  SIM->SCGC4   |=   SIM_SCGC4_USBOTG_MASK;      /* Enable USBOTG clock        */

  USB_IntrEna ();

  USB0->USBTRC0 |= USB_USBTRC0_USBRESET_MASK;
  while (USB0->USBTRC0 & USB_USBTRC0_USBRESET_MASK);

  USB0->BDTPAGE1 = (uint8_t) ((uint32_t) BD >> 8 );
  USB0->BDTPAGE2 = (uint8_t) ((uint32_t) BD >> 16);
  USB0->BDTPAGE3 = (uint8_t) ((uint32_t) BD >> 24);

  USB0->ISTAT   = 0xFF;                 /* clear interrupt flags              */

  /* enable interrupts                                                        */
  USB0->INTEN =                            USB_INTEN_USBRSTEN_MASK |
                                           USB_INTEN_TOKDNEEN_MASK |
                                           USB_INTEN_SLEEPEN_MASK  |
#ifdef __RTX
              ((USB_RTX_DevTask   != 0) ? USB_INTEN_SOFTOKEN_MASK : 0) |
              ((USB_RTX_DevTask   != 0) ? USB_INTEN_ERROREN_MASK  : 0) ;
#else
              (((USB0->ISTAT & (1 << USB_ISTAT_SOFTOK_SHIFT)) != 0) ? USB_INTEN_SOFTOKEN_MASK : 0) |
              (((USB0->ISTAT & (1 << USB_INTEN_ERROREN_SHIFT)) != 0) ? USB_INTEN_ERROREN_MASK  : 0) ;
#endif

  USB0->USBCTRL  = USB_USBCTRL_PDE_MASK;/* pull dawn on D+ and D-             */
  USB0->USBTRC0 |= (1 << 6);            /* bit 6 must be set to 1             */
}


/*
 *  USB Device Connect Function
 *   Called by the User to Connect/Disconnect USB Device
 *    Parameters:      con:   Connect/Disconnect
 *    Return Value:    None
 */

void USB_Connect (uint32_t con) {

  if ( con) {
    USB0->CTL  |= USB_CTL_USBENSOFEN_MASK;            /* enable USB           */
    USB0->CONTROL = USB_CONTROL_DPPULLUPNONOTG_MASK;  /* pull up on D+        */
  }
  else {
    USB0->CTL  &= ~USB_CTL_USBENSOFEN_MASK;           /* disable USB          */
    USB0->CONTROL &= ~USB_CONTROL_DPPULLUPNONOTG_MASK;/* pull down on D+      */
  }
}


/*
 *  USB Device Reset Function
 *   Called automatically on USB Device Reset
 *    Return Value:    None
 */

void USB_Reset (void) {
  uint32_t i;

  for (i = 1; i < 16; i++) {
    USB0->ENDPOINT[i].ENDPT = 0x00;
  }

  /* EP0 control endpoint                                                     */
  BD[IDX(0, RX, ODD )].bc       = USB_MAX_PACKET0;
  BD[IDX(0, RX, ODD )].buf_addr = (uint32_t) &(EPBuf[IDX(0, RX, ODD )][0]);
  BD[IDX(0, RX, ODD )].stat     = BD_OWN_MASK | BD_DTS_MASK | BD_DATA01_MASK;

  BD[IDX(0, RX, EVEN)].stat     = 0;

  BD[IDX(0, TX, ODD )].buf_addr = (uint32_t) &(EPBuf[IDX(0, TX, ODD )][0]);
  BD[IDX(0, TX, EVEN)].buf_addr = 0;

  USB0->ENDPOINT[0].ENDPT = USB_ENDPT_EPHSHK_MASK | /* enable ep handshaking  */
                            USB_ENDPT_EPTXEN_MASK | /* enable TX (IN) tran.   */
                            USB_ENDPT_EPRXEN_MASK;  /* enable RX (OUT) tran.  */

  Data1 = 0x55555555;
  USB0->CTL    |=  USB_CTL_ODDRST_MASK;

  USB0->ISTAT   =  0xFF;                /* clear all interrupt status flags   */
  USB0->ERRSTAT =  0xFF;                /* clear all error flags              */
  USB0->ERREN   =  0xFF;                /* enable error interrupt sources     */
  USB0->ADDR    =  0x00;                /* set default address                */

}


/*
 *  USB Device Suspend Function
 *   Called automatically on USB Device Suspend
 *    Return Value:    None
 */

void USB_Suspend (void) {
  USB0->INTEN |= USB_INTEN_RESUMEEN_MASK;
}


/*
 *  USB Device Resume Function
 *   Called automatically on USB Device Resume
 *    Return Value:    None
 */

void USB_Resume (void) {
  USB0->INTEN &= ~USB_INTEN_RESUMEEN_MASK;

}


/*
 *  USB Device Remote Wakeup Function
 *   Called automatically on USB Device Remote Wakeup
 *    Return Value:    None
 */

void USB_WakeUp (void) {
  uint32_t i = 50000;
  if (USB_DeviceStatus & USB_GETSTATUS_REMOTE_WAKEUP) {
    USB0->CTL |=  USB_CTL_RESUME_MASK;
    while (i--)   __nop();
    USB0->CTL &= ~USB_CTL_RESUME_MASK;
  }
}


/*
 *  USB Device Remote Wakeup Configuration Function
 *    Parameters:      cfg:   Device Enable/Disable
 *    Return Value:    None
 */

void USB_WakeUpCfg (uint32_t cfg) {
  /* Not needed                                                               */
}


/*
 *  USB Device Set Address Function
 *    Parameters:      adr:   USB Device Address
 *    Return Value:    None
 */

void USB_SetAddress (uint32_t  adr) {
    USB0->ADDR    = adr & 0x7F;
}


/*
 *  USB Device Configure Function
 *    Parameters:      cfg:   Device Configure/Deconfigure
 *    Return Value:    None
 */

void USB_Configure (uint32_t cfg) {
}


/*
 *  Configure USB Device Endpoint according to Descriptor
 *    Parameters:      pEPD:  Pointer to Device Endpoint Descriptor
 *    Return Value:    None
 */

void USB_ConfigEP (USB_ENDPOINT_DESCRIPTOR *pEPD) {
  uint32_t num, val;

  num  = pEPD->bEndpointAddress;
  val  = pEPD->wMaxPacketSize;

  if (!(pEPD->bEndpointAddress & 0x80)) {
    OutEpSize[num] = val;
  }
  USB_ResetEP (num);
}


/*
 *  Set Direction for USB Device Control Endpoint
 *    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
 *    Return Value:    None
 */

void USB_DirCtrlEP (uint32_t dir) {
  /* Not needed                                                               */
}


/*
 *  Enable USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_EnableEP (uint32_t EPNum) {

  if (EPNum & 0x80) {
    EPNum &= 0x0F;
    USB0->ENDPOINT[EPNum].ENDPT |= USB_ENDPT_EPHSHK_MASK | /*en ep handshaking*/
                                   USB_ENDPT_EPTXEN_MASK;  /*en TX (IN) tran  */
  }
  else {
    USB0->ENDPOINT[EPNum].ENDPT |= USB_ENDPT_EPHSHK_MASK | /*en ep handshaking*/
                                   USB_ENDPT_EPRXEN_MASK;  /*en RX (OUT) tran.*/
  }
}


/*
 *  Disable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_DisableEP (uint32_t EPNum) {
  if (EPNum & 0x80) {
    EPNum &= 0x0F;
    USB0->ENDPOINT[EPNum].ENDPT &= ~(USB_ENDPT_EPHSHK_MASK |/*dis handshaking */
                                     USB_ENDPT_EPTXEN_MASK);/*dis TX(IN) tran */
  }
  else {
    USB0->ENDPOINT[EPNum].ENDPT &= ~(USB_ENDPT_EPHSHK_MASK |/*dis handshaking */
                                     USB_ENDPT_EPRXEN_MASK);/*dis RX(OUT) tran*/
  }
}


/*
 *  Reset USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_ResetEP (uint32_t EPNum) {
  if (EPNum & 0x80) {
    EPNum &= 0x0F;
    Data1 |= (1 << ((EPNum * 2) + 1));
    BD[IDX(EPNum, TX, ODD )].buf_addr = (uint32_t) &(EPBuf[IDX(EPNum, TX, ODD )][0]);
    BD[IDX(EPNum, TX, EVEN)].buf_addr = 0;
  }
  else {
    Data1 &= ~(1 << ((EPNum * 2)));
    BD[IDX(EPNum, RX, ODD )].bc       = OutEpSize[EPNum];
    BD[IDX(EPNum, RX, ODD )].buf_addr = (uint32_t) &(EPBuf[IDX(EPNum, RX, ODD )][0]);
    BD[IDX(EPNum, RX, ODD )].stat     = BD_OWN_MASK | BD_DTS_MASK;

    BD[IDX(EPNum, RX, EVEN)].stat     = 0;
  }
}

/*
 *  Set Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_SetStallEP (uint32_t EPNum) {
  EPNum &= 0x0F; 
  USB0->ENDPOINT[EPNum].ENDPT |= USB_ENDPT_EPSTALL_MASK;
}


/*
 *  Clear Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_ClrStallEP (uint32_t EPNum) {
  USB0->ENDPOINT[EPNum & 0x0F].ENDPT &= ~USB_ENDPT_EPSTALL_MASK;
  USB_ResetEP (EPNum);
}


/*
 *  Clear USB Device Endpoint Buffer
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_ClearEPBuf (uint32_t EPNum) {
}


/*
 *  Read USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *    Return Value:    Number of bytes read
 */

uint32_t USB_ReadEP (uint32_t EPNum, uint8_t *pData) {
  uint32_t n, sz, idx, setup = 0;


  idx = IDX(EPNum, RX, 0);
  sz  = BD[idx].bc;
  
  if ((EPNum == 0) && (TOK_PID(idx) == SETUP_TOKEN)) setup = 1;
  
  for (n = 0; n < sz; n++) {
    pData[n] = EPBuf[idx][n];
  }
  
  BD[idx].bc = OutEpSize[EPNum];
  
  if ((Data1 >> (idx / 2) & 1) == ((BD[idx].stat >> 6) & 1)) {
    if (setup && (pData[6] == 0))       /* if no setup data stage,            */
      Data1 &= ~1UL;                    /* set DATA0                          */
    else Data1 ^= (1 << (idx / 2));
  }
  
  if ((Data1 >> (idx / 2)) & 1) {
    BD[idx].stat  = BD_DTS_MASK | BD_DATA01_MASK;
    BD[idx].stat |= BD_OWN_MASK;
  }
  else {
    BD[idx].stat  = BD_DTS_MASK;
    BD[idx].stat |= BD_OWN_MASK;
  }
  
  USB0->CTL &= ~USB_CTL_TXSUSPENDTOKENBUSY_MASK;
  return (sz);
}


/*
 *  Write USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *                     cnt:   Number of bytes to write
 *    Return Value:    Number of bytes written
 */

uint32_t USB_WriteEP (uint32_t EPNum, uint8_t *pData, uint32_t cnt) {
  uint32_t idx, n;
  
  EPNum &=0x0F;
  
  idx = IDX(EPNum, TX, 0);
  BD[idx].bc = cnt;
  for (n = 0; n < cnt; n++) {
    EPBuf[idx][n] = pData[n];
  }
  if ((Data1 >> (idx / 2)) & 1) {
    BD[idx].stat = BD_OWN_MASK | BD_DTS_MASK;
  }
  else {
    BD[idx].stat = BD_OWN_MASK | BD_DTS_MASK | BD_DATA01_MASK;
  }
  Data1 ^= (1 << (idx / 2));
  return(cnt);
}

/*
 *  Get USB Device Last Frame Number
 *    Parameters:      None
 *    Return Value:    Frame Number
 */

uint32_t USB_GetFrame (void) {
  return((USB0->FRMNUML | (USB0->FRMNUMH << 8) & 0x07FF));
}

#ifdef __RTX
uint32_t LastError;                          /* Last Error                         */

/*
 *  Get USB Device Last Error Code
 *    Parameters:      None
 *    Return Value:    Error Code
 */

uint32_t USB_GetError (void) {
  return (LastError);
}
#endif


/*
 *  USB Device Interrupt Service Routine
 */
void USB0_IRQHandler(void) {
  uint32_t istr, num, dir, ev_odd;

  istr = USB0->ISTAT & USB0->INTEN;

/* reset interrupt                                                            */
  if (istr & USB_ISTAT_USBRST_MASK) {
    USB_Reset();
#ifdef __RTX
    if (USB_RTX_DevTask) {
      isr_evt_set(USB_EVT_RESET, USB_RTX_DevTask);
    }
#else
		#if USB_RESUME_EVENT
    if (USB0->ISTAT & (1 << USB_ISTAT_USBRST_SHIFT)) {
      USB_Reset_Event();
    }
		#endif
#endif
    USB0->ISTAT = USB_ISTAT_USBRST_MASK;
  }

/* suspend interrupt                                                          */
  if (istr & USB_ISTAT_SLEEP_MASK) {
    USB_Suspend();
#ifdef __RTX
    if (USB_RTX_DevTask) {
      isr_evt_set(USB_EVT_SUSPEND, USB_RTX_DevTask);
    }
#else
 //   if (USB_P_Suspend_Event) {
 //     USB_P_Suspend_Event();
  //  }
#endif
    USB0->ISTAT = USB_ISTAT_SLEEP_MASK |
                  USB_ISTAT_RESUME_MASK;
  }

/* resume interrupt                                                           */
  if (istr & USB_ISTAT_RESUME_MASK) {
    USB_Resume();
#ifdef __RTX
    if (USB_RTX_DevTask) {
      isr_evt_set(USB_EVT_RESUME, USB_RTX_DevTask);
    }
#else	
		#if USB_RESUME_EVENT
    if(USB0->ISTAT & (1 << USB_ISTAT_RESUME_SHIFT)) {
      USB_Resume_Event();
    }
		#endif
#endif
    USB0->ISTAT = USB_ISTAT_RESUME_MASK;
  }


/* Start Of Frame                                                             */
  if (istr & USB_ISTAT_SOFTOK_MASK) {
#ifdef __RTX
    if (USB_RTX_DevTask) {
      isr_evt_set(USB_EVT_SOF, USB_RTX_DevTask);
    }
#else
		#if USB_SOF_EVENT
    if (USB0->ISTAT & (1 << USB_ISTAT_SOFTOK_SHIFT)) {
      USB_SOF_Event();
    }
		#endif
#endif
    USB0->ISTAT = USB_ISTAT_SOFTOK_MASK;
  }


/* Error interrupt                                                            */
  if (istr == USB_ISTAT_ERROR_MASK) {
#ifdef __RTX
    LastError = USB0->ERRSTAT;
    if (USB_RTX_DevTask) {
      isr_evt_set(USB_EVT_ERROR, USB_RTX_DevTask);
    }
#else
		#if USB_ERROR_EVENT
    if (USB0->ISTAT & (1 << USB_ISTAT_ERROR_SHIFT)) {
      USB_Error_Event(USB0->ERRSTAT);
    }
		#endif
#endif
    USB0->ERRSTAT = 0xFF;
    USB0->ISTAT = USB_ISTAT_ERROR_MASK;
  }


/* token interrupt                                                            */
  if (istr & USB_ISTAT_TOKDNE_MASK) {

    num    = (USB0->STAT >> 4) & 0x0F;
    dir    = (USB0->STAT >> 3) & 0x01;
    ev_odd = (USB0->STAT >> 2) & 0x01;

/* setup packet                                                               */
    if ((num == 0) && (TOK_PID((IDX(num, dir, ev_odd))) == SETUP_TOKEN)) {
      Data1 &= ~0x02;
      BD[IDX(0, TX, EVEN)].stat &= ~BD_OWN_MASK;
      BD[IDX(0, TX, ODD)].stat  &= ~BD_OWN_MASK;
#ifdef __RTX
        if (USB_RTX_EPTask[num]) {
          isr_evt_set(USB_EVT_SETUP, USB_RTX_EPTask[num]);
        }
#else
        if (USB_P_EP[num]) {
          USB_P_EP[num](USB_EVT_SETUP);
        }
#endif
    }
    else {

/* OUT packet                                                                 */
      if (TOK_PID((IDX(num, dir, ev_odd))) == OUT_TOKEN) {
#ifdef __RTX
        if (USB_RTX_EPTask[num]) {
          isr_evt_set(USB_EVT_OUT, USB_RTX_EPTask[num]);
        }
#else
        if (USB_P_EP[num]) {
          USB_P_EP[num](USB_EVT_OUT);
        }
#endif
      }

/* IN packet                                                                  */
      if (TOK_PID((IDX(num, dir, ev_odd))) == IN_TOKEN) {
#ifdef __RTX
        if (USB_RTX_EPTask[num]) {
          isr_evt_set(USB_EVT_IN,  USB_RTX_EPTask[num]);
        }
#else
        if (USB_P_EP[num]) {
          USB_P_EP[num](USB_EVT_IN);
        }
#endif
      }
    }
    USB0->ISTAT = USB_ISTAT_TOKDNE_MASK;
  }
}


