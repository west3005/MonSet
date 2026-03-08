/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD Disk I/O driver
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include <stdio.h>

#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT (30U * 1000U)
#endif

#define SD_DEFAULT_BLOCK_SIZE 512U
#define SD_WAIT_READY_TIMEOUT_MS 3000U

static volatile DSTATUS Stat = STA_NOINIT;

static DSTATUS SD_CheckStatus(BYTE lun);
static DSTATUS SD_WaitReady(uint32_t timeout_ms);

DSTATUS SD_initialize(BYTE);
DSTATUS SD_status(BYTE);
DRESULT SD_read(BYTE, BYTE*, DWORD, UINT);

#if _USE_WRITE == 1
DRESULT SD_write(BYTE, const BYTE*, DWORD, UINT);
#endif

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE, BYTE, void*);
#endif

const Diskio_drvTypeDef SD_Driver = {
  SD_initialize,
  SD_status,
  SD_read,
#if _USE_WRITE == 1
  SD_write,
#endif
#if _USE_IOCTL == 1
  SD_ioctl,
#endif
};

static DSTATUS SD_CheckStatus(BYTE lun)
{
    (void)lun;
    Stat = STA_NOINIT;

    uint8_t st = BSP_SD_GetCardState();
    printf("[DISKIO] status: cardState=%u\r\n", (unsigned)st);

    if (st == MSD_OK) {
        Stat &= ~STA_NOINIT;
    }

    printf("[DISKIO] status: Stat=0x%02X\r\n", Stat);
    return Stat;
}

static DSTATUS SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) < timeout_ms) {
        uint8_t st = BSP_SD_GetCardState();
        if (st == MSD_OK) {
            return 0;
        }
    }

    return STA_NOINIT;
}

DSTATUS SD_initialize(BYTE lun)
{
    (void)lun;
    Stat = STA_NOINIT;

    printf("[DISKIO] newinit: begin\r\n");

#if !defined(DISABLE_SD_INIT)
    if (BSP_SD_Init() == MSD_OK) {
        Stat = SD_CheckStatus(lun);
    } else {
        printf("[DISKIO] newinit: BSP_SD_Init failed\r\n");
    }
#else
    Stat = SD_CheckStatus(lun);
#endif

    printf("[DISKIO] newinit: final Stat=0x%02X\r\n", Stat);
    return Stat;
}

DSTATUS SD_status(BYTE lun)
{
    return SD_CheckStatus(lun);
}

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    (void)lun;
    DRESULT res = RES_ERROR;

    printf("[DISKIO] read: sec=%lu cnt=%u\r\n",
           (unsigned long)sector, (unsigned)count);

    if (count == 0U) {
        printf("[DISKIO] read: invalid count=0\r\n");
        return RES_PARERR;
    }

    if (BSP_SD_ReadBlocks((uint32_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) != MSD_OK) {
        printf("[DISKIO] read: BSP_SD_ReadBlocks failed\r\n");
        return RES_ERROR;
    }

    uint32_t t0 = HAL_GetTick();
    while (BSP_SD_GetCardState() != MSD_OK) {
        if ((HAL_GetTick() - t0) > 3000U) {
            printf("[DISKIO] read: wait ready timeout\r\n");
            return RES_ERROR;
        }
        IWDG->KR = 0xAAAA;
    }

    printf("[DISKIO] read: OK\r\n");
    res = RES_OK;
    return res;
}


#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    (void)lun;
    DRESULT res = RES_ERROR;

    printf("[DISKIO] write: sec=%lu cnt=%u\r\n",
           (unsigned long)sector, (unsigned)count);

    if (count == 0U) {
        printf("[DISKIO] write: invalid count=0\r\n");
        return RES_PARERR;
    }

    if (BSP_SD_WriteBlocks((uint32_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) != MSD_OK) {
        printf("[DISKIO] write: BSP_SD_WriteBlocks failed\r\n");
        return RES_ERROR;
    }

    uint32_t t0 = HAL_GetTick();
    while (BSP_SD_GetCardState() != MSD_OK) {
        if ((HAL_GetTick() - t0) > 3000U) {
            printf("[DISKIO] write: wait ready timeout\r\n");
            return RES_ERROR;
        }
        IWDG->KR = 0xAAAA;
    }

    printf("[DISKIO] write: OK\r\n");
    res = RES_OK;
    return res;
}
#endif

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    (void)lun;
    DRESULT res = RES_ERROR;
    BSP_SD_CardInfo CardInfo;

    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        res = RES_OK;
        break;

    case GET_SECTOR_COUNT:
        BSP_SD_GetCardInfo(&CardInfo);
        *(DWORD*)buff = CardInfo.LogBlockNbr;
        res = RES_OK;
        break;

    case GET_SECTOR_SIZE:
        BSP_SD_GetCardInfo(&CardInfo);
        *(WORD*)buff = CardInfo.LogBlockSize;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        BSP_SD_GetCardInfo(&CardInfo);
        *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
        res = RES_OK;
        break;

    default:
        res = RES_PARERR;
        break;
    }

    return res;
}
#endif
