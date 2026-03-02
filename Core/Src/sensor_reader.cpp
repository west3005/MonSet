#include "sensor_reader.hpp"
#include "debug_uart.hpp"

SensorReader::SensorReader(ModbusRTU& modbus, DS3231& rtc)
: m_modbus(modbus), m_rtc(rtc)
{
}

float SensorReader::convert(uint16_t reg0, uint16_t reg1)
{
  const RuntimeConfig& c = Cfg();
  float raw = static_cast<float>(static_cast<uint32_t>(reg0) * 65536UL + static_cast<uint32_t>(reg1)) / 10.0f;
  return c.sensor_zero_level - raw / c.sensor_divider;
}

float SensorReader::read(DateTime& timestamp)
{
  m_lastValue = -9999.0f;
  uint16_t regs[2] = {0, 0};

  if (!m_rtc.getTime(timestamp)) {
    DBG.error("DS3231: ошибка чтения времени");
    timestamp = DateTime{};
  }

  const RuntimeConfig& c = Cfg();

  auto status = m_modbus.readRegisters(
    c.modbus_slave,
    c.modbus_func,
    c.modbus_start_reg,
    c.modbus_num_regs,
    regs
  );

  if (status == ModbusStatus::Ok) {
    m_lastValue = convert(regs[0], regs[1]);
    DBG.info("Modbus: [0x%04X,0x%04X] -> %.3f", regs[0], regs[1], m_lastValue);
  } else {
    DBG.error("Modbus: ошибка %d", static_cast<int>(status));
  }

  return m_lastValue;
}
