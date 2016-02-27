#include "spi.h"
#include "sst25.h"
#include "mscuser.h"
#include "flash_mem.h"
#include "diskio.h"

DSTATUS MMC_disk_initialize(void)
{
	return 0;
}

DSTATUS SST25_disk_status(void)
{
	return 0;
}

DRESULT SST25_disk_read(BYTE *buff, DWORD sector, BYTE count)
{
	return SST25_Read(sector*512 + MSC_FLASH_START,buff,512);
}

DRESULT SST25_disk_write(BYTE *buff, DWORD sector, BYTE count)
{
	return SST25_Write(sector*512 + MSC_FLASH_START,buff,512);
}

uint32_t get_fattime(void)
{
	DWORD v = 0;
	
	v = 2013 - 1980;
	v <<= 25;
	
	v |= (DWORD)1 << 21;
	v |= (DWORD)1 << 16;
	v |= (DWORD)0 << 11;
	v |= (WORD)0 << 5;
	v |= 0 >> 1;
	
	return v;
}

DRESULT SST25_disk_ioctl(BYTE ctrl, void* buff)
{
	DRESULT res;
	DWORD csize;
	
	res = RES_ERROR;
	switch (ctrl) {
		case CTRL_SYNC:
				res = RES_OK;
			break;
			
		case GET_SECTOR_COUNT:			// get number of sectors on disk
				csize = MSC_MEMORY_SIZE / 512;
				*(DWORD*)buff = (DWORD)csize;
				res = RES_OK;		
			break;
			
		case GET_SECTOR_SIZE : // get size of sectors on disk 
			*(WORD*) buff = 512;
			res = RES_OK;
			break;
			
		case GET_BLOCK_SIZE : 		// get erase block size in units of sectors
				res = RES_OK;		
			break;
		default:
			res = RES_PARERR;	
	}	
	return res;
}

