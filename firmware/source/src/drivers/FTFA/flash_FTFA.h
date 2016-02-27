/********************************************************************************
*
* Freescale Semiconductor Inc.
* (c) Copyright 2004-2010 Freescale Semiconductor, Inc.
* ALL RIGHTS RESERVED.
*
**************************************************************************//*!
 *
 * @file flash_FTFA.h
 *
 * @author
 *
 * @version
 *
 * @date
 *
 * @brief flash driver header file
 *
 *****************************************************************************/

/*********************************** Includes ***********************************/

/*********************************** Macros ************************************/

/*********************************** Defines ***********************************/
#ifndef _FLASH_FTFA_H_
#define _FLASH_FTFA_H_
#include <stdint.h>
/* error code */
#define Flash_OK          0x00
#define Flash_FACCERR     0x01
#define Flash_FPVIOL      0x02
#define Flash_MGSTAT0	  0x04
#define Flash_RDCOLERR	  0x08
#define Flash_NOT_ERASED   0x10
#define Flash_CONTENTERR   0x11

/* flash commands */
#define FlashCmd_ProgramLongWord  	0x06
#define FlashCmd_SectorErase    	0x09


/********************************** Constant ***********************************/

/*********************************** Variables *********************************/

/*********************************** Prototype *********************************/
void Flash_Init(int a);
unsigned char Flash_SectorErase(uint32_t FlashPtr);
unsigned char Flash_ByteProgram(uint32_t FlashStartAdd,uint32_t *DataSrcPtr,uint32_t NumberOfBytes);
#endif /*_FLASH_FTFA_H_*/
