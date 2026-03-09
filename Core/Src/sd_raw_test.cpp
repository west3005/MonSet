#include "sd_raw_test.h"

#include <string.h>

#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "main.h"
#include "debug_uart.hpp"

extern SD_HandleTypeDef hsd;

static int sd_wait_card_ready_ms(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) {
            return 1;
        }
        IWDG->KR = 0xAAAA;
    }
    return 0;
}

int sd_raw_rw_test(uint32_t test_sector)
{
    HAL_StatusTypeDef hs;
    uint8_t tx[512];
    uint8_t rx[512];
    uint32_t i;

    DBG.info("SDTEST-IT: sector=%lu begin", (unsigned long)test_sector);

    for (i = 0; i < sizeof(tx); i++) {
        tx[i] = (uint8_t)(i ^ 0x5AU);
    }

    if (!sd_wait_card_ready_ms(2000)) {
        DBG.error("SDTEST-IT: card not ready before write err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    hs = HAL_SD_WriteBlocks_IT(&hsd, tx, test_sector, 1);
    if (hs != HAL_OK) {
        DBG.error("SDTEST-IT: start write fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST-IT: wait after write fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    memset(rx, 0, sizeof(rx));
    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

    hs = HAL_SD_ReadBlocks(&hsd, rx, test_sector, 1, 5000);
    if (hs != HAL_OK) {
        DBG.error("SDTEST-IT: readback fail err=0x%08lX hs=%d",
                  HAL_SD_GetError(&hsd), (int)hs);
        return 0;
    }

    if (!sd_wait_card_ready_ms(5000)) {
        DBG.error("SDTEST-IT: wait after readback fail err=0x%08lX",
                  HAL_SD_GetError(&hsd));
        return 0;
    }

    if (memcmp(tx, rx, 512) != 0) {
        for (i = 0; i < 512; i++) {
            if (tx[i] != rx[i]) {
                DBG.error("SDTEST-IT: verify mismatch at %lu tx=%02X rx=%02X",
                          (unsigned long)i, tx[i], rx[i]);
                return 0;
            }
        }
    }

    DBG.info("SDTEST-IT: PASS");
    return 1;
}
