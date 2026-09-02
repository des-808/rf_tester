/**
 * @file    sd_diskio.c
 * @brief   SD Card Disk I/O driver for FatFS integration
 *          Bridges FatFS diskio API with our SD_Card_* HAL driver
 */

#include "diskio.h"
#include "sd_card.h"
#include "ff_gen_drv.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Global state                                                        */
/* ------------------------------------------------------------------ */
static volatile DSTATUS s_disk_stat = STA_NOINIT;

/* ------------------------------------------------------------------ */
/*  Helper: translate SD_Status_t -> DSTATUS / DRESULT                 */
/* ------------------------------------------------------------------ */
static DSTATUS translate_init_status(SD_Status_t st)
{
    DSTATUS r = 0;
    if (st != SD_OK) r |= STA_NOINIT;
    if (st == SD_WRITE_PROTECTED || st == SD_WRITE_ERROR) r |= STA_PROTECT;
    return r;
}

/* ------------------------------------------------------------------ */
/*  FatFS diskio interface                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialize disk
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    (void)pdrv;
    SD_Status_t st = SD_Card_Init();
    s_disk_stat = translate_init_status(st);
    return s_disk_stat;
}

/**
 * @brief  Get disk status
 */
DSTATUS disk_status(BYTE pdrv)
{
    (void)pdrv;
    if (!SD_Card_IsPresent()) {
        s_disk_stat = STA_NOINIT;
    } else {
        s_disk_stat &= ~STA_NOINIT;
    }
    return s_disk_stat;
}

/**
 * @brief  Read sector(s)
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    (void)pdrv;
    if (s_disk_stat & STA_NOINIT) return RES_NOTRDY;
    if (!buff) return RES_PARERR;

    SD_Status_t st = SD_Card_ReadBlocks(sector, count, buff);
    return (st == SD_OK) ? RES_OK : RES_ERROR;
}

/**
 * @brief  Write sector(s)
 */
#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    (void)pdrv;
    if (s_disk_stat & STA_NOINIT) return RES_NOTRDY;
    if (!buff) return RES_PARERR;
    if (s_disk_stat & STA_PROTECT) return RES_WRPRT;

    SD_Status_t st = SD_Card_WriteBlocks(sector, count, buff);
    return (st == SD_OK) ? RES_OK : RES_ERROR;
}
#endif

/**
 * @brief  I/O control
 */
#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)pdrv;
    if (s_disk_stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT: {
        SD_CardInfo_t info;
        if (SD_Card_GetInfo(&info) != SD_OK) return RES_ERROR;
        *(DWORD *)buff = info.block_count;
        return RES_OK;
    }

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;  /* SD cards use 512-byte sectors */
        return RES_OK;

    case GET_BLOCK_SIZE: {
        /* Return approximate erase block size in sectors.
         * SD cards typically have 64-128 sector blocks. */
        *(DWORD *)buff = 128;
        return RES_OK;
    }

    default:
        return RES_PARERR;
    }
}
#endif

/**
 * @brief  Get current time for FAT timestamp
 *         Called when _FS_NORTC == 0. Since we use _FS_NORTC==1,
 *         this function is not used but must exist.
 */
DWORD get_fattime(void)
{
    /* Return "Jan 1 2026 00:00:00" as default timestamp */
    return ((2026UL - 1980) << 25)    /* Year */
         | (1UL << 21)                /* Month (Jan) */
         | (1UL << 16);              /* Day (1) */
}

/* ------------------------------------------------------------------ */
/*  FatFS Disk Driver Table                                            */
/* ------------------------------------------------------------------ */

const Diskio_drvTypeDef SD_Driver =
{
    disk_initialize,
    disk_status,
    disk_read,
#if _USE_WRITE == 1
    disk_write,
#endif
#if _USE_IOCTL == 1
    disk_ioctl,
#endif
};
