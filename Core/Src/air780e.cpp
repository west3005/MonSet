/**
 * ================================================================
 * @file    air780e.cpp
 * @brief   Реализация драйвера Air780E 4G CAT1.
 *
 * AT Command Manual: Air780E Series AT Command Manual v1.x
 * Hardware Design:   Air780E Hardware Design Manual v1.2.4
 * ================================================================
 */
#include "air780e.hpp"
#include "debug_uart.hpp"
#include "board_pins.hpp"
#include "uart_ringbuf.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

// ============================================================================
// Конструктор
// ============================================================================
Air780E::Air780E(UART_HandleTypeDef* uart,
                 GPIO_TypeDef*        pwrPort,
                 uint16_t             pwrPin)
    : m_uart(uart), m_pwrPort(pwrPort), m_pwrPin(pwrPin)
{}

// ============================================================================
// Низкоуровневый I/O
// ============================================================================
void Air780E::sendRaw(const char* data, uint16_t len)
{
    if (!m_uart || !data || len == 0) return;
    HAL_UART_Transmit(m_uart,
                      reinterpret_cast<const uint8_t*>(data),
                      len, 1000);
}

uint16_t Air780E::readResponse(char* buf, uint16_t bsize, uint32_t timeout)
{
    if (!buf || bsize < 2) return 0;

    uint16_t idx      = 0;
    uint32_t start    = HAL_GetTick();
    uint32_t lastByte = start;
    std::memset(buf, 0, bsize);

    while (idx < (uint16_t)(bsize - 1))
    {
        uint8_t ch;
        if (g_air780_rxbuf.pop(ch)) {
            buf[idx++] = static_cast<char>(ch);
            lastByte = HAL_GetTick();

            if (idx >= 4) {
                if (std::strstr(buf, "OK\r\n")    ||
                    std::strstr(buf, "ERROR\r\n")  ||
                    std::strstr(buf, "+CME ERROR") ||
                    std::strstr(buf, "+CMS ERROR"))
                    break;
            }
        } else {
            uint32_t now = HAL_GetTick();
            // Тишина 50 мс после последнего байта — конец ответа
            if (idx > 0 && (now - lastByte) >= 50) break;
            // Общий таймаут
            if ((now - start) >= timeout) break;
            // Небольшая пауза чтобы не жечь CPU
            HAL_Delay(1);
        }
        IWDG->KR = 0xAAAA;
    }

    buf[idx] = '\0';
    return idx;
}

uint16_t Air780E::waitFor(char* buf, uint16_t bsize,
                           const char* expected, uint32_t timeout)
{
    if (!buf || !expected || bsize < 2) return 0;

    uint16_t idx      = 0;
    uint32_t start    = HAL_GetTick();
    uint32_t lastByte = start;
    std::memset(buf, 0, bsize);

    while (idx < (uint16_t)(bsize - 1))
    {
        uint8_t ch;
        if (g_air780_rxbuf.pop(ch)) {
            buf[idx++] = static_cast<char>(ch);
            lastByte = HAL_GetTick();

            if (std::strstr(buf, expected)    ||
                std::strstr(buf, "+CME ERROR") ||
                std::strstr(buf, "ERROR\r\n"))
                break;
        } else {
            uint32_t now = HAL_GetTick();
            if (idx > 0 && (now - lastByte) >= 50) break;
            if ((now - start) >= timeout) break;
            HAL_Delay(1);
        }
        IWDG->KR = 0xAAAA;
    }

    buf[idx] = '\0';
    return idx;
}

GsmStatus Air780E::sendCommand(const char* cmd,
                                char*       resp,
                                uint16_t    rsize,
                                uint32_t    timeout)
{
    if (!resp || rsize == 0) return GsmStatus::Timeout;

    char at[256];
    std::snprintf(at, sizeof(at), "AT%s\r\n", cmd ? cmd : "");
    DBG.data("[AIR>] %s", at);

    // Сброс кольцевого буфера перед командой
    g_air780_rxbuf.clear();

    sendRaw(at, (uint16_t)std::strlen(at));
    readResponse(resp, rsize, timeout);

    DBG.data("[AIR<] %s", resp);

    if (std::strstr(resp, "OK"))    return GsmStatus::Ok;
    if (std::strstr(resp, "ERROR")) return GsmStatus::HttpErr;
    return GsmStatus::Timeout;
}

// ============================================================================
// Управление питанием
// ============================================================================
bool Air780E::waitRdy(uint32_t timeoutMs)
{
    char     buf[256]{};
    uint16_t idx   = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeoutMs)
    {
        uint8_t ch;
        if (HAL_UART_Receive(m_uart, &ch, 1, 20) == HAL_OK) {
            if (idx < sizeof(buf) - 1)
                buf[idx++] = static_cast<char>(ch);

            // Air780E шлёт несколько строк при старте:
            // +CPIN: READY
            // SMS DONE
            // PB DONE     ← полная готовность
            if (std::strstr(buf, "PB DONE") ||
                std::strstr(buf, "SMS DONE") ||
                std::strstr(buf, "RDY")) {
                DBG.info("Air780E: готов за %lu мс",
                         (unsigned long)(HAL_GetTick() - start));
                HAL_Delay(200); // пауза после PB DONE до первой AT
                return true;
            }
            // Сдвиг буфера чтобы не переполнился на длинных URC
            if (idx >= sizeof(buf) - 10) {
                std::memmove(buf, buf + 128, idx - 128);
                idx -= 128;
            }
        }
        IWDG->KR = 0xAAAA;
    }
    return false;
}

void Air780E::powerOn()
{
    DBG.info("Air780E: включение питания...");

    // Подать питание
    HAL_GPIO_WritePin(m_pwrPort, m_pwrPin, GPIO_PIN_SET);
    HAL_Delay(300);

    // Если модем уже включён — STATUS = HIGH
    if (HAL_GPIO_ReadPin(PIN_CELL_STATUS_PORT, PIN_CELL_STATUS_PIN) == GPIO_PIN_SET) {
        DBG.info("Air780E: уже включён (STATUS=HIGH)");
        return;
    }

    // PWRKEY LOW ≥ 1000 мс для включения (Hardware Design Manual §4.2)
    DBG.info("Air780E: подача PWRKEY (1000 мс)...");
    HAL_GPIO_WritePin(PIN_CELL_PWRKEY_PORT, PIN_CELL_PWRKEY_PIN, GPIO_PIN_RESET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(PIN_CELL_PWRKEY_PORT, PIN_CELL_PWRKEY_PIN, GPIO_PIN_SET);

    DBG.info("Air780E: ожидание готовности (PB DONE)...");
    if (!waitRdy(Config::SIM7020_BOOT_MS)) {
        DBG.warn("Air780E: RDY не получен — продолжаем");
    }
}

void Air780E::powerOff()
{
    DBG.info("Air780E: выключение...");

    if (HAL_GPIO_ReadPin(PIN_CELL_STATUS_PORT, PIN_CELL_STATUS_PIN) == GPIO_PIN_RESET) {
        DBG.info("Air780E: уже выключен, пропускаем");
        HAL_GPIO_WritePin(m_pwrPort, m_pwrPin, GPIO_PIN_RESET);
        return;
    }

    char r[64];
    // AT+CPWROFF — правильная команда выключения Air780E (не +CPOF!)
    sendCommand("+CPWROFF", r, sizeof(r), 3000);
    HAL_Delay(1000);

    // Если не выключился — аппаратный PWRKEY LOW >= 1500 мс
    if (HAL_GPIO_ReadPin(PIN_CELL_STATUS_PORT, PIN_CELL_STATUS_PIN) == GPIO_PIN_SET) {
        HAL_GPIO_WritePin(PIN_CELL_PWRKEY_PORT, PIN_CELL_PWRKEY_PIN, GPIO_PIN_RESET);
        HAL_Delay(1500);
        HAL_GPIO_WritePin(PIN_CELL_PWRKEY_PORT, PIN_CELL_PWRKEY_PIN, GPIO_PIN_SET);
        HAL_Delay(500);
    }

    HAL_GPIO_WritePin(m_pwrPort, m_pwrPin, GPIO_PIN_RESET);
}

void Air780E::hardReset()
{
    DBG.warn("Air780E: аппаратный сброс");
    HAL_GPIO_WritePin(PIN_CELL_RESET_PORT, PIN_CELL_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(PIN_CELL_RESET_PORT, PIN_CELL_RESET_PIN, GPIO_PIN_SET);
    waitRdy(Config::SIM7020_BOOT_MS);
}

// ============================================================================
// Инициализация
// ============================================================================
GsmStatus Air780E::activatePdn()
{
    const RuntimeConfig& c = Cfg();
    char r[256], cmd[128];

    // Задать APN
    std::snprintf(cmd, sizeof(cmd),
                  "+CGDCONT=1,\"IP\",\"%s\"", c.gsm_apn);
    sendCommand(cmd, r, sizeof(r), Config::SIM7020_CMD_TIMEOUT_MS);

    // Активировать PDP контекст
    // Если уже активен — ERROR/CME ERROR, это нормально, продолжаем
    sendCommand("+CGACT=1,1", r, sizeof(r), Config::SIM7020_PDN_TIMEOUT_MS);
    if (std::strstr(r, "ERROR") && !std::strstr(r, "OK")) {
        // Проверим что IP всё равно есть
        sendCommand("+CGPADDR=1", r, sizeof(r), Config::SIM7020_CMD_TIMEOUT_MS);
        if (!std::strstr(r, "+CGPADDR:")) {
            DBG.error("Air780E: нет IP после CGACT");
            return GsmStatus::PdnErr;
        }
    }

    // AT+NETOPEN — открыть TCP стек
    // ERROR = уже открыт, это нормально
    sendCommand("+NETOPEN", r, sizeof(r), 10000);
    // Не проверяем — ERROR здесь допустим

    // Получить IP для отладки
    sendCommand("+CGPADDR=1", r, sizeof(r), Config::SIM7020_CMD_TIMEOUT_MS);
    DBG.info("Air780E: PDN UP — %s", r);

    return GsmStatus::Ok;  // всегда OK если дошли сюда
}

GsmStatus Air780E::init()
{
    char r[256];

    // Сброс UART RX буфера
    __HAL_UART_FLUSH_DRREGISTER(m_uart);
    HAL_Delay(100);

    DBG.info("Air780E: проверка связи...");

    // Выключить эхо первым делом (3 попытки, игнорируем результат)
    for (uint8_t i = 0; i < 3; i++) {
        sendCommand("E0", r, sizeof(r), 1000);
        HAL_Delay(200);
        IWDG->KR = 0xAAAA;
    }

    // Проверка AT
    bool alive = false;
    for (uint8_t i = 0; i < 10 && !alive; i++) {
        if (sendCommand("", r, sizeof(r), 2000) == GsmStatus::Ok)
            alive = true;
        else
            HAL_Delay(500);
        IWDG->KR = 0xAAAA;
    }
    if (!alive) {
        DBG.error("Air780E: нет ответа на AT");
        return GsmStatus::Timeout;
    }
    DBG.info("Air780E: связь OK");

    // ATE0 повторно для надёжности
    sendCommand("E0", r, sizeof(r), 2000);

    // Проверить SIM
    for (uint8_t i = 0; i < 5; i++) {
        sendCommand("+CPIN?", r, sizeof(r), 5000);
        if (std::strstr(r, "READY")) break;
        HAL_Delay(1000);
        IWDG->KR = 0xAAAA;
    }
    if (!std::strstr(r, "READY")) {
        DBG.error("Air780E: SIM не готова — %s", r);
        return GsmStatus::NoSim;
    }
    DBG.info("Air780E: SIM OK");

    // Ожидание регистрации в 4G сети (AT+CREG для GSM/LTE, не CEREG!)
    bool registered = false;
    for (uint8_t i = 0; i < 60 && !registered; i++) {
        sendCommand("+CREG?", r, sizeof(r), 2000);
        // 1 = домашняя сеть, 5 = роуминг
        if (std::strstr(r, ",1") || std::strstr(r, ",5")) {
            registered = true;
        } else {
            HAL_Delay(1000);
            IWDG->KR = 0xAAAA;
        }
    }
    if (!registered) {
        DBG.error("Air780E: нет регистрации в сети");
        return GsmStatus::NoReg;
    }

    DBG.info("Air780E: сеть OK, CSQ=%d", getSignalQuality());
    return activatePdn();
}

void Air780E::disconnect()
{
    char r[64];
    if (m_mqttStarted) mqttDisconnect();

    // Закрыть HTTP сессию — игнорируем ошибку если не открыта
    sendCommand("+HTTPTERM", r, sizeof(r), 2000);

    // Закрыть TCP стек — игнорируем ошибку
    sendCommand("+NETCLOSE", r, sizeof(r), 5000);

    // Деактивировать PDP — некоторые операторы запрещают, игнорируем
    sendCommand("+CGACT=0,1", r, sizeof(r), 5000);

    DBG.info("Air780E: отключён");
}

// ============================================================================
// Утилиты
// ============================================================================
uint8_t Air780E::getSignalQuality()
{
    char r[64];
    sendCommand("+CSQ", r, sizeof(r), 2000);
    const char* p = std::strstr(r, "+CSQ:");
    if (p) {
        uint8_t v = 99;
        std::sscanf(p, "+CSQ: %hhu", &v);
        return v;
    }
    return 99;
}

// ============================================================================
// HTTP POST (AT+HTTPINIT / HTTPPARA / HTTPDATA / HTTPACTION / HTTPREAD)
// ============================================================================
uint16_t Air780E::httpPost(const char* url, const char* json, uint16_t len)
{
    if (!url || !json || len == 0) return 0;

    const RuntimeConfig& c = Cfg();
    char r[512], cmd[256];

    // 1. Инициализация HTTP-сервиса
    sendCommand("+HTTPTERM", r, sizeof(r), 2000); // сброс предыдущей сессии
    HAL_Delay(200);

    if (sendCommand("+HTTPINIT", r, sizeof(r),
                    Config::SIM7020_CMD_TIMEOUT_MS) != GsmStatus::Ok) {
        DBG.error("Air780E HTTP: HTTPINIT fail");
        return 0;
    }

    // 2. Параметры
    // PDP контекст
    sendCommand("+HTTPPARA=\"CID\",1", r, sizeof(r),
                Config::SIM7020_CMD_TIMEOUT_MS);

    // URL
    std::snprintf(cmd, sizeof(cmd), "+HTTPPARA=\"URL\",\"%s\"", url);
    if (sendCommand(cmd, r, sizeof(r),
                    Config::SIM7020_CMD_TIMEOUT_MS) != GsmStatus::Ok) {
        DBG.error("Air780E HTTP: HTTPPARA URL fail");
        sendCommand("+HTTPTERM", r, sizeof(r), 2000);
        return 0;
    }

    // Content-Type
    sendCommand("+HTTPPARA=\"CONTENT\",\"application/json\"",
                r, sizeof(r), Config::SIM7020_CMD_TIMEOUT_MS);

    // Authorization (если задан)
    if (c.server_auth_b64[0]) {
        std::snprintf(cmd, sizeof(cmd),
                      "+HTTPPARA=\"USERDATA\",\"Authorization: Basic %s\"",
                      c.server_auth_b64);
        sendCommand(cmd, r, sizeof(r), Config::SIM7020_CMD_TIMEOUT_MS);
    }

    // 3. Ввод тела запроса: AT+HTTPDATA=<size>,<timeout_ms>
    std::snprintf(cmd, sizeof(cmd), "+HTTPDATA=%u,10000", (unsigned)len);
    sendRaw("AT", 2);
    {
        char fullCmd[64];
        std::snprintf(fullCmd, sizeof(fullCmd), "AT+HTTPDATA=%u,10000\r\n", (unsigned)len);
        __HAL_UART_FLUSH_DRREGISTER(m_uart);
        sendRaw(fullCmd, (uint16_t)std::strlen(fullCmd));
    }

    // Ждём "DOWNLOAD" приглашение
    waitFor(r, sizeof(r), "DOWNLOAD", 5000);
    if (!std::strstr(r, "DOWNLOAD")) {
        DBG.error("Air780E HTTP: нет DOWNLOAD prompt (resp=%s)", r);
        sendCommand("+HTTPTERM", r, sizeof(r), 2000);
        return 0;
    }

    // Отправить тело JSON
    sendRaw(json, len);
    // Ждём OK после ввода данных
    readResponse(r, sizeof(r), 3000);

    // 4. POST запрос: AT+HTTPACTION=1
    // Ответ: +HTTPACTION: 1,<http_code>,<data_len>
    __HAL_UART_FLUSH_DRREGISTER(m_uart);
    sendRaw("AT+HTTPACTION=1\r\n", 17);
    waitFor(r, sizeof(r), "+HTTPACTION:", Config::SIM7020_TCP_TIMEOUT_MS);

    uint16_t httpCode = 0;
    {
        const char* p = std::strstr(r, "+HTTPACTION:");
        if (p) {
            unsigned method = 0, code = 0, dlen = 0;
            std::sscanf(p, "+HTTPACTION: %u,%u,%u", &method, &code, &dlen);
            httpCode = (uint16_t)code;
        }
    }
    DBG.info("Air780E HTTP: код ответа=%u", httpCode);

    // 5. Завершить сессию
    sendCommand("+HTTPTERM", r, sizeof(r), 2000);

    return httpCode;
}

// ============================================================================
// MQTT (AT+CMQTTSTART / CMQTTACCQ / CMQTTCONNECT / CMQTTPUB)
// ============================================================================
GsmStatus Air780E::mqttConnect(const char* broker, uint16_t port)
{
    if (!broker) return GsmStatus::MqttErr;

    char r[512], cmd[256];

    // Остановить предыдущую сессию
    if (m_mqttStarted) {
        std::snprintf(cmd, sizeof(cmd), "+CMQTTDISC=%hhu,10", MQTT_IDX);
        sendCommand(cmd, r, sizeof(r), 5000);
        std::snprintf(cmd, sizeof(cmd), "+CMQTTREL=%hhu", MQTT_IDX);
        sendCommand(cmd, r, sizeof(r), 2000);
        sendCommand("+CMQTTSTOP", r, sizeof(r), 3000);
        m_mqttStarted = false;
        HAL_Delay(500);
    }

    // 1. Запустить MQTT сервис
    if (sendCommand("+CMQTTSTART", r, sizeof(r), 5000) != GsmStatus::Ok) {
        DBG.error("Air780E MQTT: CMQTTSTART fail");
        return GsmStatus::MqttErr;
    }
    m_mqttStarted = true;

    // 2. Получить клиент (clientId = "MonSet_XXXX")
    char clientId[32];
    std::snprintf(clientId, sizeof(clientId),
                  "MonSet_%04X", (unsigned)(HAL_GetTick() & 0xFFFFu));
    std::snprintf(cmd, sizeof(cmd),
                  "+CMQTTACCQ=%hhu,\"%s\"", MQTT_IDX, clientId);
    if (sendCommand(cmd, r, sizeof(r), 3000) != GsmStatus::Ok) {
        DBG.error("Air780E MQTT: CMQTTACCQ fail");
        return GsmStatus::MqttErr;
    }

    // 3. Подключиться к брокеру
    // AT+CMQTTCONNECT=<idx>,"tcp://<broker>:<port>",<timeout>,<cleanSession>
    std::snprintf(cmd, sizeof(cmd),
                  "+CMQTTCONNECT=%hhu,\"tcp://%s:%u\",60,1",
                  MQTT_IDX, broker, port);
    if (sendCommand(cmd, r, sizeof(r),
                    Config::SIM7020_MQTT_TIMEOUT_MS) != GsmStatus::Ok) {
        DBG.error("Air780E MQTT: CMQTTCONNECT fail — %s", r);
        return GsmStatus::MqttErr;
    }

    DBG.info("Air780E MQTT: подключён к %s:%u", broker, port);
    return GsmStatus::Ok;
}

GsmStatus Air780E::mqttPublish(const char* topic,
                                const char* payload,
                                uint8_t     qos)
{
    if (!topic || !payload) return GsmStatus::MqttErr;

    uint16_t topicLen   = (uint16_t)std::strlen(topic);
    uint16_t payloadLen = (uint16_t)std::strlen(payload);
    char r[256], cmd[128];

    // 1. Задать топик: AT+CMQTTTOPIC=<idx>,<topic_len>
    std::snprintf(cmd, sizeof(cmd),
                  "+CMQTTTOPIC=%hhu,%u", MQTT_IDX, topicLen);
    {
        char fullCmd[64];
        std::snprintf(fullCmd, sizeof(fullCmd),
                      "AT+CMQTTTOPIC=%hhu,%u\r\n", MQTT_IDX, topicLen);
        __HAL_UART_FLUSH_DRREGISTER(m_uart);
        sendRaw(fullCmd, (uint16_t)std::strlen(fullCmd));
    }
    waitFor(r, sizeof(r), ">", 3000);
    if (!std::strstr(r, ">")) {
        DBG.error("Air780E MQTT: нет prompt для топика");
        return GsmStatus::MqttErr;
    }
    sendRaw(topic, topicLen);
    readResponse(r, sizeof(r), 2000);

    // 2. Задать payload: AT+CMQTTPAYLOAD=<idx>,<payload_len>
    {
        char fullCmd[64];
        std::snprintf(fullCmd, sizeof(fullCmd),
                      "AT+CMQTTPAYLOAD=%hhu,%u\r\n", MQTT_IDX, payloadLen);
        __HAL_UART_FLUSH_DRREGISTER(m_uart);
        sendRaw(fullCmd, (uint16_t)std::strlen(fullCmd));
    }
    waitFor(r, sizeof(r), ">", 3000);
    if (!std::strstr(r, ">")) {
        DBG.error("Air780E MQTT: нет prompt для payload");
        return GsmStatus::MqttErr;
    }
    sendRaw(payload, payloadLen);
    readResponse(r, sizeof(r), 2000);

    // 3. Публикация: AT+CMQTTPUB=<idx>,<qos>,<timeout>
    std::snprintf(cmd, sizeof(cmd),
                  "+CMQTTPUB=%hhu,%hhu,60", MQTT_IDX, qos);
    if (sendCommand(cmd, r, sizeof(r),
                    Config::SIM7020_MQTT_TIMEOUT_MS) != GsmStatus::Ok) {
        DBG.error("Air780E MQTT: CMQTTPUB fail — %s", r);
        return GsmStatus::MqttErr;
    }

    DBG.info("Air780E MQTT: publish OK topic=%s", topic);
    return GsmStatus::Ok;
}

void Air780E::mqttDisconnect()
{
    char r[64], cmd[32];

    std::snprintf(cmd, sizeof(cmd), "+CMQTTDISC=%hhu,10", MQTT_IDX);
    sendCommand(cmd, r, sizeof(r), 5000);

    std::snprintf(cmd, sizeof(cmd), "+CMQTTREL=%hhu", MQTT_IDX);
    sendCommand(cmd, r, sizeof(r), 2000);

    sendCommand("+CMQTTSTOP", r, sizeof(r), 3000);
    m_mqttStarted = false;
    DBG.info("Air780E MQTT: отключён");
}
