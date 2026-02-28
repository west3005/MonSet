/**
 * ================================================================
 * @file    power_manager.cpp
 * @brief   Реализация Stop Mode.
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

    /* 8. Возобновить SysTick */
    HAL_ResumeTick();

    /* 9. Отключить WakeUp Timer (чтобы не дёргал дальше) */
    HAL_RTCEx_DeactivateWakeUpTimer(m_hrtc);

    /* 10. Реинициализация SD после сна */
    m_backup.init();

    wakeupFlag = false;
}
