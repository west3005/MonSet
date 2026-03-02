#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "stm32f4xx_hal.h"
#include "board_pins.hpp"

#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#endif

namespace Config {

// ================================================================
// Пины (HAL style). Держим совместимость со старым кодом.
// Источник правды: board_pins.hpp
// ================================================================

// Старые имена (используются в существующем коде)
#define PIN_LED_PORT            PIN_STATUS_LED_PORT
#define PIN_LED_PIN             PIN_STATUS_LED_PIN

#define PIN_MODE_SW_PORT        PIN_MODE_SW_PORT
#define PIN_MODE_SW_PIN         PIN_MODE_SW_PIN

#define PIN_RS485_DE_PORT       PIN_RS485_DE_PORT
#define PIN_RS485_DE_PIN        PIN_RS485_DE_PIN

// Исторически называлось SIM800_PWR — оставляем алиас под “cellular power enable”
#define PIN_SIM_PWR_PORT        PIN_CELL_PWR_EN_PORT
#define PIN_SIM_PWR_PIN         PIN_CELL_PWR_EN_PIN

#define PIN_W5500_CS_PORT        PIN_W5500_CS_PORT
#define PIN_W5500_CS_PIN         PIN_W5500_CS_PIN
#define PIN_W5500_RST_PORT       PIN_W5500_RST_PORT
#define PIN_W5500_RST_PIN        PIN_W5500_RST_PIN
#define PIN_W5500_INT_PORT       PIN_W5500_INT_PORT
#define PIN_W5500_INT_PIN        PIN_W5500_INT_PIN

#define PIN_NET_SW_PORT          PIN_NET_SELECT_PORT
#define PIN_NET_SW_PIN           PIN_NET_SELECT_PIN

// Новые имена (можешь использовать в новом коде)
#define PIN_CELL_PWR_EN_PORT     PIN_CELL_PWR_EN_PORT
#define PIN_CELL_PWR_EN_PIN      PIN_CELL_PWR_EN_PIN
#define PIN_CELL_PWRKEY_PORT     PIN_CELL_PWRKEY_PORT
#define PIN_CELL_PWRKEY_PIN      PIN_CELL_PWRKEY_PIN
#define PIN_CELL_RESET_PORT      PIN_CELL_RESET_PORT
#define PIN_CELL_RESET_PIN       PIN_CELL_RESET_PIN
#define PIN_CELL_RI_WAKE_PORT    PIN_CELL_RI_WAKE_PORT
#define PIN_CELL_RI_WAKE_PIN     PIN_CELL_RI_WAKE_PIN
#define PIN_CELL_STATUS_PORT     PIN_CELL_STATUS_PORT
#define PIN_CELL_STATUS_PIN      PIN_CELL_STATUS_PIN
#define PIN_CELL_DTR_PORT        PIN_CELL_DTR_PORT
#define PIN_CELL_DTR_PIN         PIN_CELL_DTR_PIN

#define PIN_EDGE_ON_PORT         PIN_EDGE_ON_PORT
#define PIN_EDGE_ON_PIN          PIN_EDGE_ON_PIN
#define PIN_EDGE_NET_AVAIL_PORT  PIN_EDGE_NET_AVAIL_PORT
#define PIN_EDGE_NET_AVAIL_PIN   PIN_EDGE_NET_AVAIL_PIN
#define PIN_EDGE_PWR_DET_PORT    PIN_EDGE_PWR_DET_PORT
#define PIN_EDGE_PWR_DET_PIN     PIN_EDGE_PWR_DET_PIN

#define PIN_ESP_EN_PORT          PIN_ESP_EN_PORT
#define PIN_ESP_EN_PIN           PIN_ESP_EN_PIN
#define PIN_ESP_RST_PORT         PIN_ESP_RST_PORT
#define PIN_ESP_RST_PIN          PIN_ESP_RST_PIN
#define PIN_ESP_GPIO0_PORT       PIN_ESP_GPIO0_PORT
#define PIN_ESP_GPIO0_PIN        PIN_ESP_GPIO0_PIN

// ================================================================
// Дальше у тебя всё как было: MAC, NET, тайминги, буферы, URL, etc.
// ================================================================

constexpr uint8_t W5500_MAC[6] = {0x02,0x30,0x05,0x00,0x00,0x01};

enum class NetMode : uint8_t { DHCP = 0, Static = 1 };
constexpr NetMode NET_MODE = NetMode::Static;

constexpr uint8_t NET_IP[4]  = {192,168,31,122};
constexpr uint8_t NET_SN[4]  = {255,255,255,0};
constexpr uint8_t NET_GW[4]  = {192,168,31,1};
constexpr uint8_t NET_DNS[4] = {192,168,31,1};

constexpr uint32_t W5500_DHCP_TIMEOUT_MS = 8000;

constexpr uint8_t DS3231_ADDR = 0x68 << 1;

constexpr uint8_t  MODBUS_SLAVE      = 1;
constexpr uint8_t  MODBUS_FUNC_CODE  = 4;
constexpr uint16_t MODBUS_START_REG  = 0;
constexpr uint16_t MODBUS_NUM_REGS   = 2;

constexpr float SENSOR_ZERO_LEVEL = 0.0f;
constexpr float SENSOR_DIVIDER    = 1000.0f;

constexpr const char* METRIC_ID  = "f2656f53-463c-4d66-8ab1-e86fb11549b1";
constexpr const char* COMPLEX_ID = "21100e69-b08b-45d1-ab1f-0adca0f0f909";

constexpr const char* GSM_APN      = "internet";
constexpr const char* GSM_APN_USER = "";
constexpr const char* GSM_APN_PASS = "";

constexpr const char* SERVER_URL  = "https://thingsboard.cloud/api/v1/6Wv356bm51LxD2vrF22S/telemetry";
constexpr const char* SERVER_AUTH = "";

constexpr uint32_t POLL_INTERVAL_SEC      = 5;
constexpr uint32_t SEND_INTERVAL_POLLS    = 2;
constexpr uint32_t MODBUS_TIMEOUT_MS      = 1000;
constexpr uint32_t SIM800_CMD_TIMEOUT_MS  = 5000;
constexpr uint32_t SIM800_HTTP_TIMEOUT    = 30000;
constexpr uint32_t SIM800_BOOT_MS         = 4000;

constexpr uint8_t  MEAS_BUFFER_SIZE = 64;
constexpr uint16_t JSON_BUFFER_SIZE = 8192;
constexpr uint16_t GSM_RX_BUF_SIZE  = 512;

constexpr const char* BACKUP_FILENAME = "backup.jsn";
constexpr uint16_t JSONL_LINE_MAX = 240;
constexpr uint16_t HTTP_CHUNK_MAX = 1800;

} // namespace Config

#endif /* CONFIG_HPP */
