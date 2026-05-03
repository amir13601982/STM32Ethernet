
#include <Arduino.h>
#include "socket.h"
#include "w5500.h"
#include <string.h>




// Socket register access helpers

static inline uint8_t  r8  (uint8_t sn, uint16_t off)                              { return WIZCHIP_READ_B (W5500_SOCKET_REG(sn), off);           }
static inline void     w8  (uint8_t sn, uint16_t off, uint8_t v)                   {        WIZCHIP_WRITE_B(W5500_SOCKET_REG(sn), off, v);         }
static inline void     rbuf(uint8_t sn, uint16_t off, uint8_t* b, uint16_t l)      {        WIZCHIP_READ_BUF_B (W5500_SOCKET_REG(sn), off, b, l);  }
static inline void     wbuf(uint8_t sn, uint16_t off, const uint8_t* b, uint16_t l){        WIZCHIP_WRITE_BUF_B(W5500_SOCKET_REG(sn), off, b, l);  }

// 16-bit register helpers (big-endian on wire)
static inline uint16_t r16(uint8_t sn, uint16_t off)
{
    return ((uint16_t)r8(sn, off) << 8) | r8(sn, off + 1);
}
static inline void w16(uint8_t sn, uint16_t off, uint16_t v)
{
    w8(sn, off,     (uint8_t)(v >> 8));
    w8(sn, off + 1, (uint8_t)(v     ));
}

// Write command register and wait for chip to clear it (command accepted)
static inline void cmd(uint8_t sn, uint8_t c)
{
    w8(sn, Sn_CR, c);
    uint32_t start = millis();
    while (r8(sn, Sn_CR)) {
        if ((millis() - start) > 100) break; // safety — should clear in <1ms
    }
}


// Buffer size mask helper
static uint16_t tx_buf_mask(uint8_t sn)
{
    uint8_t kb = r8(sn, Sn_TXBUF_SIZE);
    if (kb == 0) kb = 2; // default to 2KB if not configured
    return (uint16_t)((uint32_t)kb * 1024 - 1);
}

static uint16_t rx_buf_mask(uint8_t sn)
{
    uint8_t kb = r8(sn, Sn_RXBUF_SIZE);
    if (kb == 0) kb = 2;
    return (uint16_t)((uint32_t)kb * 1024 - 1);
}

static uint16_t get_tx_fsr(uint8_t sn)
{
    uint16_t v1, v2;
    do {
        v1 = r16(sn, Sn_TX_FSR);
        v2 = r16(sn, Sn_TX_FSR);
    } while (v1 != v2);
    return v1;
}

static uint16_t get_rx_rsr(uint8_t sn)
{
    uint16_t v1, v2;
    do {
        v1 = r16(sn, Sn_RX_RSR);
        v2 = r16(sn, Sn_RX_RSR);
    } while (v1 != v2);
    return v1;
}

static uint16_t s_ephemeral_port = 49152;


// socket()

int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag)
{
    if (sn >= 8) return SOCKERR_SOCKNUM;

    // Force-close if not already closed, then clear all interrupt flags
    if (r8(sn, Sn_SR) != SOCK_CLOSED) {
    cmd(sn, Sn_CR_CLOSE);
    w8(sn, Sn_IR, 0xFF);
    uint32_t t = millis();
    while (r8(sn, Sn_SR) != SOCK_CLOSED) {
        if ((millis() - t) > 500) break;
    }
}

    // Set mode: protocol bits in [3:0], optional flag bits in upper nibble
    w8(sn, Sn_MR, protocol | (flag & 0xF0));

    // Assign source port
    if (port != 0) {
        w16(sn, Sn_PORT, port);
    } else {
        // Auto-assign from ephemeral range 49152–65534
        w16(sn, Sn_PORT, s_ephemeral_port++);
        if (s_ephemeral_port == 0) s_ephemeral_port = 49152; // wrap guard
    }

    cmd(sn, Sn_CR_OPEN);

    // Wait for SOCK_INIT (TCP) or SOCK_UDP — use protocol to decide expected state
    uint8_t expected = (protocol == Sn_MR_TCP) ? SOCK_INIT : SOCK_UDP;
    uint32_t start = millis();
    while (r8(sn, Sn_SR) != expected) {
        if ((millis() - start) > 100) {
            cmd(sn, Sn_CR_CLOSE);
            w8(sn, Sn_IR, 0xFF);
            return SOCKERR_TIMEOUT;
        }
    }

    return SOCK_OK;
}


// close()

int8_t close(uint8_t sn)
{
    cmd(sn, Sn_CR_CLOSE);
    w8(sn, Sn_IR, 0xFF);
    
    uint32_t start = millis();
    while (r8(sn, Sn_SR) != SOCK_CLOSED) {
        if ((millis() - start) > 1000) return SOCKERR_TIMEOUT;
    }
    return SOCK_OK;
}


// listen()

int8_t listen(uint8_t sn)
{   
    if ((r8(sn, Sn_MR) & 0x0F) != Sn_MR_TCP) return SOCKERR_SOCKMODE;
    if (r8(sn, Sn_SR) != SOCK_INIT)            return SOCKERR_SOCKINIT;

    cmd(sn, Sn_CR_LISTEN);

    uint32_t start = millis();
    while (r8(sn, Sn_SR) != SOCK_LISTEN) {
        if ((millis() - start) > 100) return SOCKERR_TIMEOUT;
    }
    return SOCK_OK;
}


// connect()

int8_t connect(uint8_t sn, const uint8_t* addr, uint16_t port)
{
    if ((r8(sn, Sn_MR) & 0x0F) != Sn_MR_TCP) return SOCKERR_SOCKMODE;

    WIZCHIP_WRITE_BUF_B(W5500_SOCKET_REG(sn), Sn_DIPR, addr, 4);
    w16(sn, Sn_DPORT, port);

    cmd(sn, Sn_CR_CONNECT);

    // Budget = (RCR+1) retries × RTR (100µs units) → convert to ms (÷10)
    uint32_t budget_ms = ((uint32_t)(getRCR() + 1) * (uint32_t)getRTR()) / 10;
    if (budget_ms < 1000) budget_ms = 1000;

    uint32_t start = millis();
    while ((millis() - start) < budget_ms) {
        uint8_t sr = r8(sn, Sn_SR);
        if (sr == SOCK_ESTABLISHED) return SOCK_OK;

        uint8_t ir = r8(sn, Sn_IR);
        if (ir & 0x08) {                    // Sn_IR_TIMEOUT
            w8(sn, Sn_IR, 0x08);
            return SOCKERR_TIMEOUT;
        }
    }
    return SOCKERR_TIMEOUT;
}


// disconnect()

int8_t disconnect(uint8_t sn)
{
    cmd(sn, Sn_CR_DISCON);

    uint8_t close_sent = 0;   // ADD THIS
    uint32_t start = millis();
    while ((millis() - start) < 3000) {
        uint8_t sr = r8(sn, Sn_SR);
        if (sr == SOCK_CLOSED) {
            w8(sn, Sn_IR, 0xFF);
            return SOCK_OK;
        }
        if (sr == SOCK_CLOSE_WAIT && !close_sent) {  // CHANGE THIS
            cmd(sn, Sn_CR_CLOSE);
            close_sent = 1;
        }
    }
    w8(sn, Sn_IR, 0xFF);
    return SOCKERR_TIMEOUT;
}


// send()

int32_t send(uint8_t sn, const uint8_t* buf, uint16_t len)
{
    if (len == 0) return SOCKERR_WRONGARG;

    // FIXED 1: Verify socket is still connected before touching TX buffer
    uint8_t sr = r8(sn, Sn_SR);
    if (sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT)
        return SOCKERR_SOCKCLOSED;

    // FIXED 3: Wait for free TX space using stable double-read
    uint32_t start = millis();
    while (get_tx_fsr(sn) < len) {
        if ((millis() - start) > 2000) return SOCKERR_TIMEOUT;
    }

    // FIXED 2: Mask pointer and handle buffer wraparound
    uint16_t mask         = tx_buf_mask(sn);
    uint16_t wr           = r16(sn, Sn_TX_WR);
    uint16_t offset       = wr & mask;
    uint16_t buf_end      = mask + 1;
    uint16_t space_to_end = buf_end - offset;

    if (len <= space_to_end) {
        // Fits without wrapping
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), offset, buf, len);
    } else {
        // Split across buffer wrap boundary
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), offset,       buf,                space_to_end);
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), 0,            buf + space_to_end, len - space_to_end);
    }

    // Advance the write pointer and kick SEND
    w16(sn, Sn_TX_WR, wr + len);
    cmd(sn, Sn_CR_SEND);

    // Wait for SENDOK (bit 4) or TIMEOUT (bit 3) interrupt
    start = millis();
    while ((millis() - start) < 2000) {
        uint8_t ir = r8(sn, Sn_IR);
        if (ir & 0x10) { w8(sn, Sn_IR, 0x10); return (int32_t)len; }   // SENDOK
        if (ir & 0x08) { w8(sn, Sn_IR, 0x08); return SOCKERR_TIMEOUT; } // TIMEOUT
    }
    return SOCKERR_TIMEOUT;
}


// recv()
int32_t recv(uint8_t sn, uint8_t* buf, uint16_t len)
{
    if (len == 0) return 0;

    // FIXED 2+3: Wait for data using stable double-read
    uint32_t start = millis();
    uint16_t recvsz;
    do {
        recvsz = get_rx_rsr(sn);
        if ((millis() - start) > 2000) return SOCKERR_TIMEOUT;
    } while (recvsz == 0);

    if (recvsz < len) len = recvsz;

    // FIXED 1: Mask pointer and handle buffer wraparound
    uint16_t mask         = rx_buf_mask(sn);
    uint16_t rd           = r16(sn, Sn_RX_RD);
    uint16_t offset       = rd & mask;
    uint16_t buf_end      = mask + 1;
    uint16_t space_to_end = buf_end - offset;

    if (len <= space_to_end) {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), offset, buf, len);
    } else {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), offset,       buf,                space_to_end);
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), 0,            buf + space_to_end, len - space_to_end);
    }

    // Advance read pointer and send RECV command to free buffer space
    w16(sn, Sn_RX_RD, rd + len);
    cmd(sn, Sn_CR_RECV);

    return (int32_t)len;
}


// sendto() — UDP transmit
int32_t sendto(uint8_t sn, const uint8_t* buf, uint16_t len,
               const uint8_t* addr, uint16_t port)
{
    if (len == 0) return SOCKERR_WRONGARG;

    WIZCHIP_WRITE_BUF_B(W5500_SOCKET_REG(sn), Sn_DIPR, addr, 4);
    w16(sn, Sn_DPORT, port);

    // FIXED: double-read for stable free-space value
    uint32_t start = millis();
    while (get_tx_fsr(sn) < len) {
        if ((millis() - start) > 2000) return SOCKERR_TIMEOUT;
    }

    uint16_t mask         = tx_buf_mask(sn);
    uint16_t wr           = r16(sn, Sn_TX_WR);
    uint16_t offset       = wr & mask;
    uint16_t buf_end      = mask + 1;
    uint16_t space_to_end = buf_end - offset;

    if (len <= space_to_end) {
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), offset, buf, len);
    } else {
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), offset,       buf,                space_to_end);
        WIZCHIP_WRITE_BUF_B(W5500_TX_BUFFER(sn), 0,            buf + space_to_end, len - space_to_end);
    }

    w16(sn, Sn_TX_WR, wr + len);
    cmd(sn, Sn_CR_SEND);

    start = millis();
    while ((millis() - start) < 2000) {
        uint8_t ir = r8(sn, Sn_IR);
        if (ir & 0x10) { w8(sn, Sn_IR, 0x10); return (int32_t)len; }
        if (ir & 0x08) { w8(sn, Sn_IR, 0x08); return SOCKERR_TIMEOUT; }
    }
    return SOCKERR_TIMEOUT;
}


// recvfrom() — UDP receive
int32_t recvfrom(uint8_t sn, uint8_t* buf, uint16_t len,
                 uint8_t* addr, uint16_t* port)
{
    if (len == 0) return 0;

    // FIXED: double-read, wait for at least the 8-byte UDP header
    uint32_t start = millis();
    uint16_t recvsz;
    do {
        recvsz = get_rx_rsr(sn);
        if ((millis() - start) > 2000) return SOCKERR_TIMEOUT;
    } while (recvsz < 8);

    uint16_t mask    = rx_buf_mask(sn);
    uint16_t rd      = r16(sn, Sn_RX_RD);
    uint16_t buf_end = mask + 1;

    // Read 8-byte UDP info header: [SRC_IP(4)][SRC_PORT(2)][DATA_LEN(2)]
    uint8_t head[8];
    uint16_t h_offset = rd & mask;
    uint16_t h_space  = buf_end - h_offset;
    if (8 <= h_space) {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), h_offset, head, 8);
    } else {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), h_offset, head,           h_space);
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), 0,        head + h_space, 8 - h_space);
    }
    rd += 8;

    if (addr) {
        addr[0] = head[0]; addr[1] = head[1];
        addr[2] = head[2]; addr[3] = head[3];
    }
    if (port) {
        *port = ((uint16_t)head[4] << 8) | head[5];
    }

    uint16_t dlen = ((uint16_t)head[6] << 8) | head[7];
    if (dlen < len) len = dlen;

    // Read payload with wraparound handling
    uint16_t p_offset     = rd & mask;
    uint16_t p_space      = buf_end - p_offset;

    if (len <= p_space) {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), p_offset, buf, len);
    } else {
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), p_offset,     buf,           p_space);
        WIZCHIP_READ_BUF_B(W5500_RX_BUFFER(sn), 0,            buf + p_space, len - p_space);
    }

    // Advance by full datagram length (not truncated len) to discard remainder
    w16(sn, Sn_RX_RD, rd + dlen);
    cmd(sn, Sn_CR_RECV);

    return (int32_t)len;
}
