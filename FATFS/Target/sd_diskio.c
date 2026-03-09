/**
 * @file sd_diskio.c
 * @brief FatFS diskio через HAL_SD (polling mode).
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ff_gen_drv.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_sd.h"
#include "stm32f4xx_hal_uart.h"

extern SD_HandleTypeDef hsd;
extern UART_HandleTypeDef huart1;

#define SD_TIMEOUT_MS 10000U
#define SD_READY_WAIT_MS 2000U

static volatile DSTATUS Stat = STA_NOINIT;

/* C-логгер */
static void diskio_log(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, (uint16_t)len, 200);
    }
}

/* Прототипы */
DSTATUS SD_initialize(BYTE pdrv);
DSTATUS SD_status(BYTE pdrv);
DRESULT SD_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
DRESULT SD_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE pdrv, BYTE cmd, void *buff);
#endif

/* 🔴 ИМЯ СТРУКТУРЫ SD_Driver для совместимости с MX */
const Diskio_drvTypeDef SD_Driver = {
    SD_initialize,
    SD_status,
    SD_read,
#if _USE_WRITE
    SD_write,
#endif
#if _USE_IOCTL == 1
    SD_ioctl,
#endif
};

/* Ждём TRANSFER state */
static uint8_t sd_wait_ready(uint32_t timeout_ms)
{
    uint32_t tick = HAL_GetTick();
    HAL_SD_CardStateTypeDef state = HAL_SD_CARD_ERROR;  // ← Инициализация!

    while ((HAL_GetTick() - tick) < timeout_ms) {
        state = HAL_SD_GetCardState(&hsd);
        if (state == HAL_SD_CARD_TRANSFER) {
            return 1;
        }
        if (state == HAL_SD_CARD_ERROR) {
            diskio_log("[DISKIO] wait: CARD ERROR\r\n");
            return 0;
        }
        IWDG->KR = 0xAAAA;
    }

    diskio_log("[DISKIO] wait: TIMEOUT state=%d err=0x%08lX\r\n",
               (int)state, HAL_SD_GetError(&hsd));
    return 0;
}

DSTATUS SD_initialize(BYTE pdrv)
{
    HAL_SD_CardInfoTypeDef info;

    if (pdrv != 0) return STA_NOINIT;
    Stat = STA_NOINIT;

    diskio_log("[DISKIO] init: begin\r\n");

    if (HAL_SD_GetState(&hsd) == HAL_SD_STATE_RESET) {
        if (HAL_SD_Init(&hsd) != HAL_OK) {
            diskio_log("[DISKIO] init: HAL_SD_Init failed err=0x%08lX\r\n", HAL_SD_GetError(&hsd));
            return Stat;
        }
    }

    if (!sd_wait_ready(SD_READY_WAIT_MS)) {
        diskio_log("[DISKIO] init: card not ready err=0x%08lX\r\n", HAL_SD_GetError(&hsd));
        return Stat;
    }

    if (HAL_SD_GetCardInfo(&hsd, &info) != HAL_OK) {
        diskio_log("[DISKIO] init: GetCardInfo failed err=0x%08lX\r\n", HAL_SD_GetError(&hsd));
        return Stat;
    }

    Stat = 0;
    diskio_log("[DISKIO] init: blocks=%lu blockSize=%lu\r\n",
               (unsigned long)info.LogBlockNbr,
               (unsigned long)info.LogBlockSize);
    return Stat;
}


DSTATUS SD_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd);
    Stat = (state == HAL_SD_CARD_TRANSFER) ? 0 : STA_NOINIT;

    if (Stat != 0) {
        diskio_log("[DISKIO] status: not ready, state=%d\r\n", (int)state);
    }

    return Stat;
}

DRESULT SD_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    HAL_StatusTypeDef hs;

    if (pdrv != 0 || buff == NULL || count == 0) return RES_PARERR;
    if (SD_status(pdrv) & STA_NOINIT) return RES_NOTRDY;

    if (!sd_wait_ready(SD_READY_WAIT_MS)) {
        diskio_log("[DISKIO] read: card not ready before read\r\n");
        return RES_ERROR;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    hs = HAL_SD_ReadBlocks(&hsd, buff,
                           (uint32_t)sector, (uint32_t)count, SD_TIMEOUT_MS);

    if (hs != HAL_OK) {
        diskio_log("[DISKIO] read: HAL_SD_ReadBlocks failed err=0x%08lX sta=0x%08lX hs=%d\r\n",
                   HAL_SD_GetError(&hsd), SDIO->STA, (int)hs);
        return RES_ERROR;
    }

    if (!sd_wait_ready(SD_TIMEOUT_MS)) {
        diskio_log("[DISKIO] read: ready wait failed err=0x%08lX sta=0x%08lX\r\n",
                   HAL_SD_GetError(&hsd), SDIO->STA);
        return RES_ERROR;
    }

    return RES_OK;
}



#if _USE_WRITE == 1
DRESULT SD_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    HAL_StatusTypeDef hs;

    if (pdrv != 0 || buff == NULL || count == 0) return RES_PARERR;
    if (SD_status(pdrv) & STA_NOINIT) return RES_NOTRDY;

    if (!sd_wait_ready(SD_READY_WAIT_MS)) {
        diskio_log("[DISKIO] write: card not ready before write, err=0x%08lX\r\n",
                   HAL_SD_GetError(&hsd));
        return RES_ERROR;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    hs = HAL_SD_WriteBlocks(&hsd, (uint8_t*)buff,
                            (uint32_t)sector, (uint32_t)count, SD_TIMEOUT_MS);

    if (hs != HAL_OK) {
        diskio_log("[DISKIO] write: HAL_SD_WriteBlocks failed err=0x%08lX sta=0x%08lX hs=%d\r\n",
                   HAL_SD_GetError(&hsd), SDIO->STA, (int)hs);
        return RES_ERROR;
    }

    if (!sd_wait_ready(SD_TIMEOUT_MS)) {
        diskio_log("[DISKIO] write: ready wait failed err=0x%08lX sta=0x%08lX\r\n",
                   HAL_SD_GetError(&hsd), SDIO->STA);
        return RES_ERROR;
    }

    diskio_log("[DISKIO] write: OK\r\n");
    return RES_OK;
}
#endif /* _USE_WRITE == 1 */

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    HAL_SD_CardInfoTypeDef info;
    DRESULT res = RES_ERROR;

    if (pdrv != 0) return RES_PARERR;

    switch (cmd) {
    case CTRL_SYNC:
        res = sd_wait_ready(SD_TIMEOUT_MS) ? RES_OK : RES_ERROR;
        break;

    case GET_SECTOR_COUNT:
        if (SD_status(pdrv) & STA_NOINIT) return RES_NOTRDY;
        if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
            *(DWORD*)buff = info.LogBlockNbr;
            res = RES_OK;
        }
        break;

    case GET_SECTOR_SIZE:
        if (SD_status(pdrv) & STA_NOINIT) return RES_NOTRDY;
        *(WORD*)buff = 512;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        if (SD_status(pdrv) & STA_NOINIT) return RES_NOTRDY;
        if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
            *(DWORD*)buff = 1;
            res = RES_OK;
        }
        break;

            default:
                res = RES_PARERR;
                break;
            }

            return res;
        }
        #endif /* _USE_IOCTL == 1 */  // ← ДОБАВЛЕНО
