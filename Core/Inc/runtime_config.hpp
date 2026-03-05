#pragma once
#include "config.hpp"
#include "stm32f4xx_hal.h"
#include <cstdint>
#include <cstddef>

struct RuntimeConfig
{
  static constexpr uint32_t VERSION = 1;

  // --- telemetry ids ---
  bool complex_enabled = false;
  char metric_id[64]{};
  char complex_id[64]{};

  // --- server ---
  char server_url[192]{};
  // Если используешь Basic: сюда кладём именно base64("user:pass") без "Basic "
  char server_auth_b64[128]{};

  // --- ETH (W5500) ---
  enum class EthMode : uint8_t { Static = 0, Dhcp = 1 };
  EthMode eth_mode = EthMode::Static;

  uint8_t w5500_mac[6]{};
  uint8_t eth_ip[4]{};
  uint8_t eth_sn[4]{};
  uint8_t eth_gw[4]{};
  uint8_t eth_dns[4]{};

  // --- GSM ---
  char gsm_apn[32]{};
  char gsm_user[32]{};
  char gsm_pass[32]{};

  // --- timing ---
  uint32_t poll_interval_sec = 5;     // опрос датчика
  uint32_t send_interval_polls = 2;   // отправка раз в N опросов (>=1)

  // --- Modbus ---
  uint8_t  modbus_slave = 1;
  uint8_t  modbus_func = 4;
  uint16_t modbus_start_reg = 0;
  uint16_t modbus_num_regs = 2;

  // --- sensor scale ---
  float sensor_zero_level = 0.0f;
  float sensor_divider = 1000.0f;

  // --- time / NTP ---
  bool ntp_enabled = true;
  char ntp_host[64]{};
  uint32_t ntp_resync_sec = 86400; // раз в сутки

  void setDefaultsFromConfig();
  bool validateAndFix();

  bool loadFromSd(const char* filename);
  bool saveToSd(const char* filename) const;

  bool loadFromJson(const char* json, size_t len);
  void log() const;
};

RuntimeConfig& Cfg();

// имя файла на SD
constexpr const char* RUNTIME_CONFIG_FILENAME = "runtime_config.json";
