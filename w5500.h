#ifndef W5500_H
#define W5500_H

#include <stdint.h>

#define W5500_COMMON_REG      ((uint8_t)0x00)               // Common registers
#define W5500_SOCKET_REG(s)   ((uint8_t)(((s) << 2) + 1))  // S0=0x01, S1=0x05, S2=0x09 ...
#define W5500_TX_BUFFER(s)    ((uint8_t)(((s) << 2) + 2))  // S0=0x02, S1=0x06, S2=0x0A ...
#define W5500_RX_BUFFER(s)    ((uint8_t)(((s) << 2) + 3))  // S0=0x03, S1=0x07, S2=0x0B ...


#define MR          0x0000  // Mode Register
#define GAR         0x0001  // Gateway Address (4 bytes: 0x0001–0x0004)
#define SUBR        0x0005  // Subnet Mask (4 bytes: 0x0005–0x0008)
#define SHAR        0x0009  // Source MAC Address (6 bytes: 0x0009–0x000E)
#define SIPR        0x000F  // Source IP Address (4 bytes: 0x000F–0x0012)
#define INTLEVEL    0x0013  // Interrupt Low Level Timer (2 bytes)
#define IR          0x0015  // Interrupt Register
#define IMR         0x0016  // Interrupt Mask Register
#define SIR         0x0017  // Socket Interrupt Register
#define SIMR        0x0018  // Socket Interrupt Mask Register  ← ADDED
#define RTR         0x0019  // Retry Time-value Register (2 bytes: 0x0019–0x001A)
#define RCR         0x001B  // Retry Count Register
#define PTIMER      0x001C  // PPP LCP Request Timer
#define PMAGIC      0x001D  // PPP LCP Magic Number
#define PHAR        0x001E  // PPPoE Server MAC (6 bytes)
#define PSID        0x0024  // PPPoE Session ID (2 bytes)
#define PMRU        0x0026  // PPPoE Max Recv Unit (2 bytes)
#define UIPR        0x0028  // Unreachable IP (4 bytes)
#define UPORT       0x002C  // Unreachable Port (2 bytes)
#define PHYCFGR     0x002E  // PHY Configuration Register     ← ADDED
#define VERSIONR    0x0039  // Chip Version Register (read-only, should return 0x04)

#define Sn_MR           0x0000  // Socket Mode Register
#define Sn_CR           0x0001  // Socket Command Register
#define Sn_IR           0x0002  // Socket Interrupt Register
#define Sn_SR           0x0003  // Socket Status Register
#define Sn_PORT         0x0004  // Socket Source Port (2 bytes)
#define Sn_DHAR         0x0006  // Destination MAC (6 bytes)
#define Sn_DIPR         0x000C  // Destination IP (4 bytes)
#define Sn_DPORT        0x0010  // Destination Port (2 bytes)
#define Sn_MSSR         0x0012  // Max Segment Size (2 bytes)
#define Sn_TOS          0x0015  // IP Type of Service
#define Sn_TTL          0x0017  // IP Time to Live
#define Sn_RXBUF_SIZE   0x001E  // RX Buffer Size (KB: 0,1,2,4,8,16) ← ADDED
#define Sn_TXBUF_SIZE   0x001F  // TX Buffer Size (KB: 0,1,2,4,8,16) ← ADDED
#define Sn_TX_FSR       0x0020  // TX Free Size (2 bytes)
#define Sn_TX_RD        0x0022  // TX Read Pointer (2 bytes)
#define Sn_TX_WR        0x0024  // TX Write Pointer (2 bytes)
#define Sn_RX_RSR       0x0026  // RX Received Size (2 bytes)
#define Sn_RX_RD        0x0028  // RX Read Pointer (2 bytes)
#define Sn_RX_WR        0x002A  // RX Write Pointer (2 bytes, read-only)


uint8_t  WIZCHIP_READ    (uint16_t addr);
void     WIZCHIP_WRITE   (uint16_t addr, uint8_t data);
void     WIZCHIP_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len);
void     WIZCHIP_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len);


uint8_t  WIZCHIP_READ_B    (uint8_t block, uint16_t addr);
void     WIZCHIP_WRITE_B   (uint8_t block, uint16_t addr, uint8_t data);
void     WIZCHIP_READ_BUF_B(uint8_t block, uint16_t addr, uint8_t* buf, uint16_t len);
void     WIZCHIP_WRITE_BUF_B(uint8_t block, uint16_t addr, const uint8_t* buf, uint16_t len);


void     setRTR(uint16_t timeout);  // timeout in 100us units
uint16_t getRTR(void);
void     setRCR(uint8_t retry);
uint8_t  getRCR(void);


uint8_t  getSIR(void);

#endif // W5500_H