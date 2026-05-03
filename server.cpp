// Single-listener architecture with proper socket cleanup

#include <Arduino.h>
#include "server.h"
#include "client.h"
#include "socket.h"
#include "w5500.h"


#define MAX_SOCK_NUM 8

static uint16_t server_port = 0;
static uint8_t  server_sock = 0xFF;

static inline uint8_t r8(uint8_t sn, uint16_t off)
{
    return WIZCHIP_READ_B(W5500_SOCKET_REG(sn), off);
}

static inline void w8(uint8_t sn, uint16_t off, uint8_t v)
{
    WIZCHIP_WRITE_B(W5500_SOCKET_REG(sn), off, v);
}


// force_cleanup()
static void force_cleanup(uint8_t sn)
{
    uint8_t state = r8(sn, Sn_SR);
    if (state == SOCK_CLOSED) return;

    if (state == SOCK_ESTABLISHED || state == SOCK_CLOSE_WAIT) {
        WIZCHIP_WRITE_B(W5500_SOCKET_REG(sn), Sn_CR, Sn_CR_DISCON);
        uint32_t start = millis();
        while (r8(sn, Sn_SR) != SOCK_CLOSED) {
            if ((millis() - start) > 100) break;
        }
    }

    if (r8(sn, Sn_SR) != SOCK_CLOSED) {
        WIZCHIP_WRITE_B(W5500_SOCKET_REG(sn), Sn_CR, Sn_CR_CLOSE);
        uint32_t start = millis();
        while (r8(sn, Sn_SR) != SOCK_CLOSED) {
            if ((millis() - start) > 500) break;
        }
    }

    w8(sn, Sn_IR, 0xFF);
}

// sanity_sweep()
static void sanity_sweep(void)
{
    for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {
        if (s == server_sock) continue;
        uint8_t state = r8(s, Sn_SR);
        if (state != SOCK_CLOSED &&
            state != SOCK_LISTEN &&
            state != SOCK_ESTABLISHED) {
            WIZCHIP_WRITE_B(W5500_SOCKET_REG(s), Sn_CR, Sn_CR_CLOSE);
            w8(s, Sn_IR, 0xFF);
        }
    }
}


// arm_listener()
static uint8_t arm_listener(void)
{
    // Find a free socket if not yet assigned
    if (server_sock == 0xFF) {
        for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {
            if (r8(s, Sn_SR) == SOCK_CLOSED) {
                server_sock = s;
                break;
            }
        }
        if (server_sock == 0xFF) return 0;
    }

    uint8_t state = r8(server_sock, Sn_SR);

    // Already in a good state — do NOT touch it
    if (state == SOCK_LISTEN || state == SOCK_ESTABLISHED) return 1;

    // Only re-arm if truly closed — never force here
    if (state != SOCK_CLOSED) return 0;

    // Open socket in TCP mode on server_port
    if (socket(server_sock, Sn_MR_TCP, server_port, 0) != SOCK_OK) return 0;

    // Issue LISTEN
    if (listen(server_sock) != SOCK_OK) {
        force_cleanup(server_sock);
        return 0;
    }

    // Wait for LISTEN or ESTABLISHED (client may connect mid-sequence)
    uint32_t t = millis();
    while (1) {
        uint8_t sr = r8(server_sock, Sn_SR);
        if (sr == SOCK_LISTEN || sr == SOCK_ESTABLISHED) return 1;
        if ((millis() - t) > 500) break;
    }

    return 0;
}

extern "C" {


// EthernetServer_begin()
void EthernetServer_begin(uint16_t port)
{
    server_port = port;
    server_sock = 0xFF;

    sanity_sweep();

    for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {
        if (r8(s, Sn_SR) == SOCK_CLOSED) {
            server_sock = s;
            break;
        }
    }

    if (server_sock == 0xFF) return;

    arm_listener();
}


// EthernetServer_available()
uint8_t EthernetServer_available(void)
{
    if (server_sock == 0xFF) return 0xFF;

    uint8_t state = r8(server_sock, Sn_SR);

    // Return socket immediately on ESTABLISHED — don't wait for data
    if (state == SOCK_ESTABLISHED) return server_sock;

    // Re-arm if not listening
    if (state != SOCK_LISTEN) {
        arm_listener();
    }

    return 0xFF;
}


// EthernetServer_accept()
void EthernetServer_accept(void)
{
    if (server_sock == 0xFF) return;

    force_cleanup(server_sock);
    sanity_sweep();
    arm_listener();
}

// EthernetServer_prearm()
void EthernetServer_prearm(void)
{
    arm_listener();
}


// EthernetServer_hasClient()
uint8_t EthernetServer_hasClient(void)
{
    if (server_sock == 0xFF) return 0;
    return (r8(server_sock, Sn_SR) == SOCK_ESTABLISHED) ? 1 : 0;
}


// EthernetServer_stop()
void EthernetServer_stop(void)
{
    if (server_sock == 0xFF) return;
    force_cleanup(server_sock);
    server_sock = 0xFF;
    server_port = 0;
}


// Info helpers
uint16_t EthernetServer_getPort(void)
{
    return server_port;
}

uint8_t EthernetServer_getSocket(void)
{
    return server_sock;
}

uint8_t EthernetServer_isListening(void)
{
    if (server_sock == 0xFF) return 0;
    uint8_t state = r8(server_sock, Sn_SR);
    return (state == SOCK_LISTEN || state == SOCK_ESTABLISHED) ? 1 : 0;
}

} // extern "C"
