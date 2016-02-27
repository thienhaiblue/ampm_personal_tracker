/*------------------------------------------------------------------------------
 *      RL-ARM - USB
 *------------------------------------------------------------------------------
 *      Name:    usbd_hw.h
 *      Purpose: USB Device Hardware Layer header file
 *      Rev.:    V4.70
 *------------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------*/

#ifndef __USB_HW_H__
#define __USB_HW_H__
#include <stdint.h>
#include "usb.h"

/* USB Hardware Functions */
extern void USB_Init        (void);
extern void USB_Connect     (uint32_t con);
extern void USB_Reset       (void);
extern void USB_Suspend     (void);
extern void USB_Resume      (void);
extern void USB_WakeUp      (void);
extern void USB_WakeUpCfg   (uint32_t cfg);
extern void USB_SetAddress 	(uint32_t  adr);
extern void USB_Configure   (uint32_t cfg);
extern void USB_ConfigEP    (USB_ENDPOINT_DESCRIPTOR *pEPD);
extern void USB_DirCtrlEP   (uint32_t  dir);
extern void USB_EnableEP    (uint32_t  EPNum);
extern void USB_DisableEP   (uint32_t  EPNum);
extern void USB_ResetEP     (uint32_t  EPNum);
extern void USB_SetStallEP  (uint32_t  EPNum);
extern void USB_ClrStallEP  (uint32_t  EPNum);
extern void USB_ClearEPBuf  (uint32_t  EPNum);
extern uint32_t  USB_ReadEP      (uint32_t  EPNum, uint8_t *pData);
extern uint32_t  USB_WriteEP     (uint32_t  EPNum, uint8_t *pData, uint32_t cnt);
extern uint32_t  USB_GetFrame    (void);
extern uint32_t  USB_GetError    (void);


#endif  /* __USB_HW_H__ */
