/**
 * ================================================================
 * @file    debug_uart.cpp
 * @brief   Реализация отладочного UART1 (115200 8N1).
 * ================================================================
 */
#include "debug_uart.hpp"

/* ======== Глобальный экземпляр — привязан к huart1 ======== */
DebugUart DBG(&huart1);

/* ==================== Конструктор ==================== */
DebugUart::DebugUart(UART_HandleTypeDef* uart)
    : m_uart(uart), m_buf{}, m_enabled(true)
{
}

/* ==================== Инициализация ==================== */
void DebugUart::init()
{
    /* Небольшая задержка чтобы терминал на ПК успел открыться */
    HAL_Delay(100);

    separator();
    raw("  ___                      __  __             _ _\r\n");
    raw(" / _ \\ ___ ___  __ _ _ __ |  \\/  | ___  _ __ (_) |_ ___  _ __\r\n");
    raw("| | | / __/ _ \\/ _` | '_ \\| |\\/| |/ _ \\| '_ \\| | __/ _ \\| '__|\r\n");
    raw("| |_| | (_|  __/ (_| | | | | |  | | (_) | | | | | || (_) | |\r\n");
    raw(" \\___/ \\___\\___|\\__,_|_| |_|_|  |_|\\___/|_| |_|_|\\__\\___/|_|\r\n");
    raw("\r\n");

    info("Firmware v1.0.0 | STM32F407VET6");
    info("SYSCLK : %lu MHz", HAL_RCC_GetSysClockFreq() / 1000000UL);
    info("HCLK   : %lu MHz", HAL_RCC_GetHCLKFreq()     / 1000000UL);
    info("APB1   : %lu MHz", HAL_RCC_GetPCLK1Freq()    / 1000000UL);
    info("APB2   : %lu MHz", HAL_RCC_GetPCLK2Freq()    / 1000000UL);
    info("Debug  : UART1 115200 8N1 (PA9-TX, PA10-RX)");
    info("Build  : " __DATE__ " " __TIME__);

    separator();
}

/* ==================== Уровни ==================== */
void DebugUart::info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    send(LogLevel::Info, fmt, args);
    va_end(args);
}

void DebugUart::warn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    send(LogLevel::Warn, fmt, args);
    va_end(args);
}

void DebugUart::error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    send(LogLevel::Error, fmt, args);
    va_end(args);
}

void DebugUart::data(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    send(LogLevel::Data, fmt, args);
    va_end(args);
}

/* ==================== Сырой вывод ==================== */
void DebugUart::raw(const char* str)
{
    if (!m_enabled || str == nullptr) return;
    transmit(str, static_cast<uint16_t>(std::strlen(str)));
}

/* ==================== Разделитель ==================== */
void DebugUart::separator()
{
    raw("================================================"
        "========================\r\n");
}

/* ==================== Hex-дамп ==================== */
void DebugUart::hexDump(const char* label, const uint8_t* buf, uint16_t len)
{
    if (!m_enabled) return;

    /* Заголовок */
    int off = std::snprintf(m_buf, sizeof(m_buf),
                            "[%8lu] [DATA ] %s (%u bytes): ",
                            HAL_GetTick(), label, len);

    /* Байты в HEX */
    for (uint16_t i = 0; i < len && off < static_cast<int>(sizeof(m_buf)) - 8; i++) {
        off += std::snprintf(m_buf + off, sizeof(m_buf) - static_cast<size_t>(off),
                             "%02X ", buf[i]);
    }

    /* Завершение строки */
    m_buf[off++] = '\r';
    m_buf[off++] = '\n';
    m_buf[off]   = '\0';

    transmit(m_buf, static_cast<uint16_t>(off));
}

/* ==================== Ядро отправки ==================== */
void DebugUart::send(LogLevel lvl, const char* fmt, va_list args)
{
    if (!m_enabled) return;

    /* Заголовок: [tick] [LEVEL] */
    int hdr = std::snprintf(m_buf, sizeof(m_buf),
                            "[%8lu] [%s] ",
                            HAL_GetTick(),
                            levelStr(lvl));
    if (hdr < 0) return;

    /* Тело сообщения */
    int body = std::vsnprintf(
        m_buf + hdr,
        sizeof(m_buf) - static_cast<size_t>(hdr) - 4,
        fmt, args);
    if (body < 0) body = 0;

    int total = hdr + body;

    /* Защита от переполнения */
    if (total > static_cast<int>(sizeof(m_buf)) - 4) {
        total = static_cast<int>(sizeof(m_buf)) - 4;
    }

    /* \r\n */
    m_buf[total++] = '\r';
    m_buf[total++] = '\n';
    m_buf[total]   = '\0';

    transmit(m_buf, static_cast<uint16_t>(total));
}

/* ==================== Физическая передача ==================== */
void DebugUart::transmit(const char* data, uint16_t len)
{
    if (m_uart == nullptr || len == 0) return;

    HAL_UART_Transmit(m_uart,
                      reinterpret_cast<const uint8_t*>(data),
                      len,
                      200);   /* таймаут 200 мс */
}

/* ==================== Строка уровня ==================== */
const char* DebugUart::levelStr(LogLevel lvl)
{
    switch (lvl) {
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Data:  return "DATA ";
    default:              return "?????";
    }
}
