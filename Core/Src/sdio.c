/**
 * @file sdio.c
 * @brief SDIO для SD-карты через HAL_SD (без DMA, PIO).
 */
#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "main.h"

SD_HandleTypeDef hsd;

void MX_SDIO_SD_Init(void) {
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;

  /* ИСПРАВЛЕНО: Включен аппаратный контроль потока для защиты от опустошения FIFO */
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;

  hsd.Init.ClockDiv = 2; // как было
  uint32_t t0 = HAL_GetTick();
  HAL_StatusTypeDef st;

  // Пытаемся инициализировать до 5 секунд
  do {
    st = HAL_SD_Init(&hsd);
    if (st == HAL_OK) break;
    HAL_Delay(50);
  } while ((HAL_GetTick() - t0) < 5000);

  if (st != HAL_OK) {
    // Карта так и не ответила — продолжаем без SD
    return;
  }
  // Остаёмся в 1-битной шине
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd_ptr) {
  GPIO_InitTypeDef g = {0};
  if (hsd_ptr->Instance == SDIO) {
    __HAL_RCC_SDIO_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* D0-D3 (PC8-PC11): PULLUP обязателен */
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOC, &g);

    /* CLK (PC12): без подтяжки */
    g.Pin = GPIO_PIN_12;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    /* CMD (PD2): PULLUP обязателен */
    g.Pin = GPIO_PIN_2;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &g);

    /* SDIO interrupt Init (опционально) */
    HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
  }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd_ptr) {
  if (hsd_ptr->Instance == SDIO) {
    __HAL_RCC_SDIO_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    HAL_NVIC_DisableIRQ(SDIO_IRQn);
  }
}
