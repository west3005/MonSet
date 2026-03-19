/**
 * ================================================================
 * @file    sim7020c_tls.hpp
 * @brief   TLS-туннель поверх SIM7020C TCP-сокета.
 *
 * Архитектура:
 *   SIM7020C TCP socket (AT+CSOC/CSOCON/CSODSEND/CSORCV)
 *        ↕  UART (PA2/PA3, 115200)
 *   mbedTLS BIO callbacks (Sim7020cTlsBio::send / recv)
 *        ↕
 *   mbedtls_ssl_write / mbedtls_ssl_read
 *        ↕
 *   HTTP/1.1 application layer
 *
 * mbedTLS уже настроен в проекте (monset_mbedtls_config.h):
 *   - MBEDTLS_ENTROPY_HARDWARE_ALT → mbedtls_hardware_poll() из STM32 RNG
 *   - MBEDTLS_PLATFORM_MS_TIME_ALT → HAL_GetTick()
 *   - MBEDTLS_NET_C выключен (нет POSIX)
 *   - TLS 1.3 выключен (используем TLS 1.2)
 *
 * Использование:
 *   @code
 *   Sim7020cTls tls(sim7020c_instance, timeoutMs);
 *   if (tls.connect("thingsboard.cloud", 443) == 0) {
 *       int code = tls.httpsPost("/api/v1/TOKEN/telemetry",
 *                                "thingsboard.cloud",
 *                                nullptr,            // authB64 (опционально)
 *                                json, jsonLen);
 *       tls.close();
 *   }
 *   @endcode
 *
 * Верификация сертификата:
 *   По умолчанию VERIFY_NONE (не проверять CA) — подходит для IoT
 *   с закрытым периметром или при ограниченной Flash.
 *   Для VERIFY_REQUIRED передайте PEM-строку через setCaPem().
 * ================================================================
 */
#pragma once

#include "sim7020c.hpp"
#include "config.hpp"

extern "C" {
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
}

#include <cstdint>

// ================================================================
// Контекст одного TLS-соединения через SIM7020C
// ================================================================
class Sim7020cTls {
public:
    /**
     * @param modem      Уже инициализированный объект SIM7020C
     *                   (init() + activatePdn() уже выполнены).
     * @param timeoutMs  Таймаут всего TLS-хендшейка + передачи (мс).
     */
    explicit Sim7020cTls(SIM7020C& modem, uint32_t timeoutMs = 30000);

    ~Sim7020cTls() { close(); }

    // Не копируем — TLS-контекст не переносим
    Sim7020cTls(const Sim7020cTls&)            = delete;
    Sim7020cTls& operator=(const Sim7020cTls&) = delete;

    // ---- CA cert для верификации (опционально) ----

    /**
     * Установить CA-сертификат в PEM-формате для верификации сервера.
     * Если не задан — используется MBEDTLS_SSL_VERIFY_NONE.
     * @param caPem  NULL-terminated PEM-строка (должна жить всё время TLS-сессии).
     */
    void setCaPem(const char* caPem) { m_caPem = caPem; }

    // ---- Жизненный цикл соединения ----

    /**
     * Открыть TCP-сокет и выполнить TLS-хендшейк.
     * @param host  Имя хоста или IP-адрес
     * @param port  Порт (443 для HTTPS)
     * @return 0 при успехе, отрицательное число при ошибке.
     */
    int connect(const char* host, uint16_t port = 443);

    /**
     * Отправить данные через TLS (сырые байты — не HTTP).
     * Используется внутри httpsPost().
     */
    int write(const uint8_t* buf, size_t len);

    /**
     * Прочитать данные через TLS.
     * Используется внутри httpsPost().
     */
    int read(uint8_t* buf, size_t len);

    /**
     * HTTP POST через уже открытое TLS-соединение.
     * Формирует и отправляет HTTP/1.1 запрос, возвращает HTTP-код.
     *
     * @param path      URL-путь (например "/api/v1/TOKEN/telemetry")
     * @param host      Имя хоста для заголовка Host:
     * @param authB64   Base64 для Authorization: Basic (nullptr = не нужен)
     * @param json      Тело запроса
     * @param jsonLen   Длина тела
     * @return HTTP-код (200, 400…), 0 при ошибке связи.
     */
    int httpsPost(const char* path,
                  const char* host,
                  const char* authB64,
                  const char* json,
                  uint16_t    jsonLen);

    /**
     * Полный цикл: TCP connect + TLS handshake + HTTP POST + close.
     * Удобный однострочный метод для postViaGsm().
     *
     * @param url      Полный HTTPS URL ("https://host/path")
     * @param authB64  Base64 или nullptr
     * @param json     JSON-тело
     * @param jsonLen  Длина тела
     * @return HTTP-код (200, 400…), или < 0 при ошибке.
     */
    int postJson(const char* url,
                 const char* authB64,
                 const char* json,
                 uint16_t    jsonLen);

    /** Закрыть TLS-соединение и TCP-сокет. Безопасно вызывать повторно. */
    void close();

    /** True если TLS-соединение установлено. */
    bool connected() const { return m_connected; }

private:
    SIM7020C&  m_modem;
    uint32_t   m_timeoutMs;
    const char* m_caPem = nullptr;

    bool       m_connected = false;
    uint8_t    m_sockId    = 0;
    uint32_t   m_deadline  = 0;

    // mbedTLS objects (только при активном соединении)
    mbedtls_ssl_context     m_ssl;
    mbedtls_ssl_config      m_conf;
    mbedtls_entropy_context m_entropy;
    mbedtls_ctr_drbg_context m_ctr;
    mbedtls_x509_crt        m_cacert;

    bool m_tlsInited = false;

    // ---- Внутренние методы ----
    void tlsFree();
    void logMbedtlsErr(const char* tag, int rc);

    // mbedTLS BIO callbacks
    static int biosend(void* ctx, const unsigned char* buf, size_t len);
    static int biorecv(void* ctx, unsigned char* buf, size_t len);

    // Приём одного байта от модема (polling)
    int  modemReadByte(uint32_t timeoutMs);

    // Отправка сырых байт через AT+CSODSEND
    int  modemWriteRaw(const uint8_t* buf, uint16_t len);

    // Приём N байт через AT+CSORCV
    int  modemReadRaw(uint8_t* buf, uint16_t len);

    // Разбор HTTPS URL
    struct UrlParts {
        char     host[64]{};
        char     path[128]{};
        uint16_t port = 443;
    };
    static bool parseHttpsUrl(const char* url, UrlParts& out);
};
