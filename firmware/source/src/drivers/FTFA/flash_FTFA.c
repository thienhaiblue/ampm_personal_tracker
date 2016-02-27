/********************************************************************************

**************************************************************************//*!

 *****************************************************************************/
 #include "common.h"
#include "flash_FTFA.h" /* include flash driver header file */
#include <stdio.h>
#include <string.h>
/*********************************** Macros ************************************/

/*********************************** Defines ***********************************/
#define PROG_WORD_SIZE 30       /* adequate space for the small program */

/********************************** Constant ***********************************/

/*********************************** Variables *********************************/

/*********************************** Prototype *********************************/

/*********************************** Function **********************************/
extern void SpSub(void);
//static void SpSubEnd(void);


/*******************************************************************************
 * Function:        Flash_Init
 *
 * Description:     Set the flash clock
 *
 * Returns:         never return
 *
 * Notes:
 *
 *******************************************************************************/
void Flash_Init(int a)
{
		SIM_SCGC6 |= SIM_SCGC6_FTF_MASK;
    /* checking access error */
    if (FTFA_FSTAT & FTFA_FSTAT_ACCERR_MASK)
    {
        /* clear error flag */
    	FTFA_FSTAT |= FTFA_FSTAT_ACCERR_MASK;
    }
    /* checking protection error */
    else if (FTFA_FSTAT & FTFA_FSTAT_FPVIOL_MASK)
    {
    	/* clear error flag */
    	FTFA_FSTAT |= FTFA_FSTAT_FPVIOL_MASK;
    }
    else if (FTFA_FSTAT & FTFA_FSTAT_RDCOLERR_MASK)
     {
        	/* clear error flag */
        	FTFA_FSTAT |= FTFA_FSTAT_RDCOLERR_MASK;
     }
    /* Disable Data Cache in Flash memory Controller Module  */
#if (defined (__MK_xxx_H__))
    FMC_PFB0CR &= ~FMC_PFB0CR_B0DCE_MASK;
    FMC_PFB1CR &= ~FMC_PFB1CR_B1DCE_MASK;
#endif
}


/*******************************************************************************
 * Function:        Flash_SectorErase
 *
 * Description:     erase a sector of the flash
 *
 * Returns:         Error Code
 *
 * Notes:
 *
 *******************************************************************************/
 uint8_t Flash_SectorErase(uint32_t FlashPtr)
{
    uint8_t Return = Flash_OK;

    /* Allocate space on stack to run flash command out of SRAM */
    /* wait till CCIF is set*/
    while (!(FTFA_FSTAT & FTFA_FSTAT_CCIF_MASK)){};
    /* Write command to FCCOB registers */
    FTFA_FCCOB0 = FlashCmd_SectorErase;
    FTFA_FCCOB1 = (uint8_t)(FlashPtr >> 16);
    FTFA_FCCOB2 = (uint8_t)((FlashPtr >> 8) & 0xFF);
    FTFA_FCCOB3 = (uint8_t)(FlashPtr & 0xFF);
		
		/* Initialize user application's Stack Pointer */
		//__set_MSP(usProgSpace[0]);
   
    SpSub();

    /* checking access error */
    if (FTFA_FSTAT & FTFA_FSTAT_ACCERR_MASK)
    {
        /* clear error flag */
    	FTFA_FSTAT |= FTFA_FSTAT_ACCERR_MASK;

        /* update return value*/
        Return |= Flash_FACCERR;
        }
    /* checking protection error */
    else if (FTFA_FSTAT & FTFA_FSTAT_FPVIOL_MASK)
    {
    	/* clear error flag */
    	FTFA_FSTAT |= FTFA_FSTAT_FPVIOL_MASK;

        /* update return value*/
        Return |= Flash_FPVIOL;
    }
    else if (FTFA_FSTAT & FTFA_FSTAT_RDCOLERR_MASK)
    {
       	/* clear error flag */
       	FTFA_FSTAT |= FTFA_FSTAT_RDCOLERR_MASK;

           /* update return value*/
           Return |= Flash_RDCOLERR;
    }
    /* checking MGSTAT0 non-correctable error */
    else if (FTFA_FSTAT & FTFA_FSTAT_MGSTAT0_MASK)
    {
    	Return |= Flash_MGSTAT0;
    }
    /* function return */
    return  Return;
}


/*******************************************************************************
 * Function:        Flash_ByteProgram
 *
 * Description:     byte program the flash
 *
 * Returns:         Error Code
 *
 * Notes:
 *
 *******************************************************************************/
uint8_t Flash_ByteProgram(uint32_t FlashStartAdd,uint32_t *DataSrcPtr,uint32_t NumberOfBytes)
{
    uint8_t Return = Flash_OK;
    uint32_t size_buffer;

    if (NumberOfBytes == 0)
    {
    	return Flash_CONTENTERR;
    }
    else
    {
    	size_buffer = (NumberOfBytes - 1)/4 + 1;	
    }
    /* wait till CCIF is set*/
    while (!(FTFA_FSTAT & FTFA_FSTAT_CCIF_MASK)){};
        
	while ((size_buffer) && (Return == Flash_OK))
	{
		/* Write command to FCCOB registers */
		FTFA_FCCOB0 = FlashCmd_ProgramLongWord;
		FTFA_FCCOB1 = (uint8_t)(FlashStartAdd >> 16);
		FTFA_FCCOB2 = (uint8_t)((FlashStartAdd >> 8) & 0xFF);
		FTFA_FCCOB3 = (uint8_t)(FlashStartAdd & 0xFF);
//#ifdef __MK_xxx_H__ /*little endian*/
		FTFA_FCCOB4 = (uint8_t)(*((uint8_t*)DataSrcPtr+3));
		FTFA_FCCOB5 = (uint8_t)(*((uint8_t*)DataSrcPtr+2));
		FTFA_FCCOB6 = (uint8_t)(*((uint8_t*)DataSrcPtr+1));
		FTFA_FCCOB7 = (uint8_t)(*((uint8_t*)DataSrcPtr+0));
// #else /* Big endian */
// 		
// 		FTFA_FCCOB4 = (uint8_t)(*((uint8_t*)DataSrcPtr+0));
// 		FTFA_FCCOB5 = (uint8_t)(*((uint8_t*)DataSrcPtr+1));
// 		FTFA_FCCOB6 = (uint8_t)(*((uint8_t*)DataSrcPtr+2));
// 		FTFA_FCCOB7 = (uint8_t)(*((uint8_t*)DataSrcPtr+3));
// #endif		
		/* Launch command */
      SpSub();

        /* checking access error */
	    if (FTFA_FSTAT & FTFA_FSTAT_ACCERR_MASK)
	    {
	        /* clear error flag */
	    	FTFA_FSTAT |= FTFA_FSTAT_ACCERR_MASK;

	        /* update return value*/
	        Return |= Flash_FACCERR;
	        }
	    /* checking protection error */
	    else if (FTFA_FSTAT & FTFA_FSTAT_FPVIOL_MASK)
	    {
	    	/* clear error flag */
	    	FTFA_FSTAT |= FTFA_FSTAT_FPVIOL_MASK;

	        /* update return value*/
	        Return |= Flash_FPVIOL;
	    }
	    else if (FTFA_FSTAT & FTFA_FSTAT_RDCOLERR_MASK)
	     {
	        	/* clear error flag */
	        	FTFA_FSTAT |= FTFA_FSTAT_RDCOLERR_MASK;

	            /* update return value*/
	            Return |= Flash_RDCOLERR;
	     }
	    /* checking MGSTAT0 non-correctable error */
	    else if (FTFA_FSTAT & FTFA_FSTAT_MGSTAT0_MASK)
	    {
	    	Return |= Flash_MGSTAT0;
	    }
		/* decrement byte count */
		 size_buffer --;
		 (uint32_t *)DataSrcPtr++;
		 FlashStartAdd +=4;
	}
    /* function return */
    return  Return;
}

// /*******************************************************************************
//  * Function:        SpSub
//  *
//  * Description:     Execute the Flash write while running out of SRAM
//  *
//  * Returns:
//  *
//  * Notes:
//  *
//  *******************************************************************************/
// static void SpSub(void)
//     {
//         /* Launch command */
//         FTFA_FSTAT |= FTFA_FSTAT_CCIF_MASK;    
//         /* wait for command completion */
//         while (!(FTFA_FSTAT & FTFA_FSTAT_CCIF_MASK)) {};
//     }

// /* Leave this immediately after SpSub */
// static void SpSubEnd(void) {}


