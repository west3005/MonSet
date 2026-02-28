/**
 * @file    sdio.c
 * @brief   SDIO РґР»СЏ SD-РєР°СЂС‚С‹ С‡РµСЂРµР· HAL_SD (РЅРµ HAL_MMC).
 */

#include "sdio.h"
#include "stm32f4xx_hal_sd.h"

SD_HandleTypeDef  hsd;
DMA_HandleTypeDef hdma_sdio_rx;
DMA_HandleTypeDef hdma_sdio_tx;

void MX_SDIO_SD_Init(void)
{
    hsd.Instance                 = SDIO;
    hsd.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide             = SDIO_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv            = 2;

    if (HAL_SD_Init(&hsd) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK) {
        /* РЅРµ С„Р°С‚Р°Р»СЊРЅРѕ вЂ” РїСЂРѕРґРѕР»Р¶РёРј РЅР° 1-bit */
    }
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd_ptr)
{
    GPIO_InitTypeDef g = {0};

    if (hsd_ptr->Instance == SDIO) {

        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* D0-D3 (PC8-PC11): PULLUP РѕР±СЏР·Р°С‚РµР»РµРЅ */
        g.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_PULLUP;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOC, &g);

        /* CLK (PC12): Р±РµР· РїРѕРґС‚СЏР¶РєРё */
        g.Pin  = GPIO_PIN_12;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &g);

        /* CMD (PD2): PULLUP РѕР±СЏР·Р°С‚РµР»РµРЅ */
        g.Pin  = GPIO_PIN_2;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOD, &g);

        /* DMA RX вЂ” Stream3 */
        hdma_sdio_rx.Instance                 = DMA2_Stream3;
        hdma_sdio_rx.Init.Channel             = DMA_CHANNEL_4;
        hdma_sdio_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_sdio_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_sdio_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_sdio_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        hdma_sdio_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        hdma_sdio_rx.Init.Mode                = DMA_PFCTRL;
        hdma_sdio_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        hdma_sdio_rx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        hdma_sdio_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_sdio_rx.Init.MemBurst            = DMA_MBURST_INC4;
        hdma_sdio_rx.Init.PeriphBurst         = DMA_PBURST_INC4;
        if (HAL_DMA_Init(&hdma_sdio_rx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(hsd_ptr, hdmarx, hdma_sdio_rx);

        /* DMA TX вЂ” Stream6 */
        hdma_sdio_tx.Instance                 = DMA2_Stream6;
        hdma_sdio_tx.Init.Channel             = DMA_CHANNEL_4;
        hdma_sdio_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_sdio_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_sdio_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_sdio_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        hdma_sdio_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        hdma_sdio_tx.Init.Mode                = DMA_PFCTRL;
        hdma_sdio_tx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
        hdma_sdio_tx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        hdma_sdio_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_sdio_tx.Init.MemBurst            = DMA_MBURST_INC4;
        hdma_sdio_tx.Init.PeriphBurst         = DMA_PBURST_INC4;
        if (HAL_DMA_Init(&hdma_sdio_tx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(hsd_ptr, hdmatx, hdma_sdio_tx);

        /* NVIC */
        HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
        HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
        HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(SDIO_IRQn);
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd_ptr)
{
    if (hsd_ptr->Instance == SDIO) {
        __HAL_RCC_SDIO_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
        HAL_DMA_DeInit(hsd_ptr->hdmarx);
        HAL_DMA_DeInit(hsd_ptr->hdmatx);
        HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
        HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);
        HAL_NVIC_DisableIRQ(SDIO_IRQn);
    }
}
