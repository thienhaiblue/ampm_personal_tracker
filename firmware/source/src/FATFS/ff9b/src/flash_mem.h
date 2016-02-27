

#ifndef __FLASH_MEM_H__
#define __FLASH_MEM_H__
#include "diskio.h"
#include <stdint.h>

DSTATUS MMC_disk_initialize(void);
DSTATUS SST25_disk_status(void);
DRESULT SST25_disk_read(BYTE *buff, DWORD sector, BYTE count);
DRESULT SST25_disk_write(BYTE *buff, DWORD sector, BYTE count);
uint32_t get_fattime(void);
DRESULT SST25_disk_ioctl(BYTE ctrl, void* buff);


#endif
