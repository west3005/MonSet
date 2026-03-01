/**
 * ================================================================
 * @file config.hpp
 * @brief Все настраиваемые параметры проекта.
 * Используем constexpr вместо #define где возможно —
 * это безопаснее и удобнее при отладке.
 * ================================================================
 */
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#endif

namespace Config {

/* ================================================================
 * Пины (оставляем #define — нужны для HAL макросов)
 * ================================================================ */

/* Светодиод статуса (DO) — мигает при чтении, горит в DEBUG */
#define PIN_LED_PORT GPIOC
#define PIN_LED_PIN  GPIO_PIN_1

/* Переключатель режимов (DI с подтяжкой к VCC)
 * Замкнут на GND → SLEEP MODE
 * Разомкнут → DEBUG MODE */
#define PIN_MODE_SW_PORT GPIOC
#define PIN_MODE_SW_PIN  GPIO_PIN_0

/* RS-485 трансивер DE/RE (объединены) */
#define PIN_RS485_DE_PORT GPIOB
#define PIN_RS485_DE_PIN  GPIO_PIN_12

/* SIM800L питание через N-MOSFET */
#define PIN_SIM_PWR_PORT GPIOC
#define PIN_SIM_PWR_PIN  GPIO_PIN_2

/* SD Card Chip Select (если используешь SPI-режим SD) */
#define PIN_SD_CS_PORT GPIOA
#define PIN_SD_CS_PIN  GPIO_PIN_4

// --- W5500 pins ---
#define PIN_W5500_CS_PORT  GPIOA
#define PIN_W5500_CS_PIN   GPIO_PIN_8
#define PIN_W5500_RST_PORT GPIOC
#define PIN_W5500_RST_PIN  GPIO_PIN_3
#define PIN_W5500_INT_PORT GPIOC
#define PIN_W5500_INT_PIN  GPIO_PIN_4

// Выбор канала связи (DI с подтяжкой к VCC)
// Замкнут на GND -> ETH (W5500)
// Разомкнут -> GSM (SIM800L)
#define PIN_NET_SW_PORT GPIOB
#define PIN_NET_SW_PIN  GPIO_PIN_0

/* ================================================================
 * W5500 сеть
 * ================================================================ */
constexpr uint8_t W5500_MAC[6] = {0x02,0x30,0x05,0x00,0x00,0x01};

enum class NetMode : uint8_t { DHCP = 0, Static = 1 };
constexpr NetMode NET_MODE = NetMode::DHCP;

// Static IPv4 (используются только если NET_MODE==Static)
constexpr uint8_t NET_IP[4]  = {192,168,31,122};
constexpr uint8_t NET_SN[4]  = {255,255,255,0};
constexpr uint8_t NET_GW[4]  = {192,168,31,1};
constexpr uint8_t NET_DNS[4] = {8,8,8,8};

constexpr uint32_t W5500_DHCP_TIMEOUT_MS = 8000;

/* ================================================================
 * DS3231 RTC
 * ================================================================ */
constexpr uint8_t DS3231_ADDR = 0x68 << 1; // Сдвинут для HAL

/* ================================================================
 * Modbus датчик (RS-485)
 * ================================================================ */
constexpr uint8_t  MODBUS_SLAVE      = 1;
constexpr uint8_t  MODBUS_FUNC_CODE  = 4;    // FC04 Input Registers
constexpr uint16_t MODBUS_START_REG  = 0;
constexpr uint16_t MODBUS_NUM_REGS   = 2;

constexpr float SENSOR_ZERO_LEVEL = 0.0f;    // Нулевая отметка (м)
constexpr float SENSOR_DIVIDER    = 1000.0f; // Делитель

/* ================================================================
 * UUID-ы (для JSON)
 * ================================================================ */
constexpr const char* METRIC_ID  =
  "f2656f53-463c-4d66-8ab1-e86fb11549b1";
constexpr const char* COMPLEX_ID =
  "21100e69-b08b-45d1-ab1f-0adca0f0f909";

/* ================================================================
 * GSM / SIM800L
 * ================================================================ */
constexpr const char* GSM_APN      = "internet";
constexpr const char* GSM_APN_USER = "";
constexpr const char* GSM_APN_PASS = "";

constexpr const char* SERVER_URL =
  "https://thingsboard.cloud/api/v1/6Wv356bm51LxD2vrF22S/telemetry";

// Если нужен Basic Auth для httpPostPlainW5500(): сюда Base64("user:pass")
constexpr const char* SERVER_AUTH = "";

/* ================================================================
 * Тайминги
 * ================================================================ */
constexpr uint32_t POLL_INTERVAL_SEC      = 5;   // Опрос датчика
constexpr uint32_t SEND_INTERVAL_POLLS    = 2;   // 60×60с = 1 час (если используешь)
constexpr uint32_t MODBUS_TIMEOUT_MS      = 1000;
constexpr uint32_t SIM800_CMD_TIMEOUT_MS  = 5000;
constexpr uint32_t SIM800_HTTP_TIMEOUT    = 30000;
constexpr uint32_t SIM800_BOOT_MS         = 4000;

/* ================================================================
 * Буферы
 * ================================================================ */
constexpr uint8_t  MEAS_BUFFER_SIZE = 64;
constexpr uint16_t JSON_BUFFER_SIZE = 8192;
constexpr uint16_t GSM_RX_BUF_SIZE  = 512;

/* ================================================================
 * SD-карта
 * ================================================================ */
constexpr const char* BACKUP_FILENAME = "backup.json";
// максимум длины одной JSONL-строки (без \r\n)
constexpr uint16_t JSONL_LINE_MAX = 240;
// максимум байт полезной нагрузки в одном HTTP POST (<= JSON_BUFFER_SIZE)
constexpr uint16_t HTTP_CHUNK_MAX = 1800;

} // namespace Config

#endif /* CONFIG_HPP */
