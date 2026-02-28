/**
 * ================================================================
 * @file    app.hpp
 * @brief   Главный класс приложения — собирает все модули.
 * ================================================================
 */
#ifndef APP_HPP
#define APP_HPP

#include "config.hpp"
#include "debug_uart.hpp"
#include "ds3231.hpp"
#include "modbus_rtu.hpp"
#include "sim800l.hpp"
#include "sd_backup.hpp"
#include "sensor_reader.hpp"
#include "data_buffer.hpp"
#include "power_manager.hpp"

/**
 * @brief  Режим работы
 */
enum class SystemMode : uint8_t {
    Sleep = 0,   /* Энергосбережение */
    Debug = 1    /* Постоянная отправка */
};

/**
 * @brief  Главный класс приложения.
 *         Инкапсулирует все модули и бизнес-логику.
 *
 *         Порядок:
 *         1. Конструктор (передаём HAL-хэндлы)
 *         2. init()       — инициализация всех модулей
 *         3. run()        — бесконечный цикл
 */
class App {
public:
    App();

    /** Инициализация всех модулей */
    void init();

    /** Главный цикл (никогда не возвращается) */
    [[noreturn]] void run();

private:
    /* Модули */
    DS3231          m_rtc;
    ModbusRTU       m_modbus;
    SIM800L         m_gsm;
    SdBackup        m_sdBackup;
    SensorReader    m_sensor;
    DataBuffer      m_buffer;
    PowerManager    m_power;

    /* Состояние */
    SystemMode      m_mode = SystemMode::Sleep;

    /* JSON буфер (статический, большой) */
    char            m_json[Config::JSON_BUFFER_SIZE]{};

    /* Методы */
    SystemMode readMode();
    void       ledOn();
    void       ledOff();
    void       ledBlink(uint8_t count, uint32_t ms);

    void       transmitBuffer();
    void       transmitSingle(float value, const DateTime& dt);
    void retransmitBackup();
};

#endif /* APP_HPP */
