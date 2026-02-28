/**
 * ================================================================
 * @file    sensor_reader.hpp
 * @brief   Чтение показаний датчика через Modbus + время DS3231.
 * ================================================================
 */
#ifndef SENSOR_READER_HPP
#define SENSOR_READER_HPP

#include "modbus_rtu.hpp"
#include "ds3231.hpp"
#include "config.hpp"

class SensorReader {
public:
    SensorReader(ModbusRTU& modbus, DS3231& rtc);

    /**
     * @brief  Прочитать датчик и время
     * @param  timestamp — заполняется текущим временем
     * @retval Значение датчика, или -9999.0 при ошибке
     */
    float read(DateTime& timestamp);

    /** Последнее прочитанное значение */
    float lastValue() const { return m_lastValue; }

private:
    ModbusRTU& m_modbus;
    DS3231&    m_rtc;
    float      m_lastValue = -9999.0f;

    /** Конвертация регистров в физическую величину (уровень) */
    static float convert(uint16_t reg0, uint16_t reg1);
};

#endif /* SENSOR_READER_HPP */
