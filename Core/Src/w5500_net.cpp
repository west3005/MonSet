#include "w5500_net.hpp"
#include "w5500_port.h"
#include "config.hpp"
#include "debug_uart.hpp"
#include <cstring>

static volatile bool s_dhcpAssigned = false;
static volatile bool s_dhcpConflict = false;

void W5500Net::dhcpCbAssigned() { s_dhcpAssigned = true; }
void W5500Net::dhcpCbConflict() { s_dhcpConflict = true; }

void W5500Net::getNetInfo(wiz_NetInfo& out) const { out = m_info; }

bool W5500Net::init(SPI_HandleTypeDef* hspi, uint32_t dhcpTimeoutMs) {
	uint8_t ver = getVERSIONR();
	DBG.info("W5500: VERSIONR=0x%02X", ver);

	m_ready = false;
  m_mode = Mode::Static;
  s_dhcpAssigned = false;
  s_dhcpConflict = false;

  W5500_PortInit(hspi);
  W5500_HardReset();

  reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
  reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);
  reg_wizchip_spiburst_cbfunc(wizchip_readburst, wizchip_writeburst);

  uint8_t txsize[8] = {2,2,2,2,2,2,2,2};
  uint8_t rxsize[8] = {2,2,2,2,2,2,2,2};
  if (wizchip_init(txsize, rxsize) != 0) {
    DBG.error("W5500: wizchip_init failed");
    applyStatic();
    return m_ready;
  }

  // Ждём линк (не обязательно, но полезно)
  uint8_t link = 0;
  uint32_t t0 = HAL_GetTick();
  do {
    ctlwizchip(CW_GET_PHYLINK, (void*)&link);
    if (link) break;
    HAL_Delay(50);
  } while ((HAL_GetTick() - t0) < 3000);

  // Сначала DHCP, если не получилось — статический
  if (tryDhcp(dhcpTimeoutMs)) {
    m_mode = Mode::Dhcp;
    m_ready = true;
    return true;
  }

  applyStatic();
  return m_ready;
}

bool W5500Net::tryDhcp(uint32_t timeoutMs) {
  s_dhcpAssigned = false;
  s_dhcpConflict = false;

  DHCP_init(0, m_dhcpBuf);
  reg_dhcp_cbfunc(dhcpCbAssigned, dhcpCbAssigned, dhcpCbConflict);

  m_lastDhcp1s = HAL_GetTick();
  uint32_t t0 = HAL_GetTick();

  while ((HAL_GetTick() - t0) < timeoutMs) {
    uint32_t now = HAL_GetTick();
    if ((now - m_lastDhcp1s) >= 1000) {
      DHCP_time_handler();
      m_lastDhcp1s += 1000;
    }

    uint8_t r = DHCP_run();
    if (r == DHCP_IP_LEASED || r == DHCP_IP_CHANGED || s_dhcpAssigned) {
      wiz_NetInfo ni{};
      std::memcpy(ni.mac, Config::W5500_MAC, 6);
      getIPfromDHCP(ni.ip);
      getGWfromDHCP(ni.gw);
      getSNfromDHCP(ni.sn);
      getDNSfromDHCP(ni.dns);
      ni.dhcp = NETINFO_DHCP;

      wizchip_setnetinfo(&ni);
      m_info = ni;

      DBG.info("W5500: DHCP OK %d.%d.%d.%d",
               ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
      return true;
    }

    if (s_dhcpConflict) {
      DBG.error("W5500: DHCP conflict");
      return false;
    }

    HAL_Delay(1);
  }

  DBG.error("W5500: DHCP timeout -> static");
  return false;
}

void W5500Net::applyStatic() {
  wiz_NetInfo ni{};
  std::memcpy(ni.mac, Config::W5500_MAC, 6);
  std::memcpy(ni.ip,  Config::NET_IP,  4);
  std::memcpy(ni.sn,  Config::NET_SN,  4);
  std::memcpy(ni.gw,  Config::NET_GW,  4);
  std::memcpy(ni.dns, Config::NET_DNS, 4);
  ni.dhcp = NETINFO_STATIC;

  wizchip_setnetinfo(&ni);
  m_info = ni;
  m_mode = Mode::Static;
  m_ready = true;

  DBG.info("W5500: STATIC %d.%d.%d.%d",
           ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
}

void W5500Net::tick() {
  if (m_mode != Mode::Dhcp) return;

  uint32_t now = HAL_GetTick();
  if ((now - m_lastDhcp1s) >= 1000) {
    DHCP_time_handler();
    m_lastDhcp1s += 1000;
  }

  uint8_t r = DHCP_run();
  if (r == DHCP_IP_CHANGED) {
    wiz_NetInfo ni{};
    std::memcpy(ni.mac, Config::W5500_MAC, 6);
    getIPfromDHCP(ni.ip);
    getGWfromDHCP(ni.gw);
    getSNfromDHCP(ni.sn);
    getDNSfromDHCP(ni.dns);
    ni.dhcp = NETINFO_DHCP;

    wizchip_setnetinfo(&ni);
    m_info = ni;

    DBG.info("W5500: DHCP changed %d.%d.%d.%d",
             ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
  }
}
