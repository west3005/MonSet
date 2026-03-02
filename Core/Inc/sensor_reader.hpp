#ifndef SENSOR_READER_HPP
#define SENSOR_READER_HPP

#include "modbus_rtu.hpp"
#include "ds3231.hpp"
#include "runtime_config.hpp"

class SensorReader {
public:
  SensorReader(ModbusRTU& modbus, DS3231& rtc);

  float read(DateTime& timestamp);

  float lastValue() const { return m_lastValue; }

private:
  ModbusRTU& m_modbus;
  DS3231& m_rtc;
  float m_lastValue = -9999.0f;

  static float convert(uint16_t reg0, uint16_t reg1);
};

#endif /* SENSOR_READER_HPP */
