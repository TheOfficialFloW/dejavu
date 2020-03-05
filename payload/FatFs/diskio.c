#include <string.h>

#include "ff.h"
#include "diskio.h"
#include "ms.h"

/* Definitions of physical drive number for each drive */
#define DEV_MEMORY_CARD		0	/* Memory Card */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv
)
{
	switch (pdrv) {
	case DEV_MEMORY_CARD:
		return RES_OK;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv
)
{
	switch (pdrv) {
	case DEV_MEMORY_CARD:
		return RES_OK;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv) {
	case DEV_MEMORY_CARD:
		if ((uintptr_t)buff & 0x3) {
			static BYTE __attribute__((aligned(64))) temp[MS_SECTOR_SIZE];
			for (LBA_t i = 0; i < count; i++) {
				if (ms_read_sector(temp, sector + i, 1) != 0)
					return RES_PARERR;
				memcpy(buff, temp, MS_SECTOR_SIZE);
				buff += MS_SECTOR_SIZE;
			}
			return RES_OK;
		} else {
			if (ms_read_sector(buff, sector, count) != 0)
				return RES_PARERR;
			return RES_OK;
		}
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	switch (pdrv) {
	case DEV_MEMORY_CARD:
		if ((uintptr_t)buff & 0x3) {
			static BYTE __attribute__((aligned(64))) temp[MS_SECTOR_SIZE];
			for (LBA_t i = 0; i < count; i++) {
				memcpy(temp, buff, MS_SECTOR_SIZE);
				if (ms_write_sector(temp, sector + i, 1) != 0)
					return RES_PARERR;
				buff += MS_SECTOR_SIZE;
			}
			return RES_OK;
		} else {
			if (ms_write_sector(buff, sector, count) != 0)
				return RES_PARERR;
			return RES_OK;
		}
	}

	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (pdrv) {
	case DEV_MEMORY_CARD:
		switch (cmd) {
			case CTRL_SYNC:
				return RES_OK;
			default:
				return RES_PARERR;
		}
	}

	return RES_PARERR;
}

