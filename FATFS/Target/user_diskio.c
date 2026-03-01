/**
 * @file    user_diskio.c
 * @brief   FatFS diskio через HAL_SD + SDIO DMA (Stream3/Stream6).
 *          HAL_SD_ReadBlocks_DMA / HAL_SD_WriteBlocks_DMA.
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "ff_gen_drv.h"
#include "stm32f4xx_hal_sd.h"
#include "stm32f4xx_hal_uart.h"

extern SD_HandleTypeDef   hsd;
extern UART_HandleTypeDef huart1;

#define SD_TIMEOUT_MS  5000U

static volatile DSTATUS Stat = STA_NOINIT;

/* -------------------------------------------------------
 *  C-логгер (без C++ заголовков)
 * ------------------------------------------------------- */
static void diskio_log(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, (uint16_t)len, 200);
    }
}

/* ---- прототипы ---- */
DSTATUS USER_initialize(BYTE pdrv);
DSTATUS USER_status    (BYTE pdrv);
DRESULT USER_read      (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
DRESULT USER_write     (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
DRESULT USER_ioctl     (BYTE pdrv, BYTE cmd, void *buff);
#endif

Diskio_drvTypeDef USER_Driver = {
    USER_initialize,
    USER_status,
    USER_read,
#if _USE_WRITE
    USER_write,
#endif
#if _USE_IOCTL == 1
    USER_ioctl,
#endif
};

/* =============================================================
 *  sd_wait_ready — ждём TRANSFER state (карта готова к команде)
 * ============================================================= */
static uint8_t sd_wait_ready(void)
{
    uint32_t tick = HAL_GetTick();
    while (HAL_GetTick() - tick < SD_TIMEOUT_MS) {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) {
            return 1;
        }
    }
    diskio_log("[DISKIO] sd_wait_ready: TIMEOUT\r\n");
    return 0;
}

/* =============================================================
 *  USER_initialize
 * ============================================================= */
DSTATUS USER_initialize(BYTE pdrv)
{
    if (pdrv != 0) {
        diskio_log("[DISKIO] init: wrong pdrv=%u\r\n", pdrv);
        return STA_NOINIT;
    }

    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd);
    diskio_log("[DISKIO] init: cardState=%d\r\n", (int)state);

    Stat = (state == HAL_SD_CARD_TRANSFER) ? 0 : STA_NOINIT;
    diskio_log("[DISKIO] init: Stat=0x%02X\r\n", Stat);
    return Stat;
}

/* =============================================================
 *  USER_status
 * ============================================================= */
DSTATUS USER_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    Stat = (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) ? 0 : STA_NOINIT;
    return Stat;
}

/* =============================================================
 *  USER_read — DMA-чтение
 *  Буфер должен быть выровнен по 4 байтам (требование DMA).
 *  Если нет — читаем посекторно через статический aligned-буфер.
 * ============================================================= */
DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    diskio_log("[DISKIO] read: sec=%lu cnt=%u\r\n",
               (unsigned long)sector, count);

    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT)       return RES_NOTRDY;

    /* Статический выровненный буфер для DMA (512 байт, 4-byte aligned) */
    static uint32_t aligned[512 / 4];

    if ((uint32_t)buff % 4 != 0) {
        /* Невыровненный — читаем посекторно через aligned */
        for (UINT i = 0; i < count; i++) {
            if (!sd_wait_ready()) return RES_ERROR;

            if (HAL_SD_ReadBlocks_DMA(&hsd, (uint8_t*)aligned,
                                      sector + i, 1) != HAL_OK) {
                diskio_log("[DISKIO] read DMA err=0x%08lX (unaligned)\r\n",
                           HAL_SD_GetError(&hsd));
                return RES_ERROR;
            }
            /* Ждём завершения DMA — карта вернётся в TRANSFER */
            uint32_t tick = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
                if (HAL_GetTick() - tick > SD_TIMEOUT_MS) {
                    diskio_log("[DISKIO] read DMA wait timeout (unaligned)\r\n");
                    return RES_ERROR;
                }
            }
            memcpy(buff + i * 512, aligned, 512);
        }
    } else {
        /* Выровненный — читаем всё одним DMA-вызовом */
        if (!sd_wait_ready()) return RES_ERROR;

        if (HAL_SD_ReadBlocks_DMA(&hsd, buff, sector, count) != HAL_OK) {
            diskio_log("[DISKIO] read DMA err=0x%08lX\r\n",
                       HAL_SD_GetError(&hsd));
            return RES_ERROR;
        }
        uint32_t tick = HAL_GetTick();
        while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
            if (HAL_GetTick() - tick > SD_TIMEOUT_MS) {
                diskio_log("[DISKIO] read DMA wait timeout\r\n");
                return RES_ERROR;
            }
        }
    }

    diskio_log("[DISKIO] read: OK\r\n");
    return RES_OK;
}

/* =============================================================
 *  USER_write — DMA-запись
 * ============================================================= */
#if _USE_WRITE == 1
DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT)       return RES_NOTRDY;

    static uint32_t aligned[512 / 4];

    if ((uint32_t)buff % 4 != 0) {
        for (UINT i = 0; i < count; i++) {
            memcpy(aligned, buff + i * 512, 512);
            if (!sd_wait_ready()) return RES_ERROR;

            if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t*)aligned,
                                       sector + i, 1) != HAL_OK)
                return RES_ERROR;

            uint32_t tick = HAL_GetTick();
            while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
                if (HAL_GetTick() - tick > SD_TIMEOUT_MS) return RES_ERROR;
            }
        }
    } else {
        if (!sd_wait_ready()) return RES_ERROR;

        if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t*)buff,
                                   sector, count) != HAL_OK)
            return RES_ERROR;

        uint32_t tick = HAL_GetTick();
        while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
            if (HAL_GetTick() - tick > SD_TIMEOUT_MS) return RES_ERROR;
        }
    }
    return RES_OK;
}
#endif /* _USE_WRITE == 1 */

/* =============================================================
 *  USER_ioctl
 * ============================================================= */
#if _USE_IOCTL == 1
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0)         return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    HAL_SD_CardInfoTypeDef info;
    DRESULT res = RES_ERROR;

    switch (cmd) {
    case CTRL_SYNC:
        res = sd_wait_ready() ? RES_OK : RES_ERROR;
        break;
    case GET_SECTOR_COUNT:
        if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
            *(DWORD*)buff = info.LogBlockNbr;
            res = RES_OK;
        }
        break;
    case GET_SECTOR_SIZE:
        *(WORD*)buff = 512;
        res = RES_OK;
        break;
    case GET_BLOCK_SIZE:
        if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
            *(DWORD*)buff = info.LogBlockSize / 512;
            res = RES_OK;
        }
        break;
    default:
        res = RES_PARERR;
        break;
    }
    return res;
}
#endif /* _USE_IOCTL == 1 */

