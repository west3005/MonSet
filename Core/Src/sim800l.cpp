#include "sim800l.hpp"
#include "debug_uart.hpp"
#include <cstring>
#include <cstdio>

SIM800L::SIM800L(UART_HandleTypeDef* uart,
                 GPIO_TypeDef* pwrPort, uint16_t pwrPin)
    : m_uart(uart), m_pwrPort(pwrPort), m_pwrPin(pwrPin)
{
}

void SIM800L::sendRaw(const char* data, uint16_t len)
{
    if (m_uart == nullptr || data == nullptr || len == 0) return;
    HAL_UART_Transmit(m_uart,
                      reinterpret_cast<const uint8_t*>(data),
                      len, 1000);
}

uint16_t SIM800L::readResponse(char* buf, uint16_t bsize, uint32_t timeout)
{
    if (buf == nullptr || bsize < 2) return 0;

    uint16_t idx = 0;
    uint32_t start = HAL_GetTick();
    std::memset(buf, 0, bsize);

    while ((HAL_GetTick() - start) < timeout && idx < (uint16_t)(bsize - 1)) {
        uint8_t ch;
        if (HAL_UART_Receive(m_uart, &ch, 1, 10) == HAL_OK) {
            buf[idx++] = static_cast<char>(ch);

            // Стоп-условия: OK / ERROR
            if (idx >= 4) {
                if (std::strstr(buf, "OK\r\n") || std::strstr(buf, "ERROR"))
                    break;
            }
        }
    }

    buf[idx] = '\0';
    return idx;
}

uint16_t SIM800L::waitFor(char* buf, uint16_t bsize,
                          const char* expected, uint32_t timeout)
{
    if (buf == nullptr || expected == nullptr || bsize < 2) return 0;

    uint16_t idx = 0;
    uint32_t start = HAL_GetTick();
    std::memset(buf, 0, bsize);

    while ((HAL_GetTick() - start) < timeout && idx < (uint16_t)(bsize - 1)) {
        uint8_t ch;
        if (HAL_UART_Receive(m_uart, &ch, 1, 10) == HAL_OK) {
            buf[idx++] = static_cast<char>(ch);
            if (std::strstr(buf, expected) || std::strstr(buf, "ERROR"))
                break;
        }
    }

    buf[idx] = '\0';
    return idx;
}

void SIM800L::powerOn()
{
    DBG.info("GSM: включение питания...");
    HAL_GPIO_WritePin(m_pwrPort, m_pwrPin, GPIO_PIN_SET);
    HAL_Delay(Config::SIM800_BOOT_MS);
    DBG.info("GSM: питание включено");
}

void SIM800L::powerOff()
{
    DBG.info("GSM: выключение питания");
    HAL_GPIO_WritePin(m_pwrPort, m_pwrPin, GPIO_PIN_RESET);
    HAL_Delay(500);
}

GsmStatus SIM800L::sendCommand(const char* cmd, char* resp,
                               uint16_t rsize, uint32_t timeout)
{
    if (resp == nullptr || rsize == 0) return GsmStatus::Timeout;

    char at[256];
    std::snprintf(at, sizeof(at), "AT%s\r\n", cmd ? cmd : "");

    DBG.data("[GSM>] %s", at);

    sendRaw(at, static_cast<uint16_t>(std::strlen(at)));
    readResponse(resp, rsize, timeout);

    DBG.data("[GSM<] %s", resp);

    if (std::strstr(resp, "OK"))    return GsmStatus::Ok;
    if (std::strstr(resp, "ERROR")) return GsmStatus::HttpErr;

    return GsmStatus::Timeout;
}

uint8_t SIM800L::getSignalQuality()
{
    char r[64];
    sendCommand("+CSQ", r, sizeof(r), 2000);

    auto* p = std::strstr(r, "+CSQ:");
    if (p) {
        uint8_t v = 0;
        std::sscanf(p, "+CSQ: %hhu", &v);
        return v;
    }
    return 99;
}

// ---- Новое ----
bool SIM800L::enableNetworkTimeSync()
{
    char r[128];

    // CLTS: network time sync (NITZ). Сеть должна поддерживать. [web:157]
    if (sendCommand("+CLTS=1", r, sizeof(r), 2000) != GsmStatus::Ok) {
        DBG.error("GSM: CLTS=1 fail");
        return false;
    }

    // Сохраняем профиль
    sendCommand("&W", r, sizeof(r), 2000);
    return true;
}

// ---- Новое ----
bool SIM800L::getCCLK(char* out, uint16_t outSize, uint32_t timeoutMs)
{
    if (!out || outSize < 4) return false;

    char r[256];
    if (sendCommand("+CCLK?", r, sizeof(r), timeoutMs) != GsmStatus::Ok) {
        return false;
    }

    // Оставим в out весь ответ: DS3231 парсер найдёт кавычки сам.
    std::strncpy(out, r, outSize - 1);
    out[outSize - 1] = '\0';

    // Минимальная проверка на наличие +CCLK:
    return (std::strstr(out, "+CCLK:") != nullptr) || (std::strchr(out, '\"') != nullptr);
}

/* ========= Инициализация ========= */
GsmStatus SIM800L::init()
{
    char r[256];

    DBG.info("GSM: проверка модуля...");

    bool connected = false;
    for (uint8_t i = 0; i < 10; i++) {
        if (sendCommand("", r, sizeof(r), 2000) == GsmStatus::Ok) {
            connected = true;
            break;
        }
        HAL_Delay(500);
    }

    if (!connected) {
        DBG.error("GSM: нет ответа");
        return GsmStatus::Timeout;
    }

    sendCommand("E0", r, sizeof(r), 2000);

    sendCommand("+CPIN?", r, sizeof(r), 5000);
    if (!std::strstr(r, "READY")) {
        DBG.error("GSM: нет SIM");
        return GsmStatus::NoSim;
    }

    DBG.info("GSM: SIM OK");

    bool registered = false;
    for (uint8_t i = 0; i < 30; i++) {
        sendCommand("+CREG?", r, sizeof(r), 2000);
        if (std::strstr(r, ",1") || std::strstr(r, ",5")) {
            registered = true;
            break;
        }
        HAL_Delay(1000);
    }

    if (!registered) {
        DBG.error("GSM: нет регистрации");
        return GsmStatus::NoReg;
    }

    DBG.info("GSM: сеть OK, CSQ=%d", getSignalQuality());

    // Включаем Network Time Sync (один раз тут — норм)
    enableNetworkTimeSync();

    // GPRS
    sendCommand("+SAPBR=0,1", r, sizeof(r), 5000);
    HAL_Delay(500);
    sendCommand("+SAPBR=3,1,\"Contype\",\"GPRS\"", r, sizeof(r), 5000);

    char cmd[128];
    std::snprintf(cmd, sizeof(cmd), "+SAPBR=3,1,\"APN\",\"%s\"", Config::GSM_APN);
    sendCommand(cmd, r, sizeof(r), 5000);

    if (std::strlen(Config::GSM_APN_USER) > 0) {
        std::snprintf(cmd, sizeof(cmd), "+SAPBR=3,1,\"USER\",\"%s\"", Config::GSM_APN_USER);
        sendCommand(cmd, r, sizeof(r), 5000);
    }

    if (std::strlen(Config::GSM_APN_PASS) > 0) {
        std::snprintf(cmd, sizeof(cmd), "+SAPBR=3,1,\"PWD\",\"%s\"", Config::GSM_APN_PASS);
        sendCommand(cmd, r, sizeof(r), 5000);
    }

    if (sendCommand("+SAPBR=1,1", r, sizeof(r), 15000) != GsmStatus::Ok) {
        DBG.error("GSM: GPRS fail");
        return GsmStatus::GprsErr;
    }

    sendCommand("+SAPBR=2,1", r, sizeof(r), 5000);
    DBG.info("GSM: GPRS OK, IP: %s", r);

    return GsmStatus::Ok;
}

/* ========= HTTP POST ========= */
uint16_t SIM800L::httpPost(const char* url, const char* json, uint16_t len)
{
    if (url == nullptr || json == nullptr || len == 0) return 0;

    char r[512], cmd[256];

    sendCommand("+HTTPTERM", r, sizeof(r), 2000);
    HAL_Delay(300);

    if (sendCommand("+HTTPINIT", r, sizeof(r), 5000) != GsmStatus::Ok) {
        DBG.error("HTTP: HTTPINIT fail");
        return 0;
    }

    sendCommand("+HTTPPARA=\"CID\",1", r, sizeof(r), 3000);

    std::snprintf(cmd, sizeof(cmd), "+HTTPPARA=\"URL\",\"%s\"", url);
    sendCommand(cmd, r, sizeof(r), 3000);

    sendCommand("+HTTPPARA=\"CONTENT\",\"application/json\"", r, sizeof(r), 3000);

    std::snprintf(cmd, sizeof(cmd),
                  "+HTTPPARA=\"USERDATA\",\"Authorization: Basic %s\"",
                  Config::SERVER_AUTH);
    sendCommand(cmd, r, sizeof(r), 3000);

    std::snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000\r\n", static_cast<int>(len));
    sendRaw(cmd, static_cast<uint16_t>(std::strlen(cmd)));

    waitFor(r, sizeof(r), "DOWNLOAD", 5000);
    if (!std::strstr(r, "DOWNLOAD")) {
        DBG.error("HTTP: нет DOWNLOAD");
        sendCommand("+HTTPTERM", r, sizeof(r), 3000);
        return 0;
    }

    DBG.info("HTTP: отправка %d байт...", len);
    sendRaw(json, len);
    waitFor(r, sizeof(r), "OK", 10000);

    sendRaw("AT+HTTPACTION=1\r\n", 17);
    waitFor(r, sizeof(r), "+HTTPACTION:", Config::SIM800_HTTP_TIMEOUT);

    uint16_t httpCode = 0;
    auto* p = std::strstr(r, "+HTTPACTION:");
    if (p) {
        int m = 0, s = 0, d = 0;
        std::sscanf(p, "+HTTPACTION: %d,%d,%d", &m, &s, &d);
        httpCode = static_cast<uint16_t>(s);
        DBG.info("HTTP: ответ %d, данных %d B", s, d);
    } else {
        DBG.error("HTTP: таймаут HTTPACTION");
    }

    sendCommand("+HTTPTERM", r, sizeof(r), 3000);
    return httpCode;
}

void SIM800L::disconnect()
{
    char r[64];
    sendCommand("+HTTPTERM", r, sizeof(r), 2000);
    sendCommand("+SAPBR=0,1", r, sizeof(r), 5000);
    DBG.info("GSM: отключён");
}
