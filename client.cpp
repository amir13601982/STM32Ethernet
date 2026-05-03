// =======================================================
// ethernet_446/client.cpp (Fixed version)
// =======================================================
#include <Arduino.h>
#include "client.h"
#include "socket.h"
#include "w5500.h"

// Socket register access helpers
static inline uint8_t r8(uint8_t sn, uint16_t off)
{
    return WIZCHIP_READ_B(W5500_SOCKET_REG(sn), off);
}

static inline uint16_t r16(uint8_t sn, uint16_t off)
{
    return ((uint16_t)r8(sn, off) << 8) | r8(sn, off + 1);
}

static inline void w8(uint8_t sn, uint16_t off, uint8_t v)
{
    WIZCHIP_WRITE_B(W5500_SOCKET_REG(sn), off, v);
}

static uint16_t stable_tx_fsr(uint8_t sn)
{
    uint16_t v1, v2;
    do { v1 = r16(sn, Sn_TX_FSR); v2 = r16(sn, Sn_TX_FSR); } while (v1 != v2);
    return v1;
}

static uint16_t stable_rx_rsr(uint8_t sn)
{
    uint16_t v1, v2;
    do { v1 = r16(sn, Sn_RX_RSR); v2 = r16(sn, Sn_RX_RSR); } while (v1 != v2);
    return v1;
}

extern "C" {


// EthernetClient_begin()

uint8_t EthernetClient_begin(uint8_t socket_num)
{
    if (socket_num >= 8) return 0;

    // Close if not already closed
    if (r8(socket_num, Sn_SR) != SOCK_CLOSED) {
        close(socket_num);
    }

    // Open as TCP with auto-assigned local port
    if (socket(socket_num, Sn_MR_TCP, 0, 0) != SOCK_OK) return 0;

    return (r8(socket_num, Sn_SR) == SOCK_INIT) ? 1 : 0;
}


// EthernetClient_connect()
uint8_t EthernetClient_connect(uint8_t socket_num, const uint8_t* ip, uint16_t port)
{
    if (socket_num >= 8 || ip == NULL) return 0;

    // Ensure socket is in INIT state
    if (r8(socket_num, Sn_SR) != SOCK_INIT) {
        if (!EthernetClient_begin(socket_num)) return 0;
    }

    int8_t result = connect(socket_num, ip, port);

    if (result != SOCK_OK) {
        
        w8(socket_num, Sn_IR, 0xFF);
        return 0;
    }

    return 1;
}


// EthernetClient_write()  — single byte

size_t EthernetClient_write(uint8_t socket_num, uint8_t data)
{
    return EthernetClient_write_array(socket_num, &data, 1);
}


// EthernetClient_write_array()

size_t EthernetClient_write_array(uint8_t socket_num, const uint8_t* buf, size_t size)
{
    if (socket_num >= 8 || buf == NULL || size == 0) return 0;

    if (r8(socket_num, Sn_SR) != SOCK_ESTABLISHED) return 0;

    // Wait for the full requested space — do not truncate silently
    uint32_t start = millis();
    while (stable_tx_fsr(socket_num) < (uint16_t)size) {
        if ((millis() - start) > 2000) return 0;
    }

    int32_t result = send(socket_num, buf, (uint16_t)size);
    return (result < 0) ? 0 : (size_t)result;
}


// EthernetClient_read_array()

size_t EthernetClient_read_array(uint8_t socket_num, uint8_t* buf, size_t size)
{
    if (socket_num >= 8 || buf == NULL || size == 0) return 0;

    uint16_t available = stable_rx_rsr(socket_num);
    if (available == 0) return 0;

    if ((uint16_t)size > available) size = available;

    int32_t result = recv(socket_num, buf, (uint16_t)size);
    return (result < 0) ? 0 : (size_t)result;
}


// EthernetClient_available()

int EthernetClient_available(uint8_t socket_num)
{
    if (socket_num >= 8) return -1;
    if (r8(socket_num, Sn_SR) == SOCK_CLOSED) return -1;
    return (int)stable_rx_rsr(socket_num);
}


// EthernetClient_read()  — single byte

int EthernetClient_read(uint8_t socket_num)
{
    if (socket_num >= 8) return -1;
    uint8_t b;
    return (EthernetClient_read_array(socket_num, &b, 1) == 1) ? (int)b : -1;
}


// EthernetClient_stop()

void EthernetClient_stop(uint8_t socket_num)
{
    if (socket_num >= 8) return;

    uint8_t state = r8(socket_num, Sn_SR);

    if (state == SOCK_ESTABLISHED || state == SOCK_CLOSE_WAIT) {
        disconnect(socket_num);  // already blocks until CLOSED or 3000ms timeout
    }

    // Fresh read — disconnect() may have already closed it
    if (r8(socket_num, Sn_SR) != SOCK_CLOSED) {
        close(socket_num);
    }
}


// EthernetClient_status()

uint8_t EthernetClient_status(uint8_t socket_num)
{
    if (socket_num >= 8) return SOCK_CLOSED;
    return r8(socket_num, Sn_SR);
}


// EthernetClient_connected()

uint8_t EthernetClient_connected(uint8_t socket_num)
{
    if (socket_num >= 8) return 0;
    uint8_t sr = r8(socket_num, Sn_SR);
    if (sr == SOCK_ESTABLISHED) return 1;
    if (sr == SOCK_CLOSE_WAIT) return (stable_rx_rsr(socket_num) > 0) ? 1 : 0;
    return 0;
}

} // extern "C"