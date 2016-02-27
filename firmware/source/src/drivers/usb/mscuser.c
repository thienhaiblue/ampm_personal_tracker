/*----------------------------------------------------------------------------
 *      U S B  -  K e r n e l
 *----------------------------------------------------------------------------
 *      Name:    MSCUSER.C
 *      Purpose: Mass Storage Class Custom User Module
 *      Version: V1.10
 *----------------------------------------------------------------------------
 *      This software is supplied "AS IS" without any warranties, express,
 *      implied or statutory, including but not limited to the implied
 *      warranties of fitness for purpose, satisfactory quality and
 *      noninfringement. Keil extends you a royalty-free right to reproduce
 *      and distribute executable files created using this software for use
 *      on NXP Semiconductors LPC family microcontroller devices only. Nothing
 *      else gives you the right to use this software.
 *
 *      Copyright (c) 2005-2009 Keil Software.
 *---------------------------------------------------------------------------*/


#include <string.h>

#include "usb.h"
#include "msc.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "mscuser.h"
#include "common.h"
#include "spi.h"
#include "sst25.h"



uint32_t  MemOK;                   /* Memory OK */

uint32_t index_of_data_transfer;
uint32_t index_of_data_written;
uint32_t SST25_sectorOffset = 0,dataOffset = 0,dataLoaded = 0,sectorWriten = 0;

uint32_t current_sector = 0;
uint32_t data_counter = 0;
uint32_t number_of_sector_transfer;

uint32_t writeNumCnt = 0;

uint8_t  buff_data[128];

uint8_t  BulkStage;               /* Bulk Stage */

uint8_t  BulkBuf[MSC_MAX_PACKET]; /* Bulk In/Out Buffer */
uint8_t  BulkLen;                 /* Bulk In/Out Length */

MSC_CBW CBW;                   /* Command Block Wrapper */
MSC_CSW CSW;                   /* Command Status Wrapper */


/*
 *  MSC Mass Storage Reset Request Callback
 *   Called automatically on Mass Storage Reset Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

uint32_t MSC_Reset (void) {

  BulkStage = MSC_BS_CBW;
  return (TRUE);
}


/*
 *  MSC Get Max LUN Request Callback
 *   Called automatically on Get Max LUN Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

uint32_t MSC_GetMaxLUN (void) {

  EP0Buf[0] = 0;               /* No LUN associated with this device */
  return (TRUE);
}


/*
 *  MSC Memory Read Callback
 *   Called automatically on Memory Read Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */


// void MSC_MemoryRead (void) {

// 	if ((current_sector*MSC_SECTOR_SIZE + index_of_data_transfer) >= MSC_MEMORY_SIZE) {
// 		current_sector =  MSC_NUM_SECTOR_MAX;
// 		BulkStage = MSC_BS_DATA_IN_LAST_STALL;
// 	}
// 	SST25_Read(current_sector*MSC_SECTOR_SIZE + MSC_FLASH_START + index_of_data_transfer,buff_data,MSC_MAX_PACKET);
// 	USB_WriteEP(MSC_EP_IN, buff_data,MSC_MAX_PACKET);
// 	index_of_data_transfer += MSC_MAX_PACKET;
// 	if(index_of_data_transfer >= MSC_SECTOR_SIZE)
// 	{
// 		number_of_sector_transfer--;
// 		current_sector++;
// 		index_of_data_transfer = 0;
// 		if(number_of_sector_transfer == 0)
// 		{
// 			BulkStage = MSC_BS_DATA_IN_LAST;
// 		}
// 	}
// 	CSW.dDataResidue -= MSC_MAX_PACKET;
//   if (BulkStage != MSC_BS_DATA_IN) {
//     CSW.bStatus = CSW_CMD_PASSED;
//   }
// }

void MSC_MemoryRead (void) {

	if ((current_sector*MSC_SECTOR_SIZE + index_of_data_transfer) >= MSC_MEMORY_SIZE) {
		current_sector =  MSC_NUM_SECTOR_MAX;
		BulkStage = MSC_BS_DATA_IN_LAST_STALL;
	}
	SST25_Read(current_sector*MSC_SECTOR_SIZE + MSC_FLASH_START + index_of_data_transfer,buff_data,MSC_MAX_PACKET);
	USB_WriteEP(MSC_EP_IN, buff_data,MSC_MAX_PACKET);
	index_of_data_transfer += MSC_MAX_PACKET;
	if(index_of_data_transfer >= MSC_SECTOR_SIZE)
	{
		number_of_sector_transfer--;
		current_sector++;
		index_of_data_transfer = 0;
		if(number_of_sector_transfer == 0)
		{
			BulkStage = MSC_BS_DATA_IN_LAST;
		}
	}
	CSW.dDataResidue -= MSC_MAX_PACKET;
  if (BulkStage != MSC_BS_DATA_IN) {
    CSW.bStatus = CSW_CMD_PASSED;
  }
}


/* 
 *  MSC Memory Write Callback
 *   Called automatically on Memory Write Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_MemoryWrite (void) 
{	
  //printf_dbg("MSC_MemoryWrite(); 2A\r\n");
  if (current_sector >= MSC_NUM_SECTOR_MAX) {
    BulkStage = MSC_BS_CSW;
    USB_SetStallEP(MSC_EP_OUT);
  }
  index_of_data_transfer +=  BulkLen;
  CSW.dDataResidue -= BulkLen;
  if(index_of_data_transfer >= MSC_SECTOR_SIZE)
  {
		number_of_sector_transfer--;
		current_sector++;
		index_of_data_transfer = 0;
  }
  if ((number_of_sector_transfer == 0) || (BulkStage == MSC_BS_CSW)) {
    CSW.bStatus = CSW_CMD_PASSED;
    MSC_SetCSW();
  }
}
	
// void MSC_MemoryWrite (void) {
// 	uint32_t n,i,writeFlag = 0,j;
// 	
//   //printf_dbg("MSC_MemoryWrite(); 2A\r\n");
//   if (current_sector >= MSC_NUM_SECTOR_MAX) {
//     BulkStage = MSC_BS_CSW;
//     USB_SetStallEP(MSC_EP_OUT);
//   }
// 	if(index_of_data_transfer == 0)
// 	{
// 		n = current_sector * MSC_SECTOR_SIZE;
// 		if((dataLoaded != 0xa5a5a5a5) || (SST25_sectorOffset != (n / SECTOR_SIZE)))
// 		{
// 			if((dataLoaded == 0xa5a5a5a5) && (sectorWriten == 0))
// 			{
// 				for(i = 0 ; i < SECTOR_SIZE; i += MSC_MAX_PACKET)
// 				{
// 					SST25_Write(SST25_sectorOffset * SECTOR_SIZE + MSC_FLASH_START + i,&buff_data1[i],MSC_MAX_PACKET);
// 					SST25_Read(SST25_sectorOffset * SECTOR_SIZE + MSC_FLASH_START + i,buff_data,MSC_MAX_PACKET);
// 					for(j = 0; j < MSC_MAX_PACKET ; j++)
// 					{
// 						if(buff_data1[i + j] != buff_data[j])
// 						{
// 							CSW.bStatus = CSW_CMD_FAILED;
// 							MSC_SetCSW();
// 							return;
// 						}
// 					}
// 					writeNumCnt++;
// 				}
// 			}
// 			SST25_sectorOffset = n / SECTOR_SIZE;
// 			dataLoaded = 0xa5a5a5a5;
// 			for(j = 0; j < SECTOR_SIZE; j += MSC_MAX_PACKET)
// 				SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + j,&buff_data1[j],MSC_MAX_PACKET);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START,buff_data1,512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 1 * 512,&buff_data1[1 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 2 * 512,&buff_data1[2 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 3 * 512,&buff_data1[3 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 4 * 512,&buff_data1[4 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 5 * 512,&buff_data1[5 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 6 * 512,&buff_data1[6 * 512],512);
// // 			SST25_Read(SST25_sectorOffset*SECTOR_SIZE + MSC_FLASH_START + 7 * 512,&buff_data1[7 * 512],512);
// 			sectorWriten = 0;
// 		}
// 		dataOffset = n - SST25_sectorOffset *  SECTOR_SIZE;
// 	}
// 	for (i = 0;(i < BulkLen) && ((index_of_data_transfer + i) <= MSC_SECTOR_SIZE); i++)
// 	{
// 		buff_data1[dataOffset + index_of_data_transfer + i] = BulkBuf[i];
// 	}
//   index_of_data_transfer +=  BulkLen;
//   CSW.dDataResidue -= BulkLen;
//   if(index_of_data_transfer >= MSC_SECTOR_SIZE)
//   {
// 		if(dataOffset >= SECTOR_SIZE)
// 		{
// 			writeFlag = 1;
// 		}
// 		number_of_sector_transfer--;
// 		current_sector++;
// 		index_of_data_transfer = 0;
//   }
// 	
// 	if((number_of_sector_transfer == 0) || (writeFlag))
// 	{
// 		for(i = 0 ; i < SECTOR_SIZE;i += MSC_MAX_PACKET)
// 		{
// 			SST25_Write(SST25_sectorOffset * SECTOR_SIZE + MSC_FLASH_START + i,&buff_data1[i],MSC_MAX_PACKET);
// 			SST25_Read(SST25_sectorOffset * SECTOR_SIZE + MSC_FLASH_START + i,buff_data,MSC_MAX_PACKET);
// 			for(j = 0; j < MSC_MAX_PACKET ; j++)
// 			{
// 				if(buff_data1[i + j] != buff_data[j])
// 				{
// 					CSW.bStatus = CSW_CMD_FAILED;
// 					MSC_SetCSW();
// 					return;
// 				}
// 			}
// 			writeNumCnt++;
// 		}
// 		sectorWriten = 1;
// 	}

//   if ((number_of_sector_transfer == 0) || (BulkStage == MSC_BS_CSW)) {
//     CSW.bStatus = CSW_CMD_PASSED;
//     MSC_SetCSW();
//   }
// }

// void MSC_MemoryWrite (void) {
// 	uint32_t n;
// 	static uint32_t index = 0,index_of_data_written = 0;
//   //printf_dbg("MSC_MemoryWrite(); 2A\r\n");
//   if (current_sector >= MSC_NUM_SECTOR_MAX) {
//     BulkStage = MSC_BS_CSW;
//     USB_SetStallEP(MSC_EP_OUT);
//   }
// 	if(index_of_data_written == 0)
// 		index = 0;
// 	for (n = 0;(n < BulkLen) && ((index_of_data_transfer + n) < MSC_SECTOR_SIZE); n++)
// 	{
// 		buff_data1[index] = BulkBuf[n];
// 		index++;
// 		if(index >= MSC_MAX_PACKET)
// 		{
// 			SST25_Write(current_sector*MSC_SECTOR_SIZE + MSC_FLASH_START + index_of_data_written,buff_data1,MSC_MAX_PACKET);
// 			index_of_data_written += MSC_MAX_PACKET;
// 			index = 0;
// 		}
// 	}
//   index_of_data_transfer +=  BulkLen;
//   CSW.dDataResidue -= BulkLen;
//   if(index_of_data_transfer >= MSC_SECTOR_SIZE)
//   { 
// 		number_of_sector_transfer--;
// 		current_sector++;
// 		index_of_data_transfer = 0;
// 		index_of_data_written	= 0;
//   }

//   if ((number_of_sector_transfer == 0) || (BulkStage == MSC_BS_CSW)) {
//     CSW.bStatus = CSW_CMD_PASSED;
//     MSC_SetCSW();
//   }
// }


/*
 *  MSC Memory Verify Callback
 *   Called automatically on Memory Verify Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_MemoryVerify (void) {
  uint32_t n;
  //printf_dbg("MSC_MemoryVerify(); 2A\r\n");
  if (current_sector >= MSC_NUM_SECTOR_MAX) {
    BulkStage = MSC_BS_CSW;
    USB_SetStallEP(MSC_EP_OUT);
  }
//	SST25_Read(current_sector*MSC_SECTOR_SIZE + MSC_FLASH_START + index_of_data_transfer,buff_data,MSC_MAX_PACKET);
  for (n = 0;(n < BulkLen) && ((index_of_data_transfer + n) < MSC_SECTOR_SIZE); n++) {
//     if(buff_data[n] != BulkBuf[n])
// 		{
// 			MemOK = FALSE;
// 			break;
// 		}
  } 
  index_of_data_transfer +=  n;
  CSW.dDataResidue -= n;
  if(index_of_data_transfer >= MSC_SECTOR_SIZE)
  {
  	number_of_sector_transfer--;
		current_sector++;
		index_of_data_transfer = 0;
  }

  if ((number_of_sector_transfer == 0) || (BulkStage == MSC_BS_CSW)) {
    CSW.bStatus = (MemOK) ? CSW_CMD_PASSED : CSW_CMD_FAILED;
    MSC_SetCSW();
  }
}


/*
 *  MSC SCSI Read/Write Setup Callback
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

uint32_t MSC_RWSetup (void) {
  /* Logical Block Address of First Block */
  current_sector = (CBW.CB[2] << 24) |
      (CBW.CB[3] << 16) |
      (CBW.CB[4] <<  8) |
      (CBW.CB[5] <<  0);

  //printf_dbg("addr_sec: %d\r\n",current_sector);
  /* Number of Blocks to transfer */
  number_of_sector_transfer = ((uint32_t)CBW.CB[7] <<  8) | ((uint32_t)CBW.CB[8] <<  0);
  //printf_dbg("num_sec: %d\r\n",number_of_sector_transfer);
	index_of_data_written = 0;
  index_of_data_transfer = 0;
  if (CBW.dDataLength != number_of_sector_transfer*MSC_SECTOR_SIZE) {
    USB_SetStallEP(MSC_EP_IN);
    USB_SetStallEP(MSC_EP_OUT);
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }

  return (TRUE);
}


/*
 *  Check Data IN Format
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

uint32_t DataInFormat (void) {

  if (CBW.dDataLength == 0) {
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }
  if ((CBW.bmFlags & 0x80) == 0) {
    USB_SetStallEP(MSC_EP_OUT);
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }
  return (TRUE);
}


/*
 *  Perform Data IN Transfer
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

void DataInTransfer (void) {

  if (BulkLen > CBW.dDataLength) {
    BulkLen = CBW.dDataLength;
  }

  USB_WriteEP(MSC_EP_IN, BulkBuf, BulkLen);
  BulkStage = MSC_BS_DATA_IN_LAST;

  CSW.dDataResidue -= BulkLen;
  CSW.bStatus = CSW_CMD_PASSED;
}


/*
 *  MSC SCSI Test Unit Ready Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_TestUnitReady (void) {

  if (CBW.dDataLength != 0) {
    if ((CBW.bmFlags & 0x80) != 0) {
      USB_SetStallEP(MSC_EP_IN);
    } else {
      USB_SetStallEP(MSC_EP_OUT);
    }
  }

  CSW.bStatus = CSW_CMD_PASSED;
  MSC_SetCSW();
}


/*
 *  MSC SCSI Request Sense Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_RequestSense (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x70;          /* Response Code */
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x05;          /* Sense Key */
  BulkBuf[ 3] = 0x00;
  BulkBuf[ 4] = 0x00;
  BulkBuf[ 5] = 0x00;
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x0C;          /* Additional Length */
  BulkBuf[ 8] = 0x00;
  BulkBuf[ 9] = 0x00;
  BulkBuf[10] = 0x00;
  BulkBuf[11] = 0x00;
  BulkBuf[12] = 0x24;          /* ASC */
  BulkBuf[13] = 0x00;          /* ASCQ */
  BulkBuf[14] = 0x00;
  BulkBuf[15] = 0x00;
  BulkBuf[16] = 0x00;
  BulkBuf[17] = 0x00;

  BulkLen = 18;
  DataInTransfer();
}


/*
 *  MSC SCSI Inquiry Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_Inquiry (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;          /* Direct Access Device */
  BulkBuf[ 1] = 0x80;          /* RMB = 1: Removable Medium */
  BulkBuf[ 2] = 0x00;          /* Version: No conformance claim to standard */
  BulkBuf[ 3] = 0x01;

  BulkBuf[ 4] = 36-4;          /* Additional Length */
  BulkBuf[ 5] = 0x80;          /* SCCS = 1: Storage Controller Component */
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x00;

  BulkBuf[ 8] = 'B';           /* Vendor Identification */
  BulkBuf[ 9] = 'l';
  BulkBuf[10] = 'u';
  BulkBuf[11] = 'e';
  BulkBuf[12] = 'M';
  BulkBuf[13] = 'o';
  BulkBuf[14] = 'o';
  BulkBuf[15] = 'n';

  BulkBuf[16] = 'C';           /* Product Identification */
  BulkBuf[17] = 'a';
  BulkBuf[18] = 'r';
  BulkBuf[19] = 'd';
  BulkBuf[20] = ' ';
  BulkBuf[21] = 'R';
  BulkBuf[22] = 'e';
  BulkBuf[23] = 'a';
  BulkBuf[24] = 'd';
  BulkBuf[25] = 'e';
  BulkBuf[26] = 'r';
  BulkBuf[27] = ' ';
  BulkBuf[28] = ' ';
  BulkBuf[29] = ' ';
  BulkBuf[30] = ' ';
  BulkBuf[31] = ' ';

  BulkBuf[32] = '1';           /* Product Revision Level */
  BulkBuf[33] = '.';
  BulkBuf[34] = '0';
  BulkBuf[35] = ' ';

  BulkLen = 36;
  DataInTransfer();
}


/*
 *  MSC SCSI Mode Sense (6-Byte) Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ModeSense6 (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x03;
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x00;

  BulkLen = 4;
  DataInTransfer();
}


/*
 *  MSC SCSI Mode Sense (10-Byte) Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ModeSense10 (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;
  BulkBuf[ 1] = 0x06;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x00;
  BulkBuf[ 4] = 0x00;
  BulkBuf[ 5] = 0x00;
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x00;

  BulkLen = 8;
  DataInTransfer();
}


/*
 *  MSC SCSI Read Capacity Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ReadCapacity (void) {

  if (!DataInFormat()) return;

  /* Last Logical Block */
  BulkBuf[ 0] = ((MSC_NUM_SECTOR_MAX- 1) >> 24) & 0xFF;
  BulkBuf[ 1] = ((MSC_NUM_SECTOR_MAX- 1) >> 16) & 0xFF;
  BulkBuf[ 2] = ((MSC_NUM_SECTOR_MAX- 1) >>  8) & 0xFF;
  BulkBuf[ 3] = ((MSC_NUM_SECTOR_MAX- 1) >>  0) & 0xFF;

  /* Block Length */
  BulkBuf[ 4] = 0x00;
  BulkBuf[ 5] = 0x00;
  BulkBuf[ 6] = (MSC_SECTOR_SIZE >>  8) & 0xFF;
  BulkBuf[ 7] = (MSC_SECTOR_SIZE >>  0) & 0xFF;

  BulkLen = 8;
  DataInTransfer();
}


/*
 *  MSC SCSI Read Format Capacity Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ReadFormatCapacity (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x08;          /* Capacity List Length */

  /* Block Count */
  BulkBuf[ 4] = (MSC_NUM_SECTOR_MAX>> 24) & 0xFF;
  BulkBuf[ 5] = (MSC_NUM_SECTOR_MAX>> 16) & 0xFF;
  BulkBuf[ 6] = (MSC_NUM_SECTOR_MAX>>  8) & 0xFF;
  BulkBuf[ 7] = (MSC_NUM_SECTOR_MAX>>  0) & 0xFF;

  /* Block Length */
  BulkBuf[8] = 0x02;          /* Descriptor Code: Formatted Media */
  BulkBuf[9] = 0x00;
  BulkBuf[10] = (MSC_SECTOR_SIZE >> 8) & 0xFF;
  BulkBuf[11] = (MSC_SECTOR_SIZE >>  0) & 0xFF;
  BulkBuf[12] = (MSC_NUM_SECTOR_MAX>> 24) & 0xFF;
  BulkBuf[13] = (MSC_NUM_SECTOR_MAX>> 16) & 0xFF;
  BulkBuf[14] = (MSC_NUM_SECTOR_MAX>>  8) & 0xFF;
  BulkBuf[15] = (MSC_NUM_SECTOR_MAX>>  0) & 0xFF;
  BulkBuf[16] = 0x00;
  BulkBuf[17] = 0x00;
  BulkBuf[18] = (MSC_SECTOR_SIZE >>  8) & 0xFF;
  BulkBuf[19] = (MSC_SECTOR_SIZE >>  0) & 0xFF;

  BulkLen = 20;
  DataInTransfer();
}


/*
 *  MSC Get Command Block Wrapper Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_GetCBW (void) {
  uint32_t n;
   ////printf_dbg("MSC_GetCBW();\r\n");
  for (n = 0; n < BulkLen; n++) {
    *((uint8_t *)&CBW + n) = BulkBuf[n];
  }
  if ((BulkLen == sizeof(CBW)) && (CBW.dSignature == MSC_CBW_Signature)) {
    /* Valid CBW */
    CSW.dTag = CBW.dTag;
    CSW.dDataResidue = CBW.dDataLength;
    if ((CBW.bLUN != 0) || (CBW.bCBLength < 1) || CBW.bCBLength > 16) {
fail: CSW.bStatus = CSW_CMD_FAILED;
      MSC_SetCSW();
    } else {
      switch (CBW.CB[0]) {
        case SCSI_TEST_UNIT_READY:	 //00 yes
          MSC_TestUnitReady();
		  //printf_dbg("MSC_TestUnitReady(); 00\r\n");
          break;
        case SCSI_REQUEST_SENSE:	//03 yes
          MSC_RequestSense();
		  //printf_dbg("MSC_RequestSense(); 03\r\n");
          break;
        case SCSI_FORMAT_UNIT:	
          goto fail;
        case SCSI_INQUIRY:
          MSC_Inquiry();		   //12 yes
		  //printf_dbg("MSC_Inquiry(); 12\r\n");
          break;
        case SCSI_START_STOP_UNIT:
          goto fail;
        case SCSI_MEDIA_REMOVAL:
          goto fail;
        case SCSI_MODE_SELECT6:
          goto fail;
        case SCSI_MODE_SENSE6:	 //1A yes
          MSC_ModeSense6();
		  //printf_dbg("MSC_ModeSense6(); 1A\r\n");
          break;
        case SCSI_MODE_SELECT10:	//55
          goto fail;
        case SCSI_MODE_SENSE10:		//5A
          MSC_ModeSense10();
		  //printf_dbg("5A\r\n");
          break;
        case SCSI_READ_FORMAT_CAPACITIES: //23 yes
          MSC_ReadFormatCapacity();
		  //printf_dbg("MSC_ReadFormatCapacity(); 23\r\n");
          break;
        case SCSI_READ_CAPACITY:		 //25yes
          MSC_ReadCapacity();
		  //printf_dbg("MSC_ReadCapacity(); 25\r\n");
          break;
        case SCSI_READ10:			   //28 yes
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) != 0) {
              BulkStage = MSC_BS_DATA_IN;
              MSC_MemoryRead();
            } else {
              USB_SetStallEP(MSC_EP_OUT);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
			//printf_dbg("28\r\n");
          }
          break;
        case SCSI_WRITE10:	   //2A yes
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) == 0) {
              BulkStage = MSC_BS_DATA_OUT;
            } else {
              USB_SetStallEP(MSC_EP_IN);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
          }
		  //printf_dbg("2A\r\n");
          break;
        case SCSI_VERIFY10:		   //2F 
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) == 0) {
              BulkStage = MSC_BS_DATA_OUT;
              MemOK = TRUE;
            } else {
              USB_SetStallEP(MSC_EP_IN);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
          }
		  //printf_dbg("2F\r\n");
          break;
        default:
          goto fail;
      }
    }
  } else {
    /* Invalid CBW */
    USB_SetStallEP(MSC_EP_IN);
    USB_SetStallEP(MSC_EP_OUT);
    BulkStage = MSC_BS_ERROR;
  }
}

/*
 *  MSC Set Command Status Wrapper Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_SetCSW (void) {
   //printf_dbg("MSC_SetCSW();\r\n");
  CSW.dSignature = MSC_CSW_Signature;
  USB_WriteEP(MSC_EP_IN, (uint8_t *)&CSW, sizeof(CSW));
  BulkStage = MSC_BS_CSW;
}


/*
 *  MSC Bulk In Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_BulkIn (void) {

  switch (BulkStage) {
    case MSC_BS_DATA_IN:
      switch (CBW.CB[0]) {
        case SCSI_READ10:
          MSC_MemoryRead();
          break;
      }
      break;
    case MSC_BS_DATA_IN_LAST:
      MSC_SetCSW();
      break;
    case MSC_BS_DATA_IN_LAST_STALL:
      USB_SetStallEP(MSC_EP_IN);
      MSC_SetCSW();
      break;
    case MSC_BS_CSW:
      BulkStage = MSC_BS_CBW;
      break;
  }
}


/*
 *  MSC Bulk Out Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_BulkOut (void) {

  BulkLen = USB_ReadEP(MSC_EP_OUT, BulkBuf);
  switch (BulkStage) {
    case MSC_BS_CBW:
      MSC_GetCBW();
      break;
    case MSC_BS_DATA_OUT:
      switch (CBW.CB[0]) {
        case SCSI_WRITE10:
          MSC_MemoryWrite();
          break;
        case SCSI_VERIFY10:
          MSC_MemoryVerify();
          break;
      }
      break;
    default:
      USB_SetStallEP(MSC_EP_OUT);
      CSW.bStatus = CSW_PHASE_ERROR;
      MSC_SetCSW();
      break;
  }
}
