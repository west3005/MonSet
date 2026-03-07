#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "stm32f4xx_hal.h"
#include "board_pins.hpp"

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#else
#include <stdint.h>
#include <stddef.h>
#endif

// ============================================================================
// GPIO aliases for backward compatibility
// Source of truth: board_pins.hpp
// IMPORTANT: do NOT redefine PIN_xxx to itself (it breaks compilation).
// ============================================================================

// Старое имя LED (в старом коде было PIN_LED_*)
#ifndef PIN_LED_PORT
#define PIN_LED_PORT PIN_STATUS_LED_PORT
#endif
#ifndef PIN_LED_PIN
#define PIN_LED_PIN PIN_STATUS_LED_PIN
#endif

// Исторический алиас "SIM power"
#ifndef PIN_SIM_PWR_PORT
#define PIN_SIM_PWR_PORT PIN_CELL_PWR_EN_PORT
#endif
#ifndef PIN_SIM_PWR_PIN
#define PIN_SIM_PWR_PIN PIN_CELL_PWR_EN_PIN
#endif

// Селектор сети (ETH/GSM)
#ifndef PIN_NET_SW_PORT
#define PIN_NET_SW_PORT PIN_NET_SELECT_PORT
#endif
#ifndef PIN_NET_SW_PIN
#define PIN_NET_SW_PIN PIN_NET_SELECT_PIN
#endif

namespace Config {

// ================================================================
// Network defaults (W5500)
// ================================================================
constexpr uint8_t W5500_MAC[6] = {0x02, 0x30, 0x05, 0x00, 0x00, 0x01};

enum class NetMode : uint8_t { DHCP = 0, Static = 1 };
constexpr NetMode NET_MODE = NetMode::Static;

constexpr uint8_t NET_IP[4]  = {192, 168, 31, 122};
constexpr uint8_t NET_SN[4]  = {255, 255, 255, 0};
constexpr uint8_t NET_GW[4]  = {192, 168, 31, 1};
constexpr uint8_t NET_DNS[4] = {192, 168, 31, 1};

constexpr uint32_t W5500_DHCP_TIMEOUT_MS = 8000;

// --- новые сетевые таймауты ---
constexpr uint32_t DNS_TIMEOUT_MS        = 5000;
constexpr uint16_t DNS_BUFFER_SIZE       = 512;

constexpr uint32_t HTTP_POST_TIMEOUT_MS  = 15000;
constexpr uint32_t HTTPS_POST_TIMEOUT_MS = 20000;

constexpr uint16_t HTTP_LOCAL_PORT       = 50000;

// ================================================================
// Peripherals / addresses
// ================================================================
constexpr uint8_t DS3231_ADDR = 0x68 << 1;

// ================================================================
// Modbus defaults
// ================================================================
constexpr uint8_t  MODBUS_SLAVE      = 1;
constexpr uint8_t  MODBUS_FUNC_CODE  = 4;
constexpr uint16_t MODBUS_START_REG  = 0;
constexpr uint16_t MODBUS_NUM_REGS   = 2;

// ================================================================
// Sensor scaling defaults
// ================================================================
constexpr float SENSOR_ZERO_LEVEL = 0.0f;
constexpr float SENSOR_DIVIDER    = 1000.0f;

// ================================================================
// Telemetry IDs defaults
// ================================================================
constexpr const char* METRIC_ID  = "f2656f53-463c-4d66-8ab1-e86fb11549b1";
constexpr const char* COMPLEX_ID = "21100e69-b08b-45d1-ab1f-0adca0f0f909";

// ================================================================
// GSM defaults
// ================================================================
constexpr const char* GSM_APN      = "internet";
constexpr const char* GSM_APN_USER = "";
constexpr const char* GSM_APN_PASS = "";

// ================================================================
// Server defaults
// ================================================================
constexpr const char* SERVER_URL  =
    "https://thingsboard.cloud/api/v1/6Wv356bm51LxD2vrF22S/telemetry";
constexpr const char* SERVER_AUTH = "";

// ================================================================
// Timings
// ================================================================
constexpr uint32_t POLL_INTERVAL_SEC   = 5;
constexpr uint32_t SEND_INTERVAL_POLLS = 2;

constexpr uint32_t MODBUS_TIMEOUT_MS     = 1000;
constexpr uint32_t SIM800_CMD_TIMEOUT_MS = 5000;
constexpr uint32_t SIM800_HTTP_TIMEOUT   = 30000;
constexpr uint32_t SIM800_BOOT_MS        = 4000;

// ================================================================
// Buffers / sizes
// ================================================================
constexpr uint8_t  MEAS_BUFFER_SIZE = 64;
constexpr uint16_t JSON_BUFFER_SIZE = 8192;
constexpr uint16_t GSM_RX_BUF_SIZE  = 512;

// ================================================================
// SD backup / files
// ================================================================
constexpr const char* BACKUP_FILENAME = "backup.jsn";
constexpr uint16_t JSONL_LINE_MAX     = 240;
constexpr uint16_t HTTP_CHUNK_MAX     = 1800;

} // namespace Config

#endif /* CONFIG_HPP */
