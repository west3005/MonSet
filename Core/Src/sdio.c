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
  hsd.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide             = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv            = 118;

  if (HAL_SD_Init(&hsd) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B) != HAL_OK)
  {
    Error_Handler();
  }
}
