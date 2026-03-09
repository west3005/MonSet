/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD Disk I/O driver (direct HAL version for MonSet)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "main.h"

#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT (30U * 1000U)
#endif

#define SD_DEFAULT_BLOCK_SIZE 512

static volatile DSTATUS Stat = STA_NOINIT;

static DSTATUS SD_CheckStatus(BYTE lun);
static HAL_StatusTypeDef SD_WaitCardReady(uint32_t timeout_ms);

DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif
#if _USE_IOCTL == 1
DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif

const Diskio_drvTypeDef SD_Driver =
{
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

static HAL_StatusTypeDef SD_WaitCardReady(uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();

  while ((HAL_GetTick() - t0) < timeout_ms)
  {
    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
    {
      return HAL_OK;
    }
    HAL_Delay(1);
  }

  return HAL_TIMEOUT;
}

static DSTATUS SD_CheckStatus(BYTE lun)
{
  (void)lun;
  Stat = STA_NOINIT;

  if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
  {
    Stat &= (DSTATUS)~STA_NOINIT;
  }

  return Stat;
}

DSTATUS SD_initialize(BYTE lun)
{
  (void)lun;
  Stat = STA_NOINIT;

  if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
  {
    Stat = SD_CheckStatus(lun);
    return Stat;
  }

  if (HAL_SD_DeInit(&hsd) != HAL_OK)
  {
    return Stat;
  }

  if (HAL_SD_Init(&hsd) != HAL_OK)
  {
    return Stat;
  }

  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B) != HAL_OK)
  {
    return Stat;
  }

  if (SD_WaitCardReady(2000U) != HAL_OK)
  {
    return Stat;
  }

  Stat = SD_CheckStatus(lun);
  return Stat;
}

DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  if ((buff == NULL) || (count == 0U))
  {
    return RES_PARERR;
  }

  if (HAL_SD_ReadBlocks(&hsd,
                        (uint8_t *)buff,
                        (uint32_t)sector,
                        (uint32_t)count,
                        SD_TIMEOUT) != HAL_OK)
  {
    return RES_ERROR;
  }

  if (SD_WaitCardReady(SD_TIMEOUT) != HAL_OK)
  {
    return RES_ERROR;
  }

  return RES_OK;
}

#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  if ((buff == NULL) || (count == 0U))
  {
    return RES_PARERR;
  }

  if (HAL_SD_WriteBlocks(&hsd,
                         (uint8_t *)buff,
                         (uint32_t)sector,
                         (uint32_t)count,
                         SD_TIMEOUT) != HAL_OK)
  {
    return RES_ERROR;
  }

  if (SD_WaitCardReady(SD_TIMEOUT) != HAL_OK)
  {
    return RES_ERROR;
  }

  return RES_OK;
}
#endif

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  (void)lun;
  DRESULT res = RES_ERROR;

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  switch (cmd)
  {
    case CTRL_SYNC:
      res = (SD_WaitCardReady(SD_TIMEOUT) == HAL_OK) ? RES_OK : RES_ERROR;
      break;

    case GET_SECTOR_COUNT:
    {
      HAL_SD_CardInfoTypeDef info;
      HAL_SD_GetCardInfo(&hsd, &info);
      *(DWORD *)buff = info.LogBlockNbr;
      res = RES_OK;
      break;
    }

    case GET_SECTOR_SIZE:
    {
      HAL_SD_CardInfoTypeDef info;
      HAL_SD_GetCardInfo(&hsd, &info);
      *(WORD *)buff = (WORD)info.LogBlockSize;
      res = RES_OK;
      break;
    }

    case GET_BLOCK_SIZE:
    {
      HAL_SD_CardInfoTypeDef info;
      HAL_SD_GetCardInfo(&hsd, &info);
      *(DWORD *)buff = info.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
      res = RES_OK;
      break;
    }

    default:
      res = RES_PARERR;
      break;
  }

  return res;
}
#endif
