#include "sd_raw_test.h"

#include <string.h>

extern "C" {
#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "main.h"
}

#include "debug_uart.hpp"

extern SD_HandleTypeDef hsd;

#if defined(__GNUC__)
#define SD_ALIGN4 __attribute__((aligned(4)))
#else
#define SD_ALIGN4
#endif

static uint8_t SD_ALIGN4 g_orig[512];
static uint8_t SD_ALIGN4 g_tx[512];
static uint8_t SD_ALIGN4 g_rx[512];

static uint32_t sd_irq_save(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void sd_irq_restore(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static int sd_wait_card_ready_ms(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) {
            return 1;
        }
        HAL_Delay(1);
    }
    return 0;
}

int sd_raw_rw_test(uint32_t test_sector)
{
    HAL_StatusTypeDef hs;
    uint32_t irq_state;
    uint32_t i;

    DBG.info("SDTEST: sector=%lu begin", (unsigned long)test_sector);

    if (!sd_wait_card_ready_ms(2000)) {
        DBG.error("SDTEST: card not ready before read, err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    irq_state = sd_irq_save();
    hs = HAL_SD_ReadBlocks(&hsd, g_orig, test_sector, 1, 5000);
    sd_irq_restore(irq_state);

    if (hs != HAL_OK) {
        DBG.error("SDTEST: read orig fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST: wait after read fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    for (i = 0; i < sizeof(g_tx); i++) {
        g_tx[i] = (uint8_t)(i ^ 0xA5U);
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    irq_state = sd_irq_save();
    hs = HAL_SD_WriteBlocks(&hsd, g_tx, test_sector, 1, 5000);
    sd_irq_restore(irq_state);

    if (hs != HAL_OK) {
        DBG.error("SDTEST: write test fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST: wait after write fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    memset(g_rx, 0, sizeof(g_rx));
    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    irq_state = sd_irq_save();
    hs = HAL_SD_ReadBlocks(&hsd, g_rx, test_sector, 1, 5000);
    sd_irq_restore(irq_state);

    if (hs != HAL_OK) {
        DBG.error("SDTEST: readback fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST: wait after readback fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    if (memcmp(g_tx, g_rx, 512) != 0) {
        for (i = 0; i < 512; i++) {
            if (g_tx[i] != g_rx[i]) {
                DBG.error("SDTEST: verify mismatch at %lu tx=%02X rx=%02X",
                          (unsigned long)i, g_tx[i], g_rx[i]);
                break;
            }
        }
        return 0;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    irq_state = sd_irq_save();
    hs = HAL_SD_WriteBlocks(&hsd, g_orig, test_sector, 1, 5000);
    sd_irq_restore(irq_state);

    if (hs != HAL_OK) {
        DBG.error("SDTEST: restore fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST: wait after restore fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    DBG.info("SDTEST: PASS");
    return 1;
}
