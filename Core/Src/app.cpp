/**
 * ================================================================
 * @file app.cpp
 * @brief Реализация главного класса приложения.
 * ================================================================
 */
#include "app.hpp"

#include "w5500_net.hpp"
#include "https_w5500.hpp"

#include <cctype>
#include <cstring>
#include <cstdio>

extern "C" {
  // HAL handles (cube *.h удалены — объявляем напрямую)
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

// -------------------- ETH object --------------------
static W5500Net eth;

// -------------------- Канал связи --------------------
enum class LinkChannel : uint8_t { Gsm = 0, Eth = 1 };

// PB0=1 -> GSM, PB0=0 -> ETH
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
// ETH: правильный порядок init -> link
// ============================================================================

// PHY link читаем через ctlwizchip, но только ПОСЛЕ eth.init()
static bool ethLinkUpAfterInit()
{
  uint8_t link = 0;
  ctlwizchip(CW_GET_PHYLINK, (void*)&link);
  return (link != 0);
}

// Гарантирует init W5500 и затем проверяет PHY link
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

// -------------------- HTTP (plain) over W5500 --------------------
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

// =================================================================

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
  m_gsm.powerOff();

  m_mode = readMode();
  DBG.info("Mode: %s", (m_mode == SystemMode::Debug) ? "DEBUG" : "SLEEP");
  logNetSelect("INIT");

  if (m_mode == SystemMode::Debug) {
    DBG.info(">>> РЕЖИМ: DEBUG <<<");
    DBG.info("Опрос %d сек", (int)Config::POLL_INTERVAL_SEC);
    ledOn();
  } else {
    DBG.info(">>> РЕЖИМ: SLEEP <<<");
    DBG.info("Опрос %d сек", (int)Config::POLL_INTERVAL_SEC);
    ledBlink(3, 200);
  }
}

[[noreturn]] void App::run()
{
  bool wokeFromStop = false;
  bool firstCycle = true;

  while (true) {
    if (eth.ready()) {
      eth.tick();
    }

    m_mode = readMode();

    const bool doSelfTest = firstCycle || wokeFromStop;
    const char* tag = firstCycle ? "BOOT" : "WAKE";

    if (doSelfTest) {
      DBG.info("========================================================================");
      DBG.info("[%s] Self-test: NET_SELECT + Modbus + Server POST", tag);
      logNetSelect(tag);
      DBG.info("========================================================================");
    }

    // ---- Modbus опрос ----
    DateTime ts{};
    float val = m_sensor.read(ts);

    char timeStr[32]{};
    ts.formatISO8601(timeStr);
    DBG.data("val=%.3f t=%s", val, timeStr);

    ledBlink(1, 50);

    // ---- SD журнал ----
    char line[256];
    int lenLine = std::snprintf(
      line, sizeof(line),
      "{\"metricId\":\"%s\",\"value\":%.3f,"
      "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}",
      Config::METRIC_ID,
      val,
      (unsigned)ts.year, (unsigned)ts.month, (unsigned)ts.date,
      (unsigned)ts.hours, (unsigned)ts.minutes, (unsigned)ts.seconds
    );
    if (lenLine > 0) m_sdBackup.appendLine(line);

    // ---- Тестовая отправка на сервер на BOOT/WAKE ----
    if (doSelfTest) {
      LinkChannel ch = readChannel();

      char j[256];
      int len = std::snprintf(
        j, sizeof(j),
        "[{\"metricId\":\"%s\",\"value\":%.3f,"
        "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}]",
        Config::METRIC_ID,
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
            if (startsWith(Config::SERVER_URL, "https://")) {
              http = HttpsW5500::postJson(
                Config::SERVER_URL,
                Config::SERVER_AUTH,
                j,
                (uint16_t)std::strlen(j),
                20000
              );
              DBG.info("[%s] ETH HTTPS POST code=%d", tag, http);
            } else if (startsWith(Config::SERVER_URL, "http://")) {
              http = httpPostPlainW5500(
                Config::SERVER_URL,
                Config::SERVER_AUTH,
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
            http = m_gsm.httpPost(Config::SERVER_URL, j, (uint16_t)std::strlen(j));
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

    // ---- Обычная отправка журнала ----
    transmitBuffer();

    // ---- Сон/ожидание ----
    if (m_mode == SystemMode::Sleep) {
      DBG.info("Stop Mode %d сек...", (int)Config::POLL_INTERVAL_SEC);
      ledOff();
      m_power.enterStopMode(Config::POLL_INTERVAL_SEC);
      DBG.info("...проснулись!");
      wokeFromStop = true;
    } else {
      DBG.info("Ожидание %d сек", (int)Config::POLL_INTERVAL_SEC);
      HAL_Delay(Config::POLL_INTERVAL_SEC * 1000UL);
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

  // GSM
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

  char j[256];
  int len = std::snprintf(
    j, sizeof(j),
    "[{\"metricId\":\"%s\",\"value\":%.3f,"
    "\"measureTime\":\"20%02u-%02u-%02uT%02u:%02u:%02u.000Z\"}]",
    Config::METRIC_ID,
    value,
    (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.date,
    (unsigned)dt.hours, (unsigned)dt.minutes, (unsigned)dt.seconds
  );
  if (len <= 0 || len >= (int)sizeof(j)) return;

  m_gsm.powerOn();
  if (m_gsm.init() == GsmStatus::Ok) {
    auto s = m_gsm.httpPost(Config::SERVER_URL, j, (uint16_t)len);
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

    DBG.info("Send chunk: lines=%u bytesUsed=%lu payloadLen=%u",
             (unsigned)lines, (unsigned long)used, (unsigned)std::strlen(m_json));

    int http = -1;

    if (ch == LinkChannel::Eth) {
      if (!ensureEthReadyAndLinkUp()) {
        DBG.error("ETH selected but not ready/link -> stop resend, keep journal");
        return;
      }

      if (startsWith(Config::SERVER_URL, "https://")) {
        http = HttpsW5500::postJson(
          Config::SERVER_URL,
          Config::SERVER_AUTH,
          m_json,
          (uint16_t)std::strlen(m_json),
          20000
        );
        DBG.info("ETH HTTPS: %d", http);
      } else if (startsWith(Config::SERVER_URL, "http://")) {
        http = httpPostPlainW5500(
          Config::SERVER_URL,
          Config::SERVER_AUTH,
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
      http = m_gsm.httpPost(Config::SERVER_URL, m_json, (uint16_t)std::strlen(m_json));
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
