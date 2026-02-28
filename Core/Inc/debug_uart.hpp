/**
 * ================================================================
 * @file    debug_uart.hpp
 * @brief   Отладочный вывод через USART1 (PA9-TX, PA10-RX).
 *          115200 бод, 8N1, без аппаратного контроля потока.
 *
 *          Формат строки:
 *          [tick_ms] [LEVEL] сообщение\r\n
 *
 *          Уровни: INFO, WARN, ERROR, DATA
 *
 * @usage   DBG.info("Hello %d", 42);
 *          DBG.error("Fail code=%d", rc);
 *          DBG.data("val=%.3f", value);
 * ================================================================
 */
#ifndef DEBUG_UART_HPP
#define DEBUG_UART_HPP

#include "main.h"
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

/* -------- Уровни логирования -------- */
enum class LogLevel : uint8_t {
    Info  = 0,
    Warn  = 1,
    Error = 2,
    Data  = 3
};

/* -------- Класс -------- */
class DebugUart {
public:
    /**
     * @brief  Конструктор
     * @param  uart — указатель на HAL UART handle (huart1)
     */
    explicit DebugUart(UART_HandleTypeDef* uart);

    /** @brief  Вывести баннер и системную информацию */
    void init();

    /** @brief  Информационное сообщение */
    void info(const char* fmt, ...);

    /** @brief  Предупреждение */
    void warn(const char* fmt, ...);

    /** @brief  Ошибка */
    void error(const char* fmt, ...);

    /** @brief  Данные измерений */
    void data(const char* fmt, ...);

    /** @brief  Сырой вывод строки (без заголовка и \r\n) */
    void raw(const char* str);

    /** @brief  Разделительная линия */
    void separator();

    /** @brief  Hex-дамп массива байт */
    void hexDump(const char* label, const uint8_t* buf, uint16_t len);

    /** @brief  Включить/выключить вывод */
    void setEnabled(bool en) { m_enabled = en; }
    bool isEnabled() const   { return m_enabled; }

private:
    UART_HandleTypeDef* m_uart;
    char                m_buf[300];
    bool                m_enabled;

    /** @brief  Ядро: формирование строки и отправка */
    void send(LogLevel lvl, const char* fmt, va_list args);

    /** @brief  Физическая передача по UART (блокирующая) */
    void transmit(const char* data, uint16_t len);

    /** @brief  Текстовое имя уровня (5 символов) */
    static const char* levelStr(LogLevel lvl);
};

/* ======== Глобальный экземпляр (определяется в debug_uart.cpp) ======== */
extern DebugUart DBG;

#endif /* DEBUG_UART_HPP */
