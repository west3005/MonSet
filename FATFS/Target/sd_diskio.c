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

#include "sdio.h"              // hsd
#include "stm32f4xx_hal_sd.h"  // HAL_SD_*
#include "main.h"

#include <string.h>

/* use the default SD timeout as defined in HAL driver */
#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT (30U * 1000U)
#endif

#define SD_DEFAULT_BLOCK_SIZE 512

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* Private helpers -----------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
static HAL_StatusTypeDef SD_WaitCardReady(uint32_t timeout_ms);

/* Exported FatFs diskio functions -------------------------------------------*/
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif
#if _USE_IOCTL == 1
DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if  _USE_WRITE == 1
  SD_write,
#endif
#if  _USE_IOCTL == 1
  SD_ioctl,
#endif
};

/* Private functions ---------------------------------------------------------*/

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
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

/**
  * @brief  Initializes a Drive
  */
DSTATUS SD_initialize(BYTE lun)
{
  (void)lun;
  Stat = STA_NOINIT;

  /* SDIO + GPIO уже инициализированы через MX_SDIO_SD_Init().
     Здесь просто проверяем, что карта жива. */

  if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
  {
    Stat = SD_CheckStatus(lun);
  }
  else
  {
    /* Попробовать реинициализировать карту */
    if (HAL_SD_DeInit(&hsd) == HAL_OK)
    {
      if (HAL_SD_Init(&hsd) == HAL_OK)
      {
        if (SD_WaitCardReady(2000) == HAL_OK)
        {
          Stat = SD_CheckStatus(lun);
        }
      }
    }
  }

  return Stat;
}

/**
  * @brief  Gets Disk Status
  */
DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

/**
  * @brief  Reads Sector(s)
  */
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  if (buff == NULL || count == 0U)
    return RES_PARERR;

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

/**
  * @brief  Writes Sector(s)
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  if (buff == NULL || count == 0U)
    return RES_PARERR;

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
#endif /* _USE_WRITE */

/**
  * @brief  I/O control operation
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  (void)lun;
  DRESULT res = RES_ERROR;

  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  switch (cmd)
  {
    case CTRL_SYNC:
      /* Ничего не делаем, просто убеждаемся, что карта готова */
      if (SD_WaitCardReady(SD_TIMEOUT) == HAL_OK)
        res = RES_OK;
      else
        res = RES_ERROR;
      break;

    case GET_SECTOR_COUNT:
    {
      HAL_SD_CardInfoTypeDef info;
      if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK)
      {
        *(DWORD *)buff = info.LogBlockNbr;
        res = RES_OK;
      }
      break;
    }

    case GET_SECTOR_SIZE:
    {
      HAL_SD_CardInfoTypeDef info;
      if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK)
      {
        *(WORD *)buff = (WORD)info.LogBlockSize;
        res = RES_OK;
      }
      break;
    }

    case GET_BLOCK_SIZE:
    {
      HAL_SD_CardInfoTypeDef info;
      if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK)
      {
        *(DWORD *)buff = info.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
        res = RES_OK;
      }
      break;
    }

    default:
      res = RES_PARERR;
      break;
  }

  return res;
}
#endif /* _USE_IOCTL */
