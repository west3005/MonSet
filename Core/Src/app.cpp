/**
 * ================================================================
 * @file app.cpp
 * @brief Реализация главного класса приложения.
 * ================================================================
 */
#include "app.hpp"
#include "w5500_net.hpp"
#include "https_w5500.hpp"
#include "runtime_config.hpp"

#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdint>

extern "C" {
  extern I2C_HandleTypeDef  hi2c1;
  extern UART_HandleTypeDef huart2;
  extern UART_HandleTypeDef huart3;
  extern SPI_HandleTypeDef  hspi1;
  extern RTC_HandleTypeDef  hrtc;

#include "socket.h"
#include "dns.h"
#include "w5500.h"
#include "wizchip_conf.h"
}

static W5500Net eth;

enum class LinkChannel : uint8_t { Gsm = 0, Eth = 1 };

static LinkChannel readChannel()
{
  GPIO_PinState pin = HAL_GPIO_ReadPin(PIN_NET_SW_PORT, PIN_NET_SW_PIN);
  return (pin == GPIO_PIN_RESET) ? LinkChannel::Eth : LinkChannel::Gsm;
}

static const char* chStr(LinkChannel ch)
{
  return (ch == LinkChannel::Eth) ? "ETH(W5500)" : "GSM(SIM800L)";
}

static void logNetSelect(const char* tag)
{
  GPIO_PinState pin = HAL_GPIO_ReadPin(PIN_NET_SW_PORT, PIN_NET_SW_PIN);
  LinkChannel ch = readChannel();
  DBG.info("[%s] NET_SELECT PB0=%d => %s", tag, (int)pin, chStr(ch));
}

static bool startsWith(const char* s, const char* prefix)
{
  if (!s || !prefix) return false;
  size_t n = std::strlen(prefix);
  return (std::strncmp(s, prefix, n) == 0);
}

// ============================================================================
// ETH
// ============================================================================
static bool ethLinkUpAfterInit()
{
  uint8_t link = 0;
  ctlwizchip(CW_GET_PHYLINK, (void*)&link);
  return (link != 0);
}

static bool ensureEthReadyAndLinkUp()
{
  if (!eth.ready()) {
    DBG.info("ETH: init...");
    if (!eth.init(&hspi1, Config::W5500_DHCP_TIMEOUT_MS)) {
      DBG.error("ETH: init failed");
      return false;
    }
  }

  if (!ethLinkUpAfterInit()) {
    DBG.error("ETH: link DOWN");
    return false;
  }

  return true;
}

// ============================================================================
// HTTP plain W5500
// ============================================================================
struct UrlParts {
  char host[64]{};
  char path[128]{};
  uint16_t port = 80;
};

static bool isIpv4Literal(const char* s)
{
  if (!s || !*s) return false;
  for (const char* p = s; *p; ++p) {
    if (!std::isdigit((unsigned char)*p) && *p != '.') return false;
  }
  return true;
}

static bool parseHttpUrl(const char* url, UrlParts& out)
{
  if (!url) return false;

  out = UrlParts{};
  out.port = 80;

  const char* p = url;
  const char* prefix = "http://";
  if (std::strncmp(p, prefix, std::strlen(prefix)) != 0) return false;
  p += std::strlen(prefix);

  const char* hostBeg = p;
  while (*p && *p != '/' && *p != ':') p++;
  size_t hostLen = (size_t)(p - hostBeg);
  if (hostLen == 0 || hostLen >= sizeof(out.host)) return false;
  std::memcpy(out.host, hostBeg, hostLen);
  out.host[hostLen] = 0;

  if (*p == ':') {
    p++;
    uint32_t port = 0;
    while (*p && std::isdigit((unsigned char)*p)) {
      port = port * 10u + (uint32_t)(*p - '0');
      p++;
    }
    if (port == 0 || port > 65535) return false;
    out.port = (uint16_t)port;
  }

  if (*p == 0) { std::strcpy(out.path, "/"); return true; }
  if (*p != '/') return false;

  size_t pathLen = std::strlen(p);
  if (pathLen == 0 || pathLen >= sizeof(out.path)) return false;
  std::memcpy(out.path, p, pathLen + 1);
  return true;
}

static bool resolveHost(const char* host, uint8_t outIp[4])
{
  if (isIpv4Literal(host)) {
    uint32_t a=0,b=0,c=0,d=0;
    if (std::sscanf(host, "%lu.%lu.%lu.%lu", &a,&b,&c,&d) != 4) return false;
    if (a>255||b>255||c>255||d>255) return false;
    outIp[0]=(uint8_t)a; outIp[1]=(uint8_t)b; outIp[2]=(uint8_t)c; outIp[3]=(uint8_t)d;
    return true;
  }

  wiz_NetInfo ni{};
  wizchip_getnetinfo(&ni);

  static uint8_t dnsBuf[512];
  DNS_init(1, dnsBuf);

  uint8_t resolved[4]{};
  int8_t r = DNS_run(ni.dns, (uint8_t*)host, resolved);
  if (r != 1) return false;

  std::memcpy(outIp, resolved, 4);
  return true;
}

static int httpPostPlainW5500(const char* url,
                             const char* authBasicB64,
                             const char* json,
                             uint16_t len,
                             uint32_t timeoutMs)
{
  UrlParts u{};
  if (!parseHttpUrl(url, u)) return -10;

  uint8_t dstIp[4]{};
  if (!resolveHost(u.host, dstIp)) return -11;

  const uint8_t  sn = 0;
  const uint16_t localPort = 50000;

  int8_t s = socket(sn, Sn_MR_TCP, localPort, 0);
  if (s != sn) { close(sn); return -20; }

  int8_t c = connect(sn, dstIp, u.port);
  if (c != SOCK_OK) { close(sn); return -21; }

  char hdr[600];
  int hdrLen = 0;

  if (authBasicB64 && authBasicB64[0]) {
    hdrLen = std::snprintf(
      hdr, sizeof(hdr),
      "POST %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Authorization: Basic %s\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "\r\n",
      u.path, u.host, authBasicB64, (unsigned)len
    );
  } else {
    hdrLen = std::snprintf(
      hdr, sizeof(hdr),
      "POST %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "\r\n",
      u.path, u.host, (unsigned)len
    );
  }

  if (hdrLen <= 0 || (size_t)hdrLen >= sizeof(hdr)) { close(sn); return -22; }

  auto sendAll = [&](const uint8_t* p, uint32_t n) -> int {
    uint32_t off = 0;
    while (off < n) {
      int32_t r = send(sn, (uint8_t*)p + off, (uint16_t)(n - off));
      if (r <= 0) return -1;
      off += (uint32_t)r;
    }
    return 0;
  };

  if (sendAll((const uint8_t*)hdr, (uint32_t)hdrLen) != 0) { close(sn); return -23; }
  if (sendAll((const uint8_t*)json, (uint32_t)len) != 0)    { close(sn); return -24; }

  uint32_t t0 = HAL_GetTick();
  static char rx[768];
  int rxUsed = 0;

  while ((HAL_GetTick() - t0) < timeoutMs) {
    int32_t rlen = recv(sn, (uint8_t*)rx + rxUsed, (uint16_t)(sizeof(rx) - 1 - rxUsed));
    if (rlen > 0) {
      rxUsed += (int)rlen;
      rx[rxUsed] = 0;

      const char* p = std::strstr(rx, "HTTP/1.1 ");
      if (!p) p = std::strstr(rx, "HTTP/1.0 ");
      if (p) {
        int code = 0;
        if (std::sscanf(p, "HTTP/%*s %d", &code) == 1) {
          disconnect(sn);
          close(sn);
          return code;
        }
      }
    } else {
      HAL_Delay(2);
    }
  }

  disconnect(sn);
  close(sn);
  return -30;
}

// ============================================================================
// Time helpers: DateTime -> unix ms (UTC, по полям DateTime)
// ============================================================================
static bool isLeap(int y) { return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0); }

static uint32_t daysBeforeMonth(int y, int m)
{
  static const uint16_t cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
  uint32_t d = cum[m - 1];
  if (m > 2 && isLeap(y)) d++;
  return d;
}

static uint64_t toUnixMs(const DateTime& dt)
{
  int y = 2000 + (int)dt.year;
  int m = (int)dt.month;
  int d = (int)dt.date;

  uint32_t days = 0;
  for (int yy = 1970; yy < y; yy++) days += isLeap(yy) ? 366 : 365;
  days += daysBeforeMonth(y, m);
  days += (uint32_t)(d - 1);

  uint64_t sec = (uint64_t)days * 86400ULL +
                 (uint64_t)dt.hours * 3600ULL +
                 (uint64_t)dt.minutes * 60ULL +
                 (uint64_t)dt.seconds;

  return sec * 1000ULL;
}

static void u64_to_dec(char* out, size_t outSz, uint64_t v)
{
  if (!out || outSz == 0) return;

  char tmp[32];
  size_t n = 0;

  do {
    tmp[n++] = char('0' + (v % 10ULL));
    v /= 10ULL;
  } while (v && n < sizeof(tmp));

  size_t pos = 0;
  while (n && (pos + 1) < outSz) out[pos++] = tmp[--n];
  out[pos] = '\0';
}

// ============================================================================
// NTP sync (host/period from RuntimeConfig)
// ============================================================================
static constexpr uint16_t NTP_PORT = 123;
static constexpr uint32_t NTP_TIMEOUT_MS = 3000;

static constexpr uint32_t BKP_MAGIC = 0x4E545031; // "NTP1"
static constexpr uint32_t BKP_MAGIC_REG = RTC_BKP_DR0;
static constexpr uint32_t BKP_LASTSYNC_REG = RTC_BKP_DR1;

static uint32_t bkpRead(uint32_t reg)  { return HAL_RTCEx_BKUPRead(&hrtc, reg); }
static void     bkpWrite(uint32_t reg, uint32_t v) { HAL_RTCEx_BKUPWrite(&hrtc, reg, v); }

static uint32_t loadLastSyncUnix()
{
  if (bkpRead(BKP_MAGIC_REG) != BKP_MAGIC) return 0;
  return bkpRead(BKP_LASTSYNC_REG);
}

static void storeLastSyncUnix(uint32_t unixSec)
{
  bkpWrite(BKP_MAGIC_REG, BKP_MAGIC);
  bkpWrite(BKP_LASTSYNC_REG, unixSec);
}

static bool rtcLooksInvalid(const DateTime& dt)
{
  if (dt.year < 24 || dt.year > 60) return true;
  if (dt.month < 1 || dt.month > 12) return true;
  if (dt.date < 1 || dt.date > 31) return true;
  if (dt.hours > 23 || dt.minutes > 59 || dt.seconds > 59) return true;
  return false;
}

static bool isLeapYearFull(int y)
{
  return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
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
    uint32_t diy = isLeapYearFull(y) ? 366u : 365u;
    if (days < diy) break;
    days -= diy;
    y++;
  }

  static const uint8_t mdays_norm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  uint8_t m = 0;
  while (m < 12) {
    uint8_t md = mdays_norm[m];
    if (m == 1 && isLeapYearFull(y)) md = 29;
    if (days < md) break;
    days -= md;
    m++;
  }

  out.year  = (uint8_t)(y - 2000);
  out.month = (uint8_t)(m + 1);
  out.date  = (uint8_t)(days + 1);
}

static bool sntpGetUnixTime(const char* host, uint32_t& unixSec)
{
  uint8_t ip[4]{};
  if (!resolveHost(host, ip)) {
    DBG.error("NTP: resolveHost(%s) failed", host);
    return false;
  }

  DBG.info("NTP: %s -> %u.%u.%u.%u", host, ip[0], ip[1], ip[2], ip[3]);

  const uint8_t sn = 2;
  const uint16_t lport = 40000;

  uint8_t pkt[48]{};
  pkt[0] = 0x1B;

  if (socket(sn, Sn_MR_UDP, lport, 0) != sn) {
    DBG.error("NTP: socket() failed");
    close(sn);
    return false;
  }

  int32_t s = sendto(sn, pkt, sizeof(pkt), ip, NTP_PORT);
  if (s != (int32_t)sizeof(pkt)) {
    DBG.error("NTP: sendto failed s=%ld", (long)s);
    close(sn);
    return false;
  }

  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < NTP_TIMEOUT_MS) {
    uint8_t rx[48];
    uint8_t rip[4];
    uint16_t rport = 0;
    int32_t r = recvfrom(sn, rx, sizeof(rx), rip, &rport);
    if (r >= 48) {
      close(sn);

      uint32_t ntpSec =
        ((uint32_t)rx[40] << 24) | ((uint32_t)rx[41] << 16) |
        ((uint32_t)rx[42] <<  8) | ((uint32_t)rx[43] <<  0);

      const uint32_t NTP_TO_UNIX = 2208988800UL;
      if (ntpSec < NTP_TO_UNIX) {
        DBG.error("NTP: bad ntpSec=%lu", (unsigned long)ntpSec);
        return false;
      }

      unixSec = ntpSec - NTP_TO_UNIX;
      return true;
    }

    HAL_Delay(10);
  }

  DBG.error("NTP: timeout");
  close(sn);
  return false;
}

// ============================================================================

App::App()
: m_rtc(&hi2c1),
  m_modbus(&huart3, PIN_RS485_DE_PORT, PIN_RS485_DE_PIN),
  m_gsm(&huart2, PIN_SIM_PWR_PORT, PIN_SIM_PWR_PIN),
  m_sdBackup(),
  m_sensor(m_modbus, m_rtc),
  m_buffer(),
  m_power(&hrtc, m_sdBackup)
{
}

SystemMode App::readMode()
{
  auto pin = HAL_GPIO_ReadPin(PIN_MODE_SW_PORT, PIN_MODE_SW_PIN);
  return (pin == GPIO_PIN_SET) ? SystemMode::Debug : SystemMode::Sleep;
}

void App::ledOn()  { HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_SET); }
void App::ledOff() { HAL_GPIO_WritePin(PIN_LED_PORT, PIN_LED_PIN, GPIO_PIN_RESET); }

void App::ledBlink(uint8_t count, uint32_t ms)
{
  for (uint8_t i = 0; i < count; i++) { ledOn(); HAL_Delay(ms); ledOff(); HAL_Delay(ms); }
}

void App::init()
{
  DBG.info("APP INIT MARK: %s %s", __DATE__, __TIME__);

  m_rtc.init();
  m_modbus.init();
  m_sdBackup.init();

  bool loaded = Cfg().loadFromSd(RUNTIME_CONFIG_FILENAME);
  if (!loaded) {
    DBG.warn("CFG: not loaded -> creating %s", RUNTIME_CONFIG_FILENAME);
    (void)Cfg().saveToSd(RUNTIME_CONFIG_FILENAME);
  }
  Cfg().log();

  // Подхват runtime config с SD
  (void)Cfg().loadFromSd(RUNTIME_CONFIG_FILENAME);
  Cfg().log();

  // Не удаляем журнал автоматически — иначе можно потерять данные
  // (void)m_sdBackup.remove();

  m_gsm.powerOff();

  m_mode = readMode();
  DBG.info("Mode: %s", (m_mode == SystemMode::Debug) ? "DEBUG" : "SLEEP");
  logNetSelect("INIT");

  DBG.info("Опрос %lu сек, отправка раз в %lu опросов",
           (unsigned long)Cfg().poll_interval_sec,
           (unsigned long)Cfg().send_interval_polls);

  if (m_mode == SystemMode::Debug) {
    DBG.info(">>> РЕЖИМ: DEBUG <<<");
    ledOn();
  } else {
    DBG.info(">>> РЕЖИМ: SLEEP <<<");
    ledBlink(3, 200);
  }
}

bool App::syncRtcWithNtpIfNeeded(const char* tag, bool verbose)
{
  const RuntimeConfig& c = Cfg();
  if (!c.ntp_enabled) {
    if (verbose) DBG.info("[%s] NTP: disabled", tag);
    return false;
  }

  DateTime cur{};
  if (!m_rtc.getTime(cur)) {
    DBG.error("[%s] NTP: DS3231 getTime failed", tag);
    return false;
  }

  const bool invalid = rtcLooksInvalid(cur);
  const uint32_t lastSync = loadLastSyncUnix();
  const uint32_t nowUnix = (uint32_t)(toUnixMs(cur) / 1000ULL);

  bool need = false;
  const char* reason = "";

  if (invalid) {
    need = true; reason = "RTC invalid";
  } else if (lastSync == 0) {
    need = true; reason = "first sync";
  } else {
    uint32_t diff = nowUnix - lastSync;
    if (diff >= c.ntp_resync_sec) {
      need = true; reason = "periodic";
    } else {
      if (verbose) DBG.info("[%s] NTP: skip (diff=%lu sec)", tag, (unsigned long)diff);
      return false;
    }
  }

  char curStr[32]{};
  cur.formatISO8601(curStr);
  DBG.info("[%s] NTP: need sync (%s), RTC=%s lastSync=%lu", tag, reason, curStr, (unsigned long)lastSync);

  if (readChannel() != LinkChannel::Eth) {
    DBG.error("[%s] NTP: skip, channel is not ETH", tag);
    return false;
  }
  if (!ensureEthReadyAndLinkUp()) {
    DBG.error("[%s] NTP: skip, ETH not ready/link down", tag);
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
  DBG.info("[%s] NTP: set DS3231 to %s (unix=%lu)", tag, ntpStr, (unsigned long)unixSec);

  if (!m_rtc.setTimeAutoDOW(ntpDt)) {
    DBG.error("[%s] NTP: DS3231 setTimeAutoDOW failed", tag);
    return false;
  }

  storeLastSyncUnix(unixSec);
  DBG.info("[%s] NTP: sync OK", tag);
  return true;
}

[[noreturn]] void App::run()
{
  bool wokeFromStop = false;
  bool firstCycle = true;

  while (true) {
    if (eth.ready()) eth.tick();

    m_mode = readMode();

    const bool doSelfTest = firstCycle || wokeFromStop;
    const char* tag = firstCycle ? "BOOT" : (wokeFromStop ? "WAKE" : "RUN");

    if (doSelfTest) {
      DBG.info("========================================================================");
      DBG.info("[%s] Self-test: NET_SELECT + Modbus + Server POST", tag);
      logNetSelect(tag);
      DBG.info("========================================================================");
    }

    // NTP
    (void)syncRtcWithNtpIfNeeded(tag, doSelfTest);

    // ---- Modbus опрос ----
    DateTime ts{};
    float val = m_sensor.read(ts);

    const uint64_t unixMs = toUnixMs(ts);
    char tsStr[24];
    u64_to_dec(tsStr, sizeof(tsStr), unixMs);

    char timeStr[32]{};
    ts.formatISO8601(timeStr);
    DBG.data("val=%.3f t=%s", val, timeStr);

    ledBlink(1, 50);

    // ---- SD журнал ----
    char line[320];
    int lenLine = std::snprintf(
      line, sizeof(line),
      "{\"ts\":%s,\"values\":{\"metricId\":\"%s\",\"value\":%.3f,"
      "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}}",
      tsStr,
      Cfg().metric_id,
      val,
      (unsigned)ts.year, (unsigned)ts.month, (unsigned)ts.date,
      (unsigned)ts.hours, (unsigned)ts.minutes, (unsigned)ts.seconds
    );

    if (lenLine > 0 && lenLine < (int)sizeof(line)) {
      if (!m_sdBackup.appendLine(line)) DBG.error("SD: appendLine failed");
    } else {
      DBG.error("SD: snprintf line failed/overflow (len=%d)", lenLine);
    }

    // ---- Self-test POST on BOOT/WAKE ----
    if (doSelfTest) {
      LinkChannel ch = readChannel();

      char j[320];
      int len = std::snprintf(
        j, sizeof(j),
        "[{\"ts\":%s,\"values\":{\"metricId\":\"%s\",\"value\":%.3f,"
        "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}}]",
        tsStr,
        Cfg().metric_id,
        val,
        (unsigned)ts.year, (unsigned)ts.month, (unsigned)ts.date,
        (unsigned)ts.hours, (unsigned)ts.minutes, (unsigned)ts.seconds
      );

      if (len > 0 && len < (int)sizeof(j)) {
        int http = -1;

        if (ch == LinkChannel::Eth) {
          if (!ensureEthReadyAndLinkUp()) {
            DBG.error("[%s] ETH not ready/link -> POST skipped", tag);
          } else {
            if (startsWith(Cfg().server_url, "https://")) {
              DBG.data("TB payload (self-test): %s", j);
              http = HttpsW5500::postJson(
                Cfg().server_url,
                Cfg().server_auth_b64,
                j,
                (uint16_t)std::strlen(j),
                20000
              );
              DBG.info("[%s] ETH HTTPS POST code=%d", tag, http);
            } else if (startsWith(Cfg().server_url, "http://")) {
              DBG.data("TB payload (self-test): %s", j);
              http = httpPostPlainW5500(
                Cfg().server_url,
                Cfg().server_auth_b64,
                j,
                (uint16_t)std::strlen(j),
                15000
              );
              DBG.info("[%s] ETH HTTP POST code=%d", tag, http);
            } else {
              DBG.error("[%s] Unsupported SERVER_URL scheme", tag);
            }
          }
        } else {
          m_gsm.powerOn();
          if (m_gsm.init() == GsmStatus::Ok) {
            http = m_gsm.httpPost(Cfg().server_url, j, (uint16_t)std::strlen(j));
            DBG.info("[%s] GSM POST code=%d", tag, http);
            m_gsm.disconnect();
          } else {
            DBG.error("[%s] GSM init fail -> POST skipped", tag);
          }
          m_gsm.powerOff();
        }
      } else {
        DBG.error("[%s] Test JSON build failed", tag);
      }
    }

    // ---- Обычная отправка: раз в N опросов ----
    m_pollCounter++;
    if (m_pollCounter >= Cfg().send_interval_polls) {
      m_pollCounter = 0;
      transmitBuffer();
    }

    // ---- Сон/ожидание ----
    const uint32_t pollSec = Cfg().poll_interval_sec;

    if (m_mode == SystemMode::Sleep) {
      DBG.info("Stop Mode %lu сек...", (unsigned long)pollSec);
      ledOff();
      m_power.enterStopMode(pollSec);
      DBG.info("...проснулись!");
      wokeFromStop = true;
    } else {
      DBG.info("Ожидание %lu сек", (unsigned long)pollSec);
      HAL_Delay(pollSec * 1000UL);
      wokeFromStop = false;
    }

    firstCycle = false;
  }
}

void App::transmitBuffer()
{
  LinkChannel ch = readChannel();
  DBG.info("======== ОТПРАВКА ЖУРНАЛА (%s) ========",
           (ch == LinkChannel::Eth) ? "ETH" : "GSM");

  if (ch == LinkChannel::Eth) {
    if (!ensureEthReadyAndLinkUp()) {
      DBG.error("ETH selected -> skip (no GSM fallback)");
      return;
    }

    retransmitBackup();
    DBG.info("======== ОТПРАВКА ЖУРНАЛА КОНЕЦ ========");
    return;
  }

  m_gsm.powerOn();
  if (m_gsm.init() != GsmStatus::Ok) {
    DBG.error("GSM init fail (журнал остаётся на SD)");
    m_gsm.powerOff();
    return;
  }

  retransmitBackup();

  m_gsm.disconnect();
  m_gsm.powerOff();
  DBG.info("======== ОТПРАВКА ЖУРНАЛА КОНЕЦ ========");
}

void App::transmitSingle(float value, const DateTime& dt)
{
  if (readChannel() == LinkChannel::Eth) {
    DBG.error("ETH selected -> transmitSingle via GSM запрещён, пропуск");
    return;
  }

  const uint64_t unixMs = toUnixMs(dt);
  char tsStr[24];
  u64_to_dec(tsStr, sizeof(tsStr), unixMs);

  char j[320];
  int len = std::snprintf(
    j, sizeof(j),
    "[{\"ts\":%s,\"values\":{\"metricId\":\"%s\",\"value\":%.3f,"
    "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}}]",
    tsStr,
    Cfg().metric_id,
    value,
    (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.date,
    (unsigned)dt.hours, (unsigned)dt.minutes, (unsigned)dt.seconds
  );
  if (len <= 0 || len >= (int)sizeof(j)) return;

  m_gsm.powerOn();
  if (m_gsm.init() == GsmStatus::Ok) {
    auto s = m_gsm.httpPost(Cfg().server_url, j, (uint16_t)len);
    DBG.info("DEBUG HTTP: %d", (int)s);
    m_gsm.disconnect();
  } else {
    DBG.error("GSM init fail (DEBUG)");
  }
  m_gsm.powerOff();
}

void App::retransmitBackup()
{
  LinkChannel ch = readChannel();

  while (m_sdBackup.exists()) {
    uint32_t lines = 0;
    FSIZE_t used = 0;

    const uint32_t maxPayload =
      (Config::HTTP_CHUNK_MAX < Config::JSON_BUFFER_SIZE)
        ? Config::HTTP_CHUNK_MAX
        : (Config::JSON_BUFFER_SIZE - 1);

    bool ok = m_sdBackup.readChunkAsJsonArray(m_json, sizeof(m_json), maxPayload, lines, used);
    if (!ok || lines == 0 || used == 0) {
      DBG.error("SD: chunk read failed (ok=%d lines=%u used=%lu)",
                ok ? 1 : 0, (unsigned)lines, (unsigned long)used);
      return;
    }

    DBG.data("TB payload len=%u", (unsigned)std::strlen(m_json));
    DBG.data("TB payload: %s", m_json);

    DBG.info("Send chunk: lines=%u bytesUsed=%lu payloadLen=%u",
             (unsigned)lines, (unsigned long)used, (unsigned)std::strlen(m_json));

    int http = -1;

    if (ch == LinkChannel::Eth) {
      if (!ensureEthReadyAndLinkUp()) {
        DBG.error("ETH selected but not ready/link -> stop resend, keep journal");
        return;
      }

      if (startsWith(Cfg().server_url, "https://")) {
        http = HttpsW5500::postJson(
          Cfg().server_url,
          Cfg().server_auth_b64,
          m_json,
          (uint16_t)std::strlen(m_json),
          20000
        );
        DBG.info("ETH HTTPS: %d", http);
      } else if (startsWith(Cfg().server_url, "http://")) {
        http = httpPostPlainW5500(
          Cfg().server_url,
          Cfg().server_auth_b64,
          m_json,
          (uint16_t)std::strlen(m_json),
          15000
        );
        DBG.info("ETH HTTP: %d", http);
      } else {
        DBG.error("Unsupported URL scheme in SERVER_URL");
        return;
      }
    } else {
      http = m_gsm.httpPost(Cfg().server_url, m_json, (uint16_t)std::strlen(m_json));
      DBG.info("GSM HTTP: %d", http);
    }

    if (http == 200) {
      if (!m_sdBackup.consumePrefix(used)) {
        DBG.error("SD: consumePrefix failed");
        return;
      }
    } else {
      DBG.error("HTTP fail, journal preserved");
      return;
    }
  }

  DBG.info("Журнал отправлен полностью");
}
