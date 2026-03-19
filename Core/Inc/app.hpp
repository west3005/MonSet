/**
 * ================================================================
 * @file    app.hpp
 * @brief   Главный класс приложения — координирует все модули.
 *
 * Режимы работы (определяются пином PIN_MODE_SW):
 *   Debug — LED постоянно, полинг без STOP-режима
 *   Sleep — Stop-режим между циклами (минимальное потребление)
 *
 * Канал передачи данных (определяется пином PIN_NET_SELECT):
 *   0 (LOW)  — Ethernet W5500 (HTTP/HTTPS)
 *   1 (HIGH) — NB-IoT SIM7020C (HTTP over TCP)
 * ================================================================
 */
#pragma once

#include "config.hpp"
#include "runtime_config.hpp"
#include "debug_uart.hpp"
#include "ds3231.hpp"
#include "modbus_rtu.hpp"
#include "sim7020c.hpp"
#include "sd_backup.hpp"
#include "sensor_reader.hpp"
#include "data_buffer.hpp"
#include "power_manager.hpp"

// ================================================================
// Режим работы системы
// ================================================================
enum class SystemMode : uint8_t {
    Sleep = 0,  ///< Stop-режим между опросами (штатный, полевой)
    Debug = 1,  ///< Постоянная работа, LED ON (разработка)
};

// ================================================================
// Канал передачи данных
// ================================================================
enum class LinkChannel : uint8_t {
    Eth = 0,  ///< Ethernet через W5500
    Gsm = 1,  ///< NB-IoT через SIM7020C
};

// ================================================================
// Класс приложения
// ================================================================
class App {
public:
    App();

    /** Инициализация всех подсистем. Вызывается один раз из main(). */
    void init();

    /** Главный бесконечный цикл. */
    [[noreturn]] void run();

private:
    // ---- Аппаратные модули ----
    DS3231      m_rtc;
    ModbusRTU   m_modbus;
    SIM7020C    m_gsm;
    SdBackup    m_sdBackup;
    SensorReader m_sensor;
    DataBuffer  m_buffer;
    PowerManager m_power;

    // ---- Состояние ----
    SystemMode  m_mode        = SystemMode::Sleep;
    uint32_t    m_pollCounter = 0;

    // ---- Рабочий буфер JSON (разделяется между методами) ----
    char m_json[Config::JSON_BUFFER_SIZE]{};

    // ---- LED ----
    void ledOn();
    void ledOff();
    void ledBlink(uint8_t count, uint32_t ms);

    // ---- Чтение режима и канала ----
    SystemMode  readMode();
    LinkChannel readChannel();

    // ---- NTP-синхронизация ----
    bool syncRtcWithNtpIfNeeded(const char* tag, bool verbose);

    // ---- Передача данных ----
    void transmitBuffer();
    void transmitSingle(float value, const DateTime& dt);
    void retransmitBackup();

    // ---- ETH вспомогательные ----
    bool ensureEthReady();
    int  postViaEth(const char* json, uint16_t len);
    int  postViaGsm(const char* json, uint16_t len);

    // ---- Построители JSON ----
    int buildPayload(char* buf, size_t bsz,
                     const char* tsStr, float val, const DateTime& dt,
                     bool asArray);
};
