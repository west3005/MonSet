/**
 * ================================================================
 * @file power_manager.cpp
 * @brief Реализация Stop Mode.
 * ================================================================
 */

#include "power_manager.hpp"
#include "debug_uart.hpp"
#include "main.h"

/* ========= Конструктор ========= */

PowerManager::PowerManager(RTC_HandleTypeDef* hrtc, SdBackup& backup)
: m_hrtc(hrtc), m_backup(backup)
{
}

/* ========= Проверка тактирования после STOP ========= */

bool PowerManager::checkClocksAfterStop() const
{
  // Проверяем, что HSE и PLL действительно включены и готовы.
  // Если нет — логируем и возвращаем false.
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET) {
    DBG.error("CLK: HSE not ready after STOP");
    return false;
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET) {
    DBG.error("CLK: PLL not ready after STOP");
    return false;
  }

  // Дополнительно можно проверить источник SYSCLK,
  // но здесь считаем, что SystemClock_Config() его уже установил.
  return true;
}

/* ========= Вход в Stop Mode ========= */

void PowerManager::enterStopMode(uint32_t sec)
{
  /* 1. Деинит SD (чтобы не оставлять карту в подвисшем состоянии) */
  m_backup.deinit();

  /* 2. RTC Wakeup Timer: сначала выключаем, затем настраиваем */
  HAL_RTCEx_DeactivateWakeUpTimer(m_hrtc);

  if (HAL_RTCEx_SetWakeUpTimer_IT(
          m_hrtc,
          (sec > 0) ? (sec - 1U) : 0U,
          RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
    DBG.error("RTC Wakeup: ошибка!");
    Error_Handler();
  }

  /* 3. Пауза для UART — дождаться вывода логов */
  HAL_Delay(50);

  /* 4. Останавливаем SysTick, чтобы не будил MCU в STOP */
  HAL_SuspendTick();

  /* 5. Очищаем флаг пробуждения */
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

  /* 6. Входим в STOP MODE (низкопотребляющий регулятор, WFI) */
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  /* ★ Здесь МК спит ★ */

  /* 7. После пробуждения — восстановить тактирование */
  SystemClock_Config();

  /* 7.1. Проверка, что часы реально поднялись */
  if (!checkClocksAfterStop()) {
    // Здесь два варианта: либо жёсткий Error_Handler(),
    // либо мягкий fallback на HSI с логом.
    DBG.error("CLK: fallback to HSI after STOP");

    RCC_ClkInitTypeDef clkinit{};
    uint32_t           flashLatency = 0;

    // Минимальная конфигурация: SYSCLK от HSI, без PLL
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);

    // Обновим структуру и частоты — простейший вариант:
    clkinit.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                             RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clkinit.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clkinit.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clkinit.APB1CLKDivider = RCC_HCLK_DIV1;
    clkinit.APB2CLKDivider = RCC_HCLK_DIV1;
    flashLatency           = FLASH_LATENCY_0;

    if (HAL_RCC_ClockConfig(&clkinit, flashLatency) != HAL_OK) {
      DBG.error("CLK: HAL_RCC_ClockConfig(HISIfallback) failed");
      Error_Handler();
    }
  }

  /* 8. Возобновить SysTick */
  HAL_ResumeTick();

  /* 9. Отключить WakeUp Timer (чтобы не дёргал дальше) */
  HAL_RTCEx_DeactivateWakeUpTimer(m_hrtc);

  /* 10. Реинициализация SD после сна */
  m_backup.init();

  wakeupFlag = false;
}
