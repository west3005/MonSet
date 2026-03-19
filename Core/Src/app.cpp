/**
 * ================================================================
 * @file    app.cpp
 * @brief   Реализация главного класса приложения.
 * ================================================================
 */
#include "app.hpp"
#include "w5500_net.hpp"
#include "https_w5500.hpp"
#include "sim7020c_tls.hpp"
#include "runtime_config.hpp"
#include "cfg_uart_bridge.hpp"
#include "board_pins.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
    extern I2C_HandleTypeDef  hi2c1;
    extern UART_HandleTypeDef huart2;
    extern UART_HandleTypeDef huart3;
    extern SPI_HandleTypeDef  hspi1;
    extern RTC_HandleTypeDef  hrtc;
    extern bool g_sd_disabled;

    #include "socket.h"
    #include "dns.h"
    #include "w5500.h"
    #include "wizchip_conf.h"
}

// ============================================================================
// Singleton: Ethernet-контроллер
// ============================================================================
static W5500Net eth;

// ============================================================================
// Вспомогательные функции
// ============================================================================
static bool startsWith(const char* s, const char* prefix)
{
    if (!s || !prefix) return false;
    return std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

// ============================================================================
// Время (DateTime -> Unix ms)
// ============================================================================
static bool isLeapYear(int y)
{
    return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
}

static uint64_t toUnixMs(const DateTime& dt)
{
    int y = 2000 + (int)dt.year;
    int m = (int)dt.month;

    static const uint16_t cumDays[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t days = 0;
    for (int yy = 1970; yy < y; yy++) days += isLeapYear(yy) ? 366 : 365;
    days += cumDays[m - 1];
    if (m > 2 && isLeapYear(y)) days++;
    days += (uint32_t)(dt.date - 1);

    uint64_t sec = (uint64_t)days * 86400ULL
                 + (uint64_t)dt.hours   * 3600ULL
                 + (uint64_t)dt.minutes * 60ULL
                 + (uint64_t)dt.seconds;
    return sec * 1000ULL;
}

static void u64ToDec(char* out, size_t outSz, uint64_t v)
{
    if (!out || outSz == 0) return;
    char tmp[32];
    size_t n = 0;
    do { tmp[n++] = char('0' + (v % 10)); v /= 10; } while (v && n < sizeof(tmp));
    size_t pos = 0;
    while (n && (pos + 1) < outSz) out[pos++] = tmp[--n];
    out[pos] = '\0';
}

// ============================================================================
// NTP helpers (используют W5500 socket 2, UDP)
// ============================================================================
static constexpr uint16_t NTP_PORT       = 123;
static constexpr uint32_t NTP_TIMEOUT_MS = 3000;
static constexpr uint32_t BKP_MAGIC      = 0x4E545031;
static constexpr uint32_t BKP_MAGIC_REG  = RTC_BKP_DR0;
static constexpr uint32_t BKP_SYNC_REG   = RTC_BKP_DR1;

static uint32_t bkpRead(uint32_t reg)
{
    return HAL_RTCEx_BKUPRead(&hrtc, reg);
}

static void bkpWrite(uint32_t reg, uint32_t v)
{
    HAL_RTCEx_BKUPWrite(&hrtc, reg, v);
}

static uint32_t loadLastSyncUnix()
{
    return (bkpRead(BKP_MAGIC_REG) == BKP_MAGIC) ? bkpRead(BKP_SYNC_REG) : 0;
}

static void storeLastSyncUnix(uint32_t unixSec)
{
    bkpWrite(BKP_MAGIC_REG, BKP_MAGIC);
    bkpWrite(BKP_SYNC_REG, unixSec);
}

static bool rtcIsInvalid(const DateTime& dt)
{
    if (dt.year < 24 || dt.year > 60)   return true;
    if (dt.month < 1 || dt.month > 12)  return true;
    if (dt.date  < 1 || dt.date  > 31)  return true;
    if (dt.hours > 23 || dt.minutes > 59 || dt.seconds > 59) return true;
    return false;
}

static void unixToDateTime(uint32_t unixSec, DateTime& out)
{
    uint32_t sec = unixSec;
    out.seconds = (uint8_t)(sec % 60); sec /= 60;
    out.minutes = (uint8_t)(sec % 60); sec /= 60;
    out.hours   = (uint8_t)(sec % 24); sec /= 24;

    uint32_t days = sec;
    int y = 1970;
    while (true) {
        uint32_t diy = isLeapYear(y) ? 366u : 365u;
        if (days < diy) break;
        days -= diy;
        y++;
    }

    const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t mo = 0;
    while (mo < 12) {
        uint8_t md = mdays[mo];
        if (mo == 1 && isLeapYear(y)) md = 29;
        if (days < md) break;
        days -= md;
        mo++;
    }

    out.year  = (uint8_t)(y - 2000);
    out.month = (uint8_t)(mo + 1);
    out.date  = (uint8_t)(days + 1);
}

static bool sntpGetUnixTime(const char* host, uint32_t& unixSec)
{
    uint8_t ip[4]{};

    // Определить IP: числовой или через DNS
    bool isNumeric = true;
    for (const char* p = host; *p; p++) {
        if (!std::isdigit((unsigned char)*p) && *p != '.') { isNumeric = false; break; }
    }

    if (isNumeric) {
        uint32_t a,b,c,d;
        if (std::sscanf(host, "%lu.%lu.%lu.%lu", &a,&b,&c,&d) != 4) return false;
        ip[0]=(uint8_t)a; ip[1]=(uint8_t)b; ip[2]=(uint8_t)c; ip[3]=(uint8_t)d;
    } else {
        wiz_NetInfo ni{};
        wizchip_getnetinfo(&ni);
        static uint8_t dnsBuf[Config::DNS_BUFFER_SIZE];
        DNS_init(1, dnsBuf);
        DBG.info("NTP: DNS resolve %s...", host);
        bool resolved = false;
        const uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < Config::DNS_TIMEOUT_MS) {
            int8_t r = DNS_run(ni.dns, (uint8_t*)host, ip);
            if (r == 1) { resolved = true; break; }
            if (r < 0)  { DBG.error("NTP: DNS error r=%d", (int)r); return false; }
            HAL_Delay(50);
            IWDG->KR = 0xAAAA;
        }
        if (!resolved) { DBG.error("NTP: DNS timeout"); return false; }
    }

    DBG.info("NTP: %s -> %u.%u.%u.%u", host, ip[0], ip[1], ip[2], ip[3]);

    for (int attempt = 1; attempt <= 3; attempt++) {
        const uint8_t sn    = 2;
        const uint16_t lport = (uint16_t)(40000 + attempt);

        uint8_t pkt[48]{};
        pkt[0] = 0x1B;  // LI=0, VN=3, Mode=3 (client)

        if (socket(sn, Sn_MR_UDP, lport, 0) != sn) {
            close(sn); continue;
        }

        if (sendto(sn, pkt, sizeof(pkt), ip, NTP_PORT) != (int32_t)sizeof(pkt)) {
            close(sn); continue;
        }

        uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < NTP_TIMEOUT_MS) {
            uint8_t rx[48];
            uint8_t rip[4];
            uint16_t rport = 0;
            int32_t r = recvfrom(sn, rx, sizeof(rx), rip, &rport);
            if (r >= 48) {
                close(sn);
                uint32_t ntpSec = ((uint32_t)rx[40] << 24) | ((uint32_t)rx[41] << 16)
                                | ((uint32_t)rx[42] <<  8) | ((uint32_t)rx[43]);
                constexpr uint32_t NTP2UNIX = 2208988800UL;
                if (ntpSec < NTP2UNIX) break;
                unixSec = ntpSec - NTP2UNIX;
                DBG.info("NTP: sync OK unix=%lu (попытка %d)", (unsigned long)unixSec, attempt);
                return true;
            }
            HAL_Delay(10);
            IWDG->KR = 0xAAAA;
        }
        close(sn);
        DBG.warn("NTP: timeout (попытка %d/3)", attempt);
    }

    DBG.error("NTP: все попытки исчерпаны");
    return false;
}

// ============================================================================
// Конструктор
// ============================================================================
App::App()
    : m_rtc(&hi2c1)
    , m_modbus(&huart3, PIN_RS485_DE_PORT, PIN_RS485_DE_PIN)
    , m_gsm(&huart2, PIN_SIM_PWR_PORT, PIN_SIM_PWR_PIN)
    , m_sdBackup()
    , m_sensor(m_modbus, m_rtc)
    , m_buffer()
    , m_power(&hrtc, m_sdBackup)
{}

// ============================================================================
// Состояние / LED
// ============================================================================
SystemMode App::readMode()
{
    auto pin = HAL_GPIO_ReadPin(PIN_MODE_SW_PORT, PIN_MODE_SW_PIN);
    return (pin == GPIO_PIN_SET) ? SystemMode::Debug : SystemMode::Sleep;
}

LinkChannel App::readChannel()
{
    GPIO_PinState pin = HAL_GPIO_ReadPin(PIN_NET_SW_PORT, PIN_NET_SW_PIN);
    return (pin == GPIO_PIN_RESET) ? LinkChannel::Eth : LinkChannel::Gsm;
}

void App::ledOn()    { HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_SET);   }
void App::ledOff()   { HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_RESET); }
void App::ledBlink(uint8_t count, uint32_t ms)
{
    for (uint8_t i = 0; i < count; i++) {
        ledOn();  HAL_Delay(ms);
        ledOff(); HAL_Delay(ms);
    }
}

// ============================================================================
// ETH вспомогательные
// ============================================================================
bool App::ensureEthReady()
{
    if (!eth.ready()) {
        DBG.info("ETH: init...");
        if (!eth.init(&hspi1, Config::W5500_DHCP_TIMEOUT_MS)) {
            DBG.error("ETH: init failed");
            return false;
        }
    }

    uint8_t link = 0;
    for (int i = 0; i < 50; i++) {
        if (ctlwizchip(CW_GET_PHYLINK, (void*)&link) != 0) {
            DBG.error("ETH: CW_GET_PHYLINK error");
            return false;
        }
        if (link != PHY_LINK_OFF) {
            DBG.info("ETH: link UP (попытка %d)", i + 1);
            return true;
        }
        HAL_Delay(100);
        IWDG->KR = 0xAAAA;
    }

    DBG.error("ETH: нет физического линка");
    return false;
}

// ============================================================================
// HTTP POST через W5500 (plain socket)
// ============================================================================
static int httpPostPlainW5500(const char* url,
                               const char* authB64,
                               const char* json,
                               uint16_t    len,
                               uint32_t    timeoutMs)
{
    // ---- Разбор URL ----
    struct UrlParts { char host[64]{}; char path[128]{}; uint16_t port = 80; };
    UrlParts u{};
    {
        const char* prefix = "http://";
        if (std::strncmp(url, prefix, std::strlen(prefix)) != 0) return -10;
        const char* p = url + std::strlen(prefix);
        const char* hb = p;
        while (*p && *p != '/' && *p != ':') p++;
        size_t hl = (size_t)(p - hb);
        if (hl == 0 || hl >= sizeof(u.host)) return -10;
        std::memcpy(u.host, hb, hl);
        if (*p == ':') {
            p++;
            uint32_t port = 0;
            while (*p && std::isdigit((unsigned char)*p)) port = port * 10 + (*p++ - '0');
            u.port = (uint16_t)port;
        }
        if (*p == '\0') { std::strcpy(u.path, "/"); }
        else if (*p == '/') { std::strcpy(u.path, p); }
        else return -10;
    }

    // ---- DNS ----
    uint8_t dstIp[4]{};
    {
        bool isNum = true;
        for (const char* p = u.host; *p; p++) {
            if (!std::isdigit((unsigned char)*p) && *p != '.') { isNum = false; break; }
        }
        if (isNum) {
            uint32_t a,b,c,d;
            std::sscanf(u.host, "%lu.%lu.%lu.%lu", &a,&b,&c,&d);
            dstIp[0]=(uint8_t)a; dstIp[1]=(uint8_t)b;
            dstIp[2]=(uint8_t)c; dstIp[3]=(uint8_t)d;
        } else {
            wiz_NetInfo ni{};
            wizchip_getnetinfo(&ni);
            static uint8_t dnsBuf[Config::DNS_BUFFER_SIZE];
            DNS_init(1, dnsBuf);
            uint8_t resolved[4]{};
            bool ok = false;
            uint32_t t0 = HAL_GetTick();
            while ((HAL_GetTick() - t0) < Config::DNS_TIMEOUT_MS) {
                int8_t r = DNS_run(ni.dns, (uint8_t*)u.host, resolved);
                if (r == 1) { std::memcpy(dstIp, resolved, 4); ok = true; break; }
                if (r < 0)  break;
                HAL_Delay(50); IWDG->KR = 0xAAAA;
            }
            if (!ok) { DBG.error("ETH HTTP: DNS failed"); return -11; }
        }
    }

    // ---- TCP socket ----
    const uint8_t sn = 0;
    if (socket(sn, Sn_MR_TCP, Config::HTTP_LOCAL_PORT, 0) != sn) { close(sn); return -20; }
    if (connect(sn, dstIp, u.port) != SOCK_OK)                   { close(sn); return -21; }

    // ---- Заголовок ----
    char hdr[600];
    int hdrLen;
    if (authB64 && authB64[0]) {
        hdrLen = std::snprintf(hdr, sizeof(hdr),
            "POST %s HTTP/1.1\r\nHost: %s\r\n"
            "Authorization: Basic %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\nConnection: close\r\n\r\n",
            u.path, u.host, authB64, (unsigned)len);
    } else {
        hdrLen = std::snprintf(hdr, sizeof(hdr),
            "POST %s HTTP/1.1\r\nHost: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\nConnection: close\r\n\r\n",
            u.path, u.host, (unsigned)len);
    }
    if (hdrLen <= 0 || (size_t)hdrLen >= sizeof(hdr)) { close(sn); return -22; }

    auto sendAll = [&](const uint8_t* p, uint32_t n) -> bool {
        uint32_t off = 0;
        while (off < n) {
            int32_t r = send(sn, (uint8_t*)p + off, (uint16_t)(n - off));
            if (r <= 0) return false;
            off += (uint32_t)r;
        }
        return true;
    };

    if (!sendAll((const uint8_t*)hdr, (uint32_t)hdrLen)) { close(sn); return -23; }
    if (!sendAll((const uint8_t*)json, (uint32_t)len))   { close(sn); return -24; }

    // ---- Ответ ----
    static char rx[768];
    int rxUsed = 0;
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeoutMs) {
        int32_t rlen = recv(sn, (uint8_t*)rx + rxUsed,
                            (uint16_t)(sizeof(rx) - 1 - rxUsed));
        if (rlen > 0) {
            rxUsed += (int)rlen;
            rx[rxUsed] = '\0';
            const char* p = std::strstr(rx, "HTTP/1.");
            if (p) {
                int code = 0;
                if (std::sscanf(p, "HTTP/%*s %d", &code) == 1) {
                    disconnect(sn); close(sn);
                    return code;
                }
            }
        } else {
            HAL_Delay(2);
            IWDG->KR = 0xAAAA;
        }
    }

    disconnect(sn); close(sn);
    return -30;
}

// ============================================================================
// Методы отправки (обёртки для единого API)
// ============================================================================
int App::postViaEth(const char* json, uint16_t len)
{
    if (!ensureEthReady()) return -1;

    if (startsWith(Cfg().server_url, "https://")) {
        return HttpsW5500::postJson(Cfg().server_url, Cfg().server_auth_b64,
                                    json, len, Config::HTTPS_POST_TIMEOUT_MS);
    }
    if (startsWith(Cfg().server_url, "http://")) {
        return httpPostPlainW5500(Cfg().server_url, Cfg().server_auth_b64,
                                  json, len, Config::HTTP_POST_TIMEOUT_MS);
    }
    DBG.error("ETH: неизвестная схема URL");
    return -1;
}

int App::postViaGsm(const char* json, uint16_t len)
{
    m_gsm.powerOn();
    int code = -1;

    if (m_gsm.init() != GsmStatus::Ok) {
        DBG.error("GSM: init failed — отправка отменена");
        m_gsm.powerOff();
        return -1;
    }

    const char* url = Cfg().server_url;

    if (startsWith(url, "https://")) {
        // Транспорт: mbedTLS поверх SIM7020C TCP socket
        Sim7020cTls tls(m_gsm, Config::SIM7020_TLS_TIMEOUT_MS);
        code = tls.postJson(url, Cfg().server_auth_b64, json, len);
        DBG.info("GSM HTTPS: code=%d", code);
    } else if (startsWith(url, "http://")) {
        // Plain HTTP (TCP socket, без TLS)
        code = (int)m_gsm.httpPost(url, json, len);
        DBG.info("GSM HTTP: code=%d", code);
    } else {
        DBG.error("GSM: неизвестная схема URL");
    }

    m_gsm.disconnect();
    m_gsm.powerOff();
    return code;
}

// ============================================================================
// Построитель JSON-payload
// ============================================================================
int App::buildPayload(char*       buf,
                       size_t      bsz,
                       const char* tsStr,
                       float       val,
                       const DateTime& dt,
                       bool        asArray)
{
    return std::snprintf(
        buf, bsz,
        "%s{\"ts\":%s,\"values\":{\"metricId\":\"%s\",\"value\":%.3f,"
        "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}}%s",
        asArray ? "[" : "",
        tsStr,
        Cfg().metric_id,
        val,
        (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.date,
        (unsigned)dt.hours, (unsigned)dt.minutes, (unsigned)dt.seconds,
        asArray ? "]" : "");
}

// ============================================================================
// Инициализация
// ============================================================================
void App::init()
{
    DBG.info("=== APP INIT %s %s ===", __DATE__, __TIME__);

    DBG.info("[1/5] RTC init");
    m_rtc.init();

    DBG.info("[2/5] Modbus init");
    m_modbus.init();

    DBG.info("[3/5] SD init");
    bool sdOk = false;
    if (!g_sd_disabled) {
        sdOk = m_sdBackup.init();
        DBG.info("[3/5] SD init: %s", sdOk ? "OK" : "FAIL");
    } else {
        DBG.warn("[3/5] SD пропущена (флаг watchdog-reset)");
    }

    DBG.info("[4/5] Load runtime config");
    bool cfgLoaded = false;
    if (!g_sd_disabled && sdOk) {
        cfgLoaded = Cfg().loadFromSd(RUNTIME_CONFIG_FILENAME);
    }
    if (!cfgLoaded) {
        DBG.warn("CFG: нет файла — используются defaults");
        Cfg().setDefaultsFromConfig();
    }
    Cfg().log();

    DBG.info("[5/5] Modem power OFF (cold start)");
    m_gsm.powerOff();

    m_mode = readMode();
    LinkChannel ch = readChannel();
    DBG.info("Mode: %s | Channel: %s",
             m_mode == SystemMode::Debug ? "DEBUG" : "SLEEP",
             ch    == LinkChannel::Eth   ? "ETH(W5500)" : "GSM(SIM7020C)");
    DBG.info("Poll interval: %lu s | Send every: %lu polls",
             (unsigned long)Cfg().poll_interval_sec,
             (unsigned long)Cfg().send_interval_polls);

    if (m_mode == SystemMode::Debug) {
        ledOn();
    } else {
        ledBlink(3, 200);
    }
}

// ============================================================================
// NTP-синхронизация RTC
// ============================================================================
bool App::syncRtcWithNtpIfNeeded(const char* tag, bool verbose)
{
    const RuntimeConfig& c = Cfg();
    if (!c.ntp_enabled) {
        if (verbose) DBG.info("[%s] NTP: отключён в конфиге", tag);
        return false;
    }

    DateTime cur{};
    if (!m_rtc.getTime(cur)) {
        DBG.error("[%s] NTP: DS3231 read failed", tag);
        return false;
    }

    const bool invalid    = rtcIsInvalid(cur);
    const uint32_t lastSy = loadLastSyncUnix();
    const uint32_t nowSec = (uint32_t)(toUnixMs(cur) / 1000ULL);

    bool needSync  = false;
    const char* reason = "";

    if (invalid) {
        needSync = true; reason = "RTC invalid";
    } else if (lastSy == 0) {
        needSync = true; reason = "first sync";
    } else {
        uint32_t diff = nowSec - lastSy;
        if (diff >= c.ntp_resync_sec) {
            needSync = true; reason = "periodic";
        } else {
            if (verbose) DBG.info("[%s] NTP: skip (diff=%lu s)", tag, (unsigned long)diff);
            return false;
        }
    }

    DBG.info("[%s] NTP: синхронизация (%s)", tag, reason);

    if (readChannel() != LinkChannel::Eth) {
        DBG.warn("[%s] NTP: только по ETH, канал GSM — пропуск", tag);
        return false;
    }
    if (!ensureEthReady()) {
        DBG.error("[%s] NTP: ETH недоступен", tag);
        return false;
    }

    uint32_t unixSec = 0;
    if (!sntpGetUnixTime(c.ntp_host, unixSec)) {
        DBG.error("[%s] NTP: failed", tag);
        return false;
    }

    DateTime ntpDt{};
    unixToDateTime(unixSec, ntpDt);

    char ntpStr[32]{};
    ntpDt.formatISO8601(ntpStr);
    DBG.info("[%s] NTP: set RTC -> %s", tag, ntpStr);

    if (!m_rtc.setTimeAutoDOW(ntpDt)) {
        DBG.error("[%s] NTP: DS3231 write failed", tag);
        return false;
    }

    storeLastSyncUnix(unixSec);
    return true;
}

// ============================================================================
// Главный цикл
// ============================================================================
[[noreturn]] void App::run()
{
    bool wokeFromStop = false;
    bool firstCycle   = true;

    while (true) {
        CfgUartBridge_Tick();
        if (eth.ready()) eth.tick();

        m_mode = readMode();

        const bool isWake = firstCycle || wokeFromStop;
        const char* tag   = firstCycle ? "BOOT" : (wokeFromStop ? "WAKE" : "RUN");

        // Синхронизация NTP (только при wakeup/boot)
        if (isWake) {
            syncRtcWithNtpIfNeeded(tag, true);
        }

        // Опрос датчика
        DateTime ts{};
        float val = m_sensor.read(ts);

        const uint64_t unixMs = toUnixMs(ts);
        char tsStr[24];
        u64ToDec(tsStr, sizeof(tsStr), unixMs);

        char timeStr[32]{};
        ts.formatISO8601(timeStr);
        DBG.data("[%s] val=%.3f t=%s", tag, val, timeStr);

        ledBlink(1, 50);

        // Запись в журнал SD
        if (!g_sd_disabled) {
            char line[Config::JSONL_LINE_MAX];
            int l = buildPayload(line, sizeof(line), tsStr, val, ts, false);
            if (l > 0 && l < (int)sizeof(line)) {
                if (!m_sdBackup.appendLine(line)) DBG.error("SD: appendLine failed");
            }
        }

        // Периодическая отправка журнала
        m_pollCounter++;
        if (m_pollCounter >= Cfg().send_interval_polls) {
            m_pollCounter = 0;
            if (g_sd_disabled) {
                transmitSingle(val, ts);
            } else {
                transmitBuffer();
            }
        }

        // Ожидание до следующего опроса
        const uint32_t pollSec = Cfg().poll_interval_sec;
        if (m_mode == SystemMode::Sleep) {
            DBG.info("STOP %lu s...", (unsigned long)pollSec);
            ledOff();
            m_power.enterStopMode(pollSec);
            DBG.info("...wake");
            wokeFromStop = true;
        } else {
            CfgUartBridge_DelayMs(pollSec * 1000UL);
            wokeFromStop = false;
        }

        firstCycle = false;
        IWDG->KR = 0xAAAA;
    }
}

// ============================================================================
// Передача: одиночное измерение (без SD)
// ============================================================================
void App::transmitSingle(float value, const DateTime& dt)
{
    char tsStr[24];
    u64ToDec(tsStr, sizeof(tsStr), toUnixMs(dt));

    char j[Config::JSON_BUFFER_SIZE];
    int len = buildPayload(j, sizeof(j), tsStr, value, dt, true);
    if (len <= 0 || len >= (int)sizeof(j)) {
        DBG.error("transmitSingle: JSON build failed");
        return;
    }

    int code = (readChannel() == LinkChannel::Eth)
               ? postViaEth(j, (uint16_t)len)
               : postViaGsm(j, (uint16_t)len);

    DBG.info("transmitSingle: HTTP %d", code);
}

// ============================================================================
// Передача: журнал из SD (batch)
// ============================================================================
void App::transmitBuffer()
{
    if (g_sd_disabled) {
        DBG.warn("SD отключена -> передача журнала невозможна");
        return;
    }

    LinkChannel ch = readChannel();
    DBG.info("=== ОТПРАВКА ЖУРНАЛА (%s) ===",
             ch == LinkChannel::Eth ? "ETH" : "GSM(SIM7020C)");

    bool gsmOn = false;
    if (ch == LinkChannel::Gsm) {
        m_gsm.powerOn();
        if (m_gsm.init() != GsmStatus::Ok) {
            DBG.error("GSM init failed — журнал сохранён");
            m_gsm.powerOff();
            return;
        }
        gsmOn = true;
    }

    retransmitBackup();

    if (gsmOn) {
        m_gsm.disconnect();
        m_gsm.powerOff();
    }

    DBG.info("=== ОТПРАВКА ЖУРНАЛА КОНЕЦ ===");
}

// ============================================================================
// Повторная отправка накопленного журнала чанками
// ============================================================================
void App::retransmitBackup()
{
    while (m_sdBackup.exists()) {
        uint32_t lines = 0;
        FSIZE_t  used  = 0;

        const uint32_t maxPayload = (Config::HTTP_CHUNK_MAX < Config::JSON_BUFFER_SIZE)
                                    ? Config::HTTP_CHUNK_MAX
                                    : (Config::JSON_BUFFER_SIZE - 1);

        bool ok = m_sdBackup.readChunkAsJsonArray(
                      m_json, sizeof(m_json), maxPayload, lines, used);
        if (!ok || lines == 0 || used == 0) {
            DBG.error("SD: readChunk failed (ok=%d lines=%u)", ok ? 1 : 0, (unsigned)lines);
            return;
        }

        DBG.info("Chunk: lines=%u bytes=%lu payloadLen=%u",
                 (unsigned)lines, (unsigned long)used,
                 (unsigned)std::strlen(m_json));

        int http = (readChannel() == LinkChannel::Eth)
                   ? postViaEth(m_json, (uint16_t)std::strlen(m_json))
                   : (int)m_gsm.httpPost(Cfg().server_url, m_json,
                                          (uint16_t)std::strlen(m_json));

        if (http == 200) {
            if (!m_sdBackup.consumePrefix(used)) {
                DBG.error("SD: consumePrefix failed");
                return;
            }
        } else {
            DBG.error("HTTP %d — журнал сохранён", http);
            return;
        }
    }

    DBG.info("Журнал полностью передан");
}
