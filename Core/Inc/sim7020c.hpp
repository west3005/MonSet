/**
 * ================================================================
 * @file    sim7020c.hpp
 * @brief   Драйвер SIM7020C NB-IoT модема (замена SIM800L).
 *
 * Поддерживаемые протоколы:
 *   - TCP socket (AT+CSOC / AT+CSOCON / AT+CSOSEND)
 *   - HTTP POST через TCP (прямая реализация поверх CSOC)
 *   - MQTT (AT+CMQNEW / AT+CMQCON / AT+CMQPUB)
 *
 * Аппаратная привязка:
 *   UART:     huart2  (PA2/PA3, 115200 8N1)
 *   PWR_EN:   PIN_CELL_PWR_EN  (PC2)
 *   PWRKEY:   PIN_CELL_PWRKEY  (PD6, OD, активный низкий импульс)
 *   RESET:    PIN_CELL_RESET   (PD7, OD, активный низкий)
 *   STATUS:   PIN_CELL_STATUS  (PD9, вход — модем включён при HIGH)
 *
 * Питание:   3.3 В (рекомендуется через LDO с запасом 500 мА+)
 *
 * Примечание:
 *   SIM7020C — NB-IoT (Cat-NB1), работает в сетях с поддержкой
 *   NB-IoT (полосы B20/B8 в России). Нет GPRS/LTE.
 *   APN для российских операторов: см. README.
 * ================================================================
 */
#pragma once

#include "main.h"
#include "config.hpp"
#include "runtime_config.hpp"

#include <cstdint>
#include <cstring>

// ================================================================
// Коды возврата операций модема
// ================================================================
enum class GsmStatus : uint8_t {
    Ok        = 0,  ///< Успех
    Timeout   = 1,  ///< Нет ответа за отведённое время
    NoSim     = 2,  ///< SIM не вставлена / не ответила
    NoReg     = 3,  ///< Нет регистрации в сети
    PdnErr    = 4,  ///< Ошибка активации PDP-контекста (PDN)
    TcpErr    = 5,  ///< Ошибка TCP-соединения
    HttpErr   = 6,  ///< HTTP-транзакция не удалась
    MqttErr   = 7,  ///< MQTT-ошибка
};

// ================================================================
// SIM7020C
// ================================================================
class SIM7020C {
public:
    /**
     * @param uart     Указатель на UART (huart2)
     * @param pwrPort  GPIO порт PIN_CELL_PWR_EN
     * @param pwrPin   GPIO пин  PIN_CELL_PWR_EN
     */
    SIM7020C(UART_HandleTypeDef* uart,
             GPIO_TypeDef*        pwrPort,
             uint16_t             pwrPin);

    // ---- Управление питанием ----

    /** Включить питание модема и дождаться готовности (RDY). */
    void powerOn();

    /** Выключить питание через PIN_CELL_PWR_EN. */
    void powerOff();

    /** Аппаратный сброс через PIN_CELL_RESET (≥100 мс LOW). */
    void hardReset();

    // ---- Инициализация ----

    /**
     * Инициализировать модем, зарегистрироваться в сети и поднять
     * PDN-контекст (PDP). APN берётся из RuntimeConfig::gsm_apn.
     * @return GsmStatus::Ok при успехе.
     */
    GsmStatus init();

    /** Деактивировать PDN и снять TCP/MQTT соединения. */
    void disconnect();

    // ---- HTTP (реализован поверх TCP socket, совместим с SIM800L API) ----

    /**
     * HTTP POST данных в формате JSON.
     * @param url   "http://host[:port]/path"
     * @param json  Буфер с JSON-строкой (не NULL-terminated: используется len)
     * @param len   Длина JSON в байтах
     * @return HTTP-код ответа (200, 400 …), 0 при ошибке соединения.
     */
    uint16_t httpPost(const char* url, const char* json, uint16_t len);

    // ---- MQTT (нативный для SIM7020C) ----

    /**
     * Подключиться к MQTT-брокеру.
     * @param broker  Имя хоста или IP
     * @param port    Порт (обычно 1883)
     * @return GsmStatus::Ok при успехе.
     */
    GsmStatus mqttConnect(const char* broker, uint16_t port);

    /**
     * Опубликовать данные в топик.
     * @param topic   MQTT-топик
     * @param payload Данные (строка)
     * @param qos     0, 1 или 2 (рекомендуется 1 для надёжности)
     * @return GsmStatus::Ok при успехе.
     */
    GsmStatus mqttPublish(const char* topic, const char* payload, uint8_t qos = 1);

    /** Отключиться от MQTT-брокера. */
    void mqttDisconnect();

    // ---- Утилиты ----

    /** Уровень сигнала 0..31 (99 = неизвестно). */
    uint8_t getSignalQuality();

    /**
     * Низкоуровневая отправка AT-команды.
     * @param cmd      Строка после "AT", например "+CGMM"
     * @param resp     Буфер для ответа
     * @param rsize    Размер буфера
     * @param timeout  Таймаут ожидания OK/ERROR, мс
     * @return GsmStatus::Ok / ::HttpErr / ::Timeout
     */
    GsmStatus sendCommand(const char* cmd,
                          char*       resp,
                          uint16_t    rsize,
                          uint32_t    timeout);

    // ---- TLS transport API (используется Sim7020cTls) ----
    // Открытый доступ к sendRaw/waitFor для TLS BIO callbacks.
    // Не использовать в прикладном коде напрямую.

    /** Передать сырые байты в UART модема (без "AT" префикса). */
    void sendRaw_pub(const char* data, uint16_t len) { sendRaw(data, len); }

    /**
     * Ждать строку expected в UART-ответе модема.
     * @return Количество принятых байт.
     */
    uint16_t waitFor_pub(char* buf, uint16_t bsize,
                         const char* expected, uint32_t timeout)
    {
        return waitFor(buf, bsize, expected, timeout);
    }

private:
    UART_HandleTypeDef* m_uart;
    GPIO_TypeDef*       m_pwrPort;
    uint16_t            m_pwrPin;

    char m_rxBuf[Config::GSM_RX_BUF_SIZE]{};

    // Идентификатор MQTT-сессии (индекс 0..3 для AT+CMQNEW)
    static constexpr uint8_t MQTT_IDX = 0;

    // TCP socket id для httpPost
    static constexpr uint8_t HTTP_SOCK_IDX = 0;

    // ---- Внутренние методы ----
    void     sendRaw(const char* data, uint16_t len);
    uint16_t readResponse(char* buf, uint16_t bsize, uint32_t timeout);
    uint16_t waitFor(char* buf, uint16_t bsize, const char* expected, uint32_t timeout);

    GsmStatus activatePdn();
    bool      waitRdy(uint32_t timeoutMs);
};
