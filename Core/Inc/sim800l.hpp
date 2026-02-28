#ifndef SIM800L_HPP
#define SIM800L_HPP

#include "main.h"
#include "config.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>

enum class GsmStatus : uint8_t {
    Ok       = 0,
    Timeout  = 1,
    NoSim    = 2,
    NoReg    = 3,
    GprsErr  = 4,
    HttpErr  = 5
};

class SIM800L {
public:
    SIM800L(UART_HandleTypeDef* uart,
            GPIO_TypeDef* pwrPort, uint16_t pwrPin);

    void powerOn();
    void powerOff();

    GsmStatus init();

    GsmStatus sendCommand(const char* cmd, char* resp,
                          uint16_t rsize, uint32_t timeout);

    uint16_t httpPost(const char* url, const char* json, uint16_t len);
    void disconnect();
    uint8_t getSignalQuality();

    // ===== Новое: сетевое время =====
    // Включить Network Time Sync (NITZ) внутри SIM800: AT+CLTS=1, AT&W.
    bool enableNetworkTimeSync();

    // Получить строку времени от SIM800: +CCLK: "yy/MM/dd,hh:mm:ss±zz"
    bool getCCLK(char* out, uint16_t outSize, uint32_t timeoutMs = 2000);

private:
    UART_HandleTypeDef* m_uart;
    GPIO_TypeDef*       m_pwrPort;
    uint16_t            m_pwrPin;
    char                m_rxBuf[Config::GSM_RX_BUF_SIZE]{};

    void     sendRaw(const char* data, uint16_t len);
    uint16_t readResponse(char* buf, uint16_t bsize, uint32_t timeout);
    uint16_t waitFor(char* buf, uint16_t bsize,
                     const char* expected, uint32_t timeout);
};

#endif /* SIM800L_HPP */
