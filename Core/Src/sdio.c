/**
 * @file sdio.c
 * @brief SDIO для SD-карты через HAL_SD (без DMA, без IRQ).
 */

#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "main.h"

SD_HandleTypeDef hsd;

void MX_SDIO_SD_Init(void)
{
    hsd.Instance = SDIO;
    hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = 118;   // ~400 kHz для надежной инициализации/отладки

    if (HAL_SD_Init(&hsd) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd_ptr)
{
    GPIO_InitTypeDef g = {0};

    if (hsd_ptr->Instance == SDIO) {
        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        g.Mode = GPIO_MODE_AF_PP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF12_SDIO;

        /* D0-D3: PC8, PC9, PC10, PC11 */
        g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOC, &g);

        /* CLK: PC12 */
        g.Pin = GPIO_PIN_12;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &g);

        /* CMD: PD2 */
        g.Pin = GPIO_PIN_2;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOD, &g);

        /* IRQ не включаем: polling mode */
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd_ptr)
{
    if (hsd_ptr->Instance == SDIO) {
        __HAL_RCC_SDIO_CLK_DISABLE();

        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                               GPIO_PIN_11 | GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

        /* IRQ не выключаем: не включали */
    }
}
