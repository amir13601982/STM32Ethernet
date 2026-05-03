// wizchip_conf.cpp 
#include <Arduino.h>    
#include <SPI.h>        
#include <string.h>     
#include "wizchip_conf.h"
#include "w5500.h"


int8_t wizchip_reset(void)
{
    // Software reset: set RST bit in Mode Register
    WIZCHIP_WRITE(MR, 0x80);

    // Wait for chip to clear the RST bit (indicates reset complete)
    uint32_t start = millis();
    while (WIZCHIP_READ(MR) & 0x80) {
        if ((millis() - start) > 1000) {
            return WIZCHIP_ERROR_TIMEOUT;
        }
    }

    // 10ms settling delay after reset
    uint32_t delay_start = millis();
    while ((millis() - delay_start) < 10) { /* wait */ }

    return WIZCHIP_OK;
}

int8_t wizchip_init(void)
{
    int8_t rst = wizchip_reset();
    if (rst != WIZCHIP_OK) {
        return rst;
    }

    // Now verify the chip is actually there
    if (!wizchip_isconnected()) {
        return WIZCHIP_ERROR_NETWORK;
    }

    // Set default timeout and retry values
    setRTR(WIZCHIP_DEFAULT_RTR);  // 200ms
    setRCR(WIZCHIP_DEFAULT_RCR);  // 8 retries

    // Default buffer layout: 2KB TX + 2KB RX per socket (8 x 2 = 16KB each)
    uint8_t tx_sizes[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t rx_sizes[8] = {2, 2, 2, 2, 2, 2, 2, 2};

    if (wizchip_set_buffer_sizes(tx_sizes, rx_sizes) != WIZCHIP_OK) {
        return WIZCHIP_ERROR_INVALID;
    }

    return WIZCHIP_OK;
}

uint8_t wizchip_isconnected(void)
{
    return (WIZCHIP_READ(VERSIONR) == WIZCHIP_VERSION) ? 1 : 0;
}

uint8_t wizchip_getversion(void)
{
    return WIZCHIP_READ(VERSIONR);
}

int8_t wizchip_setnetinfo(wiz_NetInfo* netinfo)
{
    if (netinfo == NULL) return WIZCHIP_ERROR_INVALID;

    if (!wizchip_validate_netinfo(netinfo)) {
        return WIZCHIP_ERROR_INVALID;
    }

    WIZCHIP_WRITE_BUF(SHAR, netinfo->mac, 6);
    WIZCHIP_WRITE_BUF(SIPR, netinfo->ip,  4);
    WIZCHIP_WRITE_BUF(SUBR, netinfo->sn,  4);
    WIZCHIP_WRITE_BUF(GAR,  netinfo->gw,  4);

   
    uint8_t verify[6];
    WIZCHIP_READ_BUF(SHAR, verify, 6);
    if (memcmp(verify, netinfo->mac, 6) != 0) {
        return WIZCHIP_ERROR_NETWORK;
    }

    return WIZCHIP_OK;
}

int8_t wizchip_getnetinfo(wiz_NetInfo* netinfo)
{
    if (netinfo == NULL) return WIZCHIP_ERROR_INVALID;

    WIZCHIP_READ_BUF(SHAR, netinfo->mac, 6);
    WIZCHIP_READ_BUF(SIPR, netinfo->ip,  4);
    WIZCHIP_READ_BUF(SUBR, netinfo->sn,  4);
    WIZCHIP_READ_BUF(GAR,  netinfo->gw,  4);

    // DNS and DHCP are software-only fields, not in W5500 registers
    memset(netinfo->dns, 0, 4);
    netinfo->dhcp = 0;

    return WIZCHIP_OK;
}

uint8_t wizchip_validate_netinfo(wiz_NetInfo* netinfo)
{
    if (netinfo == NULL) return 0;

    // Reject all-zero MAC
    uint8_t zero_mac[6] = {0, 0, 0, 0, 0, 0};
    if (memcmp(netinfo->mac, zero_mac, 6) == 0) return 0;

    // Reject broadcast MAC
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (memcmp(netinfo->mac, bcast_mac, 6) == 0) return 0;

    // Reject multicast MAC (LSB of first byte must be 0 for unicast)
    if (netinfo->mac[0] & 0x01) return 0;

    // Reject 0.0.0.0 and 255.255.255.255
    uint32_t ip = ((uint32_t)netinfo->ip[0] << 24) | ((uint32_t)netinfo->ip[1] << 16) |
                  ((uint32_t)netinfo->ip[2] <<  8) |  (uint32_t)netinfo->ip[3];
    if (ip == 0 || ip == 0xFFFFFFFFUL) return 0;

    // Reject all-zero subnet
    uint32_t mask = ((uint32_t)netinfo->sn[0] << 24) | ((uint32_t)netinfo->sn[1] << 16) |
                    ((uint32_t)netinfo->sn[2] <<  8) |  (uint32_t)netinfo->sn[3];
    if (mask == 0) return 0;

    // Verify contiguous subnet mask (e.g. 0xFFFFFF00 is valid, 0xFF00FF00 is not)
    uint32_t inv = ~mask;
    if ((inv & (inv + 1)) != 0) return 0;

    return 1;
}

int8_t wizchip_set_buffer_sizes(uint8_t* tx_size, uint8_t* rx_size)
{
    if (tx_size == NULL || rx_size == NULL) return WIZCHIP_ERROR_INVALID;

    uint16_t total_tx = 0, total_rx = 0;

    for (int i = 0; i < 8; i++) {
        // Valid sizes: 0, 1, 2, 4, 8, 16 KB
        uint8_t valid_tx = (tx_size[i] == 0  || tx_size[i] == 1  || tx_size[i] == 2 ||
                            tx_size[i] == 4  || tx_size[i] == 8  || tx_size[i] == 16);
        uint8_t valid_rx = (rx_size[i] == 0  || rx_size[i] == 1  || rx_size[i] == 2 ||
                            rx_size[i] == 4  || rx_size[i] == 8  || rx_size[i] == 16);

        if (!valid_tx || !valid_rx) return WIZCHIP_ERROR_INVALID;

        total_tx += tx_size[i];
        total_rx += rx_size[i];
    }

    // Total must not exceed 16KB per direction
    if (total_tx > 16 || total_rx > 16) return WIZCHIP_ERROR_INVALID;

    // Write sizes directly — the register stores KB value as-is
    for (int i = 0; i < 8; i++) {
        WIZCHIP_WRITE_B(W5500_SOCKET_REG(i), Sn_TXBUF_SIZE, tx_size[i]);
        WIZCHIP_WRITE_B(W5500_SOCKET_REG(i), Sn_RXBUF_SIZE, rx_size[i]);
    }

    return WIZCHIP_OK;
}

int8_t wizchip_get_buffer_sizes(uint8_t* tx_size, uint8_t* rx_size)
{
    if (tx_size == NULL || rx_size == NULL) return WIZCHIP_ERROR_INVALID;

    for (int i = 0; i < 8; i++) {
        // Register holds KB value directly — read it straight back
        tx_size[i] = WIZCHIP_READ_B(W5500_SOCKET_REG(i), Sn_TXBUF_SIZE);
        rx_size[i] = WIZCHIP_READ_B(W5500_SOCKET_REG(i), Sn_RXBUF_SIZE);
    }

    return WIZCHIP_OK;
}

int8_t wizchip_set_buffer_config(uint8_t config)
{
    uint8_t tx[8], rx[8];

    switch (config) {
        case WIZCHIP_BUFFER_CONFIG_EQUAL:
            for (int i = 0; i < 8; i++) { tx[i] = 2; rx[i] = 2; }
            break;

        case WIZCHIP_BUFFER_CONFIG_SERVER:
            tx[0] = 8; rx[0] = 8;
            for (int i = 1; i < 8; i++) { tx[i] = 1; rx[i] = 1; }
            break;

        case WIZCHIP_BUFFER_CONFIG_CLIENT:
            for (int i = 0; i < 4; i++) { tx[i] = 4; rx[i] = 4; }
            for (int i = 4; i < 8; i++) { tx[i] = 0; rx[i] = 0; }
            break;

        default:
            return WIZCHIP_ERROR_INVALID;
    }

    return wizchip_set_buffer_sizes(tx, rx);
}

int8_t wizchip_settimeout(uint16_t rtr, uint8_t rcr)
{
    setRTR(rtr);
    setRCR(rcr);

    if (getRTR() != rtr || getRCR() != rcr) {
        return WIZCHIP_ERROR_NETWORK;
    }

    return WIZCHIP_OK;
}

int8_t wizchip_gettimeout(uint16_t* rtr, uint8_t* rcr)
{
    if (rtr == NULL || rcr == NULL) return WIZCHIP_ERROR_INVALID;
    *rtr = getRTR();
    *rcr = getRCR();
    return WIZCHIP_OK;
}

uint8_t wizchip_getlinkstatus(void)
{
    return (WIZCHIP_READ(PHYCFGR) & 0x01) ? 1 : 0;
}

int8_t wizchip_getphystatus(uint8_t* duplex, uint8_t* speed, uint8_t* link)
{
    if (duplex == NULL || speed == NULL || link == NULL) {
        return WIZCHIP_ERROR_INVALID;
    }

    uint8_t phy = WIZCHIP_READ(PHYCFGR);
    *link   = (phy & 0x01) ? 1 : 0;
    *speed  = (phy & 0x02) ? 1 : 0;
    *duplex = (phy & 0x04) ? 1 : 0;

    return WIZCHIP_OK;
}

int8_t wizchip_enable_interrupts(uint8_t socket_mask)
{
    WIZCHIP_WRITE(SIMR, socket_mask);
    return WIZCHIP_OK;
}

uint8_t wizchip_get_interrupt_status(void)
{
    return getSIR();
}

int8_t wizchip_clear_interrupts(uint8_t socket_mask)
{
    for (int i = 0; i < 8; i++) {
        if (socket_mask & (1 << i)) {
            WIZCHIP_WRITE_B(W5500_SOCKET_REG(i), Sn_IR, 0xFF);
        }
    }
	 WIZCHIP_WRITE(SIR, socket_mask); 
    return WIZCHIP_OK;
}

#ifdef ENABLE_PRINTF_DEBUG
void wizchip_print_netinfo(void)
{
    wiz_NetInfo info;
    if (wizchip_getnetinfo(&info) == WIZCHIP_OK) {
        printf("W5500 Network Configuration:\n");
        printf("  MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n",
               info.mac[0], info.mac[1], info.mac[2],
               info.mac[3], info.mac[4], info.mac[5]);
        printf("  IP:      %d.%d.%d.%d\n",
               info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
        printf("  Subnet:  %d.%d.%d.%d\n",
               info.sn[0], info.sn[1], info.sn[2], info.sn[3]);
        printf("  Gateway: %d.%d.%d.%d\n",
               info.gw[0], info.gw[1], info.gw[2], info.gw[3]);
    }
}

void wizchip_print_status(void)
{
    printf("W5500 Status:\n");
    printf("  Version:   0x%02X\n", wizchip_getversion());
    printf("  Connected: %s\n",     wizchip_isconnected() ? "Yes" : "No");
    printf("  Link:      %s\n",     wizchip_getlinkstatus() ? "Up" : "Down");

    uint8_t duplex, speed, link;
    if (wizchip_getphystatus(&duplex, &speed, &link) == WIZCHIP_OK) {
        printf("  Speed:  %s\n",  speed  ? "100Mbps" : "10Mbps");
        printf("  Duplex: %s\n",  duplex ? "Full"    : "Half");
    }

    uint16_t rtr; uint8_t rcr;
    if (wizchip_gettimeout(&rtr, &rcr) == WIZCHIP_OK) {
        printf("  Timeout: %d x 100us, Retries: %d\n", rtr, rcr);
    }
}
#else
void wizchip_print_netinfo(void) { /* define ENABLE_PRINTF_DEBUG to activate */ }
void wizchip_print_status(void)  { /* define ENABLE_PRINTF_DEBUG to activate */ }
#endif

int wizchip_format_netinfo(wiz_NetInfo* netinfo, char* buffer, size_t buffer_size)
{
    if (netinfo == NULL || buffer == NULL || buffer_size < 150) return 0;

    // Safe bare-metal hex/decimal formatters — no printf/sprintf/heap needed
    const char* hex = "0123456789ABCDEF";
    int pos = 0;

    // MAC: XX:XX:XX:XX:XX:XX
    const char mac_label[] = "MAC:";
    for (int i = 0; mac_label[i]; i++) buffer[pos++] = mac_label[i];
    for (int i = 0; i < 6; i++) {
        buffer[pos++] = hex[netinfo->mac[i] >> 4];
        buffer[pos++] = hex[netinfo->mac[i] & 0xF];
        buffer[pos++] = (i < 5) ? ':' : ' ';
    }

    // IP: d.d.d.d
    const char ip_label[] = "IP:";
    for (int i = 0; ip_label[i]; i++) buffer[pos++] = ip_label[i];
    for (int i = 0; i < 4; i++) {
        uint8_t v = netinfo->ip[i];
        if (v >= 100) buffer[pos++] = '0' + (v / 100);
        if (v >=  10) buffer[pos++] = '0' + ((v / 10) % 10);
        buffer[pos++] = '0' + (v % 10);
        buffer[pos++] = (i < 3) ? '.' : ' ';
    }

    // Subnet
    const char sn_label[] = "Subnet:";
    for (int i = 0; sn_label[i]; i++) buffer[pos++] = sn_label[i];
    for (int i = 0; i < 4; i++) {
        uint8_t v = netinfo->sn[i];
        if (v >= 100) buffer[pos++] = '0' + (v / 100);
        if (v >=  10) buffer[pos++] = '0' + ((v / 10) % 10);
        buffer[pos++] = '0' + (v % 10);
        buffer[pos++] = (i < 3) ? '.' : ' ';
    }

    // Gateway
    const char gw_label[] = "Gateway:";
    for (int i = 0; gw_label[i]; i++) buffer[pos++] = gw_label[i];
    for (int i = 0; i < 4; i++) {
        uint8_t v = netinfo->gw[i];
        if (v >= 100) buffer[pos++] = '0' + (v / 100);
        if (v >=  10) buffer[pos++] = '0' + ((v / 10) % 10);
        buffer[pos++] = '0' + (v % 10);
        if (i < 3) buffer[pos++] = '.';
    }

    buffer[pos] = '\0';
    return pos;
}