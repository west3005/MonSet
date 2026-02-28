#include "https_w5500.hpp"

#include "debug_uart.hpp"

#include <cstring>
#include <cctype>

extern "C" {
#include "socket.h"
#include "dns.h"
#include "w5500.h"  // getSn_RX_RSR/getSn_TX_FSR

#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
}

static const char* s_caPem = nullptr;
void HttpsW5500::setCaPem(const char* caPem) { s_caPem = caPem; }

static bool isIpv4Literal(const char* s) {
  if (!s || !*s) return false;
  for (const char* p = s; *p; ++p) if (!std::isdigit((unsigned char)*p) && *p != '.') return false;
  return true;
}

static bool resolveHost(const char* host, uint8_t outIp[4]) {
  if (isIpv4Literal(host)) {
    uint32_t a=0,b=0,c=0,d=0;
    if (std::sscanf(host, "%lu.%lu.%lu.%lu", &a,&b,&c,&d) != 4) return false;
    if (a>255||b>255||c>255||d>255) return false;
    outIp[0]=(uint8_t)a; outIp[1]=(uint8_t)b; outIp[2]=(uint8_t)c; outIp[3]=(uint8_t)d;
    return true;
  }

  // DNS через ioLibrary
  static uint8_t dnsBuf[512];
  DNS_init(1, dnsBuf);

  wiz_NetInfo ni{};
  wizchip_getnetinfo(&ni);

  uint8_t ip[4]{};
  int8_t r = DNS_run(ni.dns, (uint8_t*)host, ip);
  if (r != 1) return false;

  std::memcpy(outIp, ip, 4);
  return true;
}

// -------- mbedTLS BIO over W5500 socket --------
struct TlsSockCtx {
  uint8_t sn;
  uint32_t deadline;
};

static int w5500_send_cb(void* ctx, const unsigned char* buf, size_t len) {
  auto* c = (TlsSockCtx*)ctx;
  if (len == 0) return 0;

  // Если нет места в TX FIFO — "WANT_WRITE"
  uint16_t fsr = getSn_TX_FSR(c->sn);
  if (fsr == 0) return MBEDTLS_ERR_SSL_WANT_WRITE;

  uint16_t toSend = (len > fsr) ? fsr : (uint16_t)len;
  int32_t r = send(c->sn, (uint8_t*) buf, toSend);

  if (r < 0)  return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  if (r == 0) return MBEDTLS_ERR_SSL_WANT_WRITE;

  return (int) r;
}

static int w5500_recv_cb(void* ctx, unsigned char* buf, size_t len) {
  auto* c = (TlsSockCtx*)ctx;
  if (len == 0) return 0;

  // Если нет данных — "WANT_READ"
  uint16_t rsr = getSn_RX_RSR(c->sn);
  if (rsr == 0) {
    if (HAL_GetTick() > c->deadline) return MBEDTLS_ERR_SSL_TIMEOUT;
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  uint16_t toRead = (len > rsr) ? rsr : (uint16_t)len;
  int32_t r = recv(c->sn, (uint8_t*) buf, toRead);

  if (r < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;

  /* r == 0: трактуем как EOF (как обычный recv() при закрытии соединения) */
  return (int) r;
}

// -------- URL parse --------
bool HttpsW5500::parseHttpsUrl(const char* url, UrlParts& out) {
  if (!url) return false;
  std::memset(&out, 0, sizeof(out));
  out.port = 443;

  const char* p = url;
  const char* prefix = "https://";
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
    while (*p && std::isdigit((unsigned char)*p)) { port = port * 10u + (uint32_t)(*p - '0'); p++; }
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

int HttpsW5500::postJson(const char* httpsUrl,
                         const char* authBasicB64,
                         const char* json,
                         uint16_t jsonLen,
                         uint32_t timeoutMs) {
  UrlParts u{};
  if (!parseHttpsUrl(httpsUrl, u)) return -10;

  uint8_t ip[4]{};
  if (!resolveHost(u.host, ip)) return -11;

  const uint8_t sn = 0;
  const uint16_t localPort = 50001;

  if (socket(sn, Sn_MR_TCP, localPort, 0) != sn) { close(sn); return -20; }
  if (connect(sn, ip, u.port) != SOCK_OK) { close(sn); return -21; }

  // ---- mbedTLS init ----
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr;

  mbedtls_ssl_init(&ssl);
  mbedtls_ssl_config_init(&conf);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr);

  const char* pers = "w5500_https";
  if (mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy,
                            (const unsigned char*)pers, std::strlen(pers)) != 0) {
    close(sn);
    return -30;
  }

  mbedtls_x509_crt cacert;
  mbedtls_x509_crt_init(&cacert);

  if (s_caPem) {
    // CA PEM (строка с \n)
    int rc = mbedtls_x509_crt_parse(&cacert,
                                    (const unsigned char*)s_caPem,
                                    std::strlen(s_caPem) + 1);
    if (rc < 0) {
      close(sn);
      return -31;
    }
  }

  if (mbedtls_ssl_config_defaults(&conf,
                                  MBEDTLS_SSL_IS_CLIENT,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    close(sn);
    return -32;
  }

  mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr);

  if (s_caPem) {
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, nullptr);
  } else {
    // Работает “как HTTPS”, но без проверки сертификата сервера
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
  }

  if (mbedtls_ssl_setup(&ssl, &conf) != 0) { close(sn); return -33; }

  // SNI/проверка hostname (даже при VERIFY_NONE полезно для SNI)
  (void)mbedtls_ssl_set_hostname(&ssl, u.host);

  TlsSockCtx bio{sn, HAL_GetTick() + timeoutMs};
  mbedtls_ssl_set_bio(&ssl, &bio, w5500_send_cb, w5500_recv_cb, nullptr);

  // ---- handshake loop ----
  while (true) {
    int rc = mbedtls_ssl_handshake(&ssl);
    if (rc == 0) break;
    if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
      if (HAL_GetTick() > bio.deadline) { rc = MBEDTLS_ERR_SSL_TIMEOUT; }
      else { HAL_Delay(1); continue; }
    }
    // Ошибка handshake
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_entropy_free(&entropy);
    disconnect(sn); close(sn);
    return -40;
  }

  // ---- HTTP request ----
  char hdr[600];
  int hdrLen = std::snprintf(
    hdr, sizeof(hdr),
    "POST %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Authorization: Basic %s\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n"
    "\r\n",
    u.path, u.host, authBasicB64 ? authBasicB64 : "", (unsigned)jsonLen
  );
  if (hdrLen <= 0 || (size_t)hdrLen >= sizeof(hdr)) return -50;

  auto sslWriteAll = [&](const uint8_t* p, uint32_t n) -> int {
    uint32_t off = 0;
    while (off < n) {
      int rc = mbedtls_ssl_write(&ssl, p + off, (size_t)(n - off));
      if (rc > 0) { off += (uint32_t)rc; continue; }
      if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) { HAL_Delay(1); continue; }
      return -1;
    }
    return 0;
  };

  if (sslWriteAll((const uint8_t*)hdr, (uint32_t)hdrLen) != 0) return -51;
  if (sslWriteAll((const uint8_t*)json, (uint32_t)jsonLen) != 0) return -52;

  // ---- read response + parse status ----
  char rx[768];
  int used = 0;
  int httpCode = -1;
  uint32_t t0 = HAL_GetTick();

  while ((HAL_GetTick() - t0) < timeoutMs) {
    int rc = mbedtls_ssl_read(&ssl, (unsigned char*)rx + used, sizeof(rx) - 1 - used);
    if (rc > 0) {
      used += rc;
      rx[used] = 0;

      const char* p = std::strstr(rx, "HTTP/1.1 ");
      if (!p) p = std::strstr(rx, "HTTP/1.0 ");
      if (p) {
        int code = 0;
        if (std::sscanf(p, "HTTP/%*s %d", &code) == 1) { httpCode = code; break; }
      }
      continue;
    }

    if (rc == 0) break; // close_notify
    if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) { HAL_Delay(2); continue; }
    break;
  }

  // ---- cleanup ----
  (void)mbedtls_ssl_close_notify(&ssl);

  mbedtls_ssl_free(&ssl);
  mbedtls_ssl_config_free(&conf);
  mbedtls_x509_crt_free(&cacert);
  mbedtls_ctr_drbg_free(&ctr);
  mbedtls_entropy_free(&entropy);

  disconnect(sn);
  close(sn);

  return httpCode;
}
