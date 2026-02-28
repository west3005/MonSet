/**
 * ================================================================
 * @file    sensor_reader.cpp
 * @brief   Реализация чтения датчика.
 * ================================================================
 */
#include "sensor_reader.hpp"
#include "debug_uart.hpp"

SensorReader::SensorReader(ModbusRTU& modbus, DS3231& rtc)
    : m_modbus(modbus), m_rtc(rtc)
{
}

float SensorReader::convert(uint16_t reg0, uint16_t reg1)
{
    /* Формула из Python:
     * zero_level - ((reg0 * 65536 + reg1) / 10) / divider */
    float raw = static_cast<float>(
        static_cast<uint32_t>(reg0) * 65536UL +
        static_cast<uint32_t>(reg1)) / 10.0f;
    return Config::SENSOR_ZERO_LEVEL - raw / Config::SENSOR_DIVIDER;
}

float SensorReader::read(DateTime& timestamp)
{
    m_lastValue = -9999.0f;
    uint16_t regs[2] = {0, 0};

    /* Время */
    if (!m_rtc.getTime(timestamp)) {
        DBG.error("DS3231: ошибка чтения времени");
        timestamp = DateTime{};
    }

    /* Modbus */
    auto status = m_modbus.readRegisters(
        Config::MODBUS_SLAVE,
        Config::MODBUS_FUNC_CODE,
        Config::MODBUS_START_REG,
        Config::MODBUS_NUM_REGS,
        regs);

    if (status == ModbusStatus::Ok) {
        m_lastValue = convert(regs[0], regs[1]);
        DBG.info("Modbus: [0x%04X,0x%04X] -> %.3f",
                 regs[0], regs[1], m_lastValue);
    } else {
        DBG.error("Modbus: ошибка %d", static_cast<int>(status));
    }

    return m_lastValue;
}
