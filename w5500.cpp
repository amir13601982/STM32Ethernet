#include <Arduino.h>    
#include <SPI.h>       
#include <string.h>    
#include "w5500.h"

#define W5500_CS_PIN  PA4    // ← change to match your wiring

static inline void wiz_sel()
{
    digitalWrite(W5500_CS_PIN, LOW);
}

static inline void wiz_desel()
{
    digitalWrite(W5500_CS_PIN, HIGH);
}

static inline uint8_t _read_u8(uint16_t addr, uint8_t bsb)
{
    wiz_sel();
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer( addr       & 0xFF);
    SPI.transfer((bsb << 3) | 0x00);   // READ | VDM
    uint8_t r = SPI.transfer(0x00);
    wiz_desel();
    return r;
}

static inline void _write_u8(uint16_t addr, uint8_t bsb, uint8_t data)
{
    wiz_sel();
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer( addr       & 0xFF);
    SPI.transfer((bsb << 3) | 0x04);   // WRITE | VDM
    SPI.transfer(data);
    wiz_desel();
}

static inline void _read_buf(uint16_t addr, uint8_t bsb,
                              uint8_t* buf, uint16_t len)
{
    wiz_sel();
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer( addr       & 0xFF);
    SPI.transfer((bsb << 3) | 0x00);   // READ | VDM
    for (uint16_t i = 0; i < len; i++) buf[i] = SPI.transfer(0xFF);
    wiz_desel();
}

static inline void _write_buf(uint16_t addr, uint8_t bsb,
                               const uint8_t* buf, uint16_t len)
{
    wiz_sel();
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer( addr       & 0xFF);
    SPI.transfer((bsb << 3) | 0x04);   // WRITE | VDM
    for (uint16_t i = 0; i < len; i++) SPI.transfer(buf[i]);
    wiz_desel();
}


// Common-block convenience wrappers

uint8_t WIZCHIP_READ(uint16_t addr)
    { return _read_u8(addr, W5500_COMMON_REG); }

void WIZCHIP_WRITE(uint16_t addr, uint8_t data)
    { _write_u8(addr, W5500_COMMON_REG, data); }

void WIZCHIP_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len)
    { _read_buf(addr, W5500_COMMON_REG, buf, len); }

void WIZCHIP_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len)
    { _write_buf(addr, W5500_COMMON_REG, buf, len); }


// Block-aware public API (socket layer uses these)

uint8_t WIZCHIP_READ_B(uint8_t block, uint16_t addr)
    { return _read_u8(addr, block); }

void WIZCHIP_WRITE_B(uint8_t block, uint16_t addr, uint8_t data)
    { _write_u8(addr, block, data); }

void WIZCHIP_READ_BUF_B(uint8_t block, uint16_t addr, uint8_t* buf, uint16_t len)
    { _read_buf(addr, block, buf, len); }

void WIZCHIP_WRITE_BUF_B(uint8_t block, uint16_t addr, const uint8_t* buf, uint16_t len)
    { _write_buf(addr, block, buf, len); }


// Timeout & Retry — RTR is a 2-byte big-endian register

void setRTR(uint16_t timeout)
{
    WIZCHIP_WRITE(RTR,     (timeout >> 8) & 0xFF);  // High byte at RTR   (0x0019)
    WIZCHIP_WRITE(RTR + 1, (timeout     ) & 0xFF);  // Low byte  at RTR+1 (0x001A)
}

uint16_t getRTR(void)
{
    return ((uint16_t)WIZCHIP_READ(RTR) << 8) | WIZCHIP_READ(RTR + 1);
}

void setRCR(uint8_t retry)
{
    WIZCHIP_WRITE(RCR, retry);
}

uint8_t getRCR(void)
{
    return WIZCHIP_READ(RCR);
}


// Socket Interrupt Register

uint8_t getSIR(void)
{
    return WIZCHIP_READ(SIR);
}