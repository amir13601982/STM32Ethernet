#ifndef WIZCHIP_CONF_H
#define WIZCHIP_CONF_H

#include <stdint.h>
#include <stddef.h>
// All register definitions (PHYCFGR, SIMR, Sn_RXBUF_SIZE, Sn_TXBUF_SIZE, etc.)
// are now properly defined in w5500.h — no fallback #ifndef guards needed here.
#include "w5500.h"

#ifdef __cplusplus
extern "C" {
#endif


// Network Configuration Structure

typedef struct {
    uint8_t mac[6];  // MAC address
    uint8_t ip[4];   // IP address
    uint8_t sn[4];   // Subnet mask
    uint8_t gw[4];   // Gateway address
    uint8_t dns[4];  // DNS server (not stored in W5500 registers, software only)
    uint8_t dhcp;    // 0 = static, 1 = DHCP
} wiz_NetInfo;


// Return Codes

#define WIZCHIP_OK              ( 0)
#define WIZCHIP_ERROR_TIMEOUT   (-1)
#define WIZCHIP_ERROR_INVALID   (-2)
#define WIZCHIP_ERROR_NETWORK   (-3)


// Constants

#define WIZCHIP_DEFAULT_RTR     2000  // 200ms (in 100us units)
#define WIZCHIP_DEFAULT_RCR     8     // 8 retries
#define WIZCHIP_VERSION         0x04  // Expected VERSIONR value for W5500
#define WIZCHIP_MAX_SOCKETS     8


// Predefined buffer layout presets for wizchip_set_buffer_config()

#define WIZCHIP_BUFFER_CONFIG_EQUAL   0  // All 8 sockets: 2KB TX + 2KB RX each
#define WIZCHIP_BUFFER_CONFIG_SERVER  1  // Socket 0: 8KB; sockets 1-7: 1KB each
#define WIZCHIP_BUFFER_CONFIG_CLIENT  2  // Sockets 0-3: 4KB each; 4-7: 0KB


// Core Lifecycle


/**
 * @brief Resets and initialises the W5500.
 *
 * Correct call order:
 *   wizchip_reset()  — software reset, waits for chip ready
 *   wizchip_init()   — sets RTR/RCR defaults and buffer sizes
 *
 * @return WIZCHIP_OK on success, error code on failure.
 */
int8_t wizchip_reset(void);
int8_t wizchip_init(void);

/**
 * @brief Reads VERSIONR and confirms it equals 0x04.
 * @return 1 if chip is responding correctly, 0 otherwise.
 */
uint8_t wizchip_isconnected(void);

/**
 * @brief Returns raw VERSIONR value (should be 0x04).
 */
uint8_t wizchip_getversion(void);


// Network Configuration

int8_t  wizchip_setnetinfo(wiz_NetInfo* netinfo);
int8_t  wizchip_getnetinfo(wiz_NetInfo* netinfo);
uint8_t wizchip_validate_netinfo(wiz_NetInfo* netinfo);


// Socket Buffer Sizes
//
// Valid sizes per socket: 0, 1, 2, 4, 8, or 16 (KB).
// Total TX and total RX across all 8 sockets must not exceed 16KB each.
// The register stores the KB value directly (e.g. write 2 for 2KB).

int8_t wizchip_set_buffer_sizes(uint8_t* tx_size, uint8_t* rx_size);
int8_t wizchip_get_buffer_sizes(uint8_t* tx_size, uint8_t* rx_size);
int8_t wizchip_set_buffer_config(uint8_t config);


// Timeout & Retry

int8_t wizchip_settimeout(uint16_t rtr, uint8_t rcr);
int8_t wizchip_gettimeout(uint16_t* rtr, uint8_t* rcr);


// Link / PHY Status

uint8_t wizchip_getlinkstatus(void);
int8_t  wizchip_getphystatus(uint8_t* duplex, uint8_t* speed, uint8_t* link);


// Interrupts

int8_t  wizchip_enable_interrupts(uint8_t socket_mask);
uint8_t wizchip_get_interrupt_status(void);
int8_t  wizchip_clear_interrupts(uint8_t socket_mask);


// Debug / Utility

void wizchip_print_netinfo(void);
void wizchip_print_status(void);
int  wizchip_format_netinfo(wiz_NetInfo* netinfo, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // WIZCHIP_CONF_H