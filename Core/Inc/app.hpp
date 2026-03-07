/**
* ================================================================
* @file app.hpp
* @brief Главный класс приложения — собирает все модули.
* ================================================================
*/
#ifndef APP_HPP
#define APP_HPP

#include "config.hpp"
#include "runtime_config.hpp"

#include "debug_uart.hpp"
#include "ds3231.hpp"
#include "modbus_rtu.hpp"
#include "sim800l.hpp"
#include "sd_backup.hpp"
#include "sensor_reader.hpp"
#include "data_buffer.hpp"
#include "power_manager.hpp"

enum class SystemMode : uint8_t {
  Sleep = 0,
  Debug = 1
};

class App {
public:
  App();

  void init();
  [[noreturn]] void run();

private:
  DS3231 m_rtc;
  ModbusRTU m_modbus;
  SIM800L m_gsm;
  SdBackup m_sdBackup;
  SensorReader m_sensor;
  DataBuffer m_buffer;
  PowerManager m_power;

  SystemMode m_mode = SystemMode::Sleep;

  char m_json[Config::JSON_BUFFER_SIZE]{};

  uint32_t m_pollCounter = 0;

  SystemMode readMode();
  void ledOn();
  void ledOff();
  void ledBlink(uint8_t count, uint32_t ms);

  void transmitBuffer();
  void transmitSingle(float value, const DateTime& dt);
  void retransmitBackup();

  // NTP sync (использует Cfg().ntp_* параметры)
  bool syncRtcWithNtpIfNeeded(const char* tag, bool verbose);
};

#endif /* APP_HPP */
