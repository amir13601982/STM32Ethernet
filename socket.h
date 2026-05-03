#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>


// Socket Mode Register (Sn_MR) values

#define Sn_MR_CLOSE   0x00   // Unused socket
#define Sn_MR_TCP     0x01   // TCP mode
#define Sn_MR_UDP     0x02   // UDP mode
#define Sn_MR_MACRAW  0x04   // MAC RAW mode (socket 0 only)
#define Sn_MR_ND      0x20   // No-Delay ACK flag (TCP only, upper nibble)


// Socket Command Register (Sn_CR) values

#define Sn_CR_OPEN       0x01
#define Sn_CR_LISTEN     0x02
#define Sn_CR_CONNECT    0x04
#define Sn_CR_DISCON     0x08
#define Sn_CR_CLOSE      0x10
#define Sn_CR_SEND       0x20
#define Sn_CR_SEND_KEEP  0x22
#define Sn_CR_RECV       0x40


// Socket Status Register (Sn_SR) values

#define SOCK_CLOSED      0x00
#define SOCK_INIT        0x13
#define SOCK_LISTEN      0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT  0x1C
#define SOCK_UDP         0x22


// Return / Error Codes

#define SOCK_OK              ( 1)
#define SOCKERR_SOCKNUM      (-1)   // Invalid socket number
#define SOCKERR_SOCKMODE     (-2)   // Wrong socket mode for operation
#define SOCKERR_SOCKINIT     (-3)   // Socket not in INIT state
#define SOCKERR_SOCKCLOSED   (-4)   // Socket is closed
#define SOCKERR_TIMEOUT      (-5)   // Operation timed out
#define SOCKERR_WRONGARG     (-6)   // Bad argument (e.g. zero-length buffer)
#define SOCKERR_BUSY         (-7)   // Socket busy
#define SOCKERR_SENDBUFFER   (-8)   // TX buffer full
#define SOCKERR_RECEIVE      (-9)   // RX error

#ifdef __cplusplus
extern "C" {
#endif


// Socket API


/**
 * @brief Opens a socket in the specified mode.
 * @param sn       Socket number (0–7)
 * @param protocol Sn_MR_TCP or Sn_MR_UDP
 * @param port     Local port number (0 = auto-assign)
 * @param flag     Optional flags, e.g. Sn_MR_ND for no-delay TCP
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);

/**
 * @brief Closes a socket and clears all interrupt flags.
 * @return SOCK_OK on success, SOCKERR_TIMEOUT if chip does not respond
 */
int8_t close(uint8_t sn);

/**
 * @brief Puts a TCP socket (must be SOCK_INIT) into listen mode.
 * @return SOCK_OK, SOCKERR_SOCKMODE, SOCKERR_SOCKINIT, or SOCKERR_TIMEOUT
 */
int8_t listen(uint8_t sn);

/**
 * @brief Initiates a TCP connection to a remote host.
 * Blocks until SOCK_ESTABLISHED or timeout.
 * @param sn   Socket number
 * @param addr 4-byte destination IP
 * @param port Destination port
 * @return SOCK_OK or SOCKERR_TIMEOUT
 */
int8_t connect(uint8_t sn, const uint8_t* addr, uint16_t port);

/**
 * @brief Sends TCP FIN to remote host. Blocks until CLOSE_WAIT or CLOSED.
 * @return SOCK_OK or SOCKERR_TIMEOUT
 */
int8_t disconnect(uint8_t sn);

/**
 * @brief Sends data over an established TCP connection.
 * Handles TX buffer wraparound internally.
 * @return Bytes sent (== len) on success, negative error code on failure
 */
int32_t send(uint8_t sn, const uint8_t* buf, uint16_t len);

/**
 * @brief Receives data from an established TCP connection.
 * Handles RX buffer wraparound internally.
 * @return Bytes received (> 0) on success, SOCKERR_TIMEOUT on timeout
 */
int32_t recv(uint8_t sn, uint8_t* buf, uint16_t len);

/**
 * @brief Sends a UDP datagram to the specified address and port.
 * @return Bytes sent on success, negative error code on failure
 */
int32_t sendto(uint8_t sn, const uint8_t* buf, uint16_t len,
               const uint8_t* addr, uint16_t port);

/**
 * @brief Receives a UDP datagram, extracting source IP and port from header.
 * @return Payload bytes received on success, SOCKERR_TIMEOUT on timeout
 */
int32_t recvfrom(uint8_t sn, uint8_t* buf, uint16_t len,
                 uint8_t* addr, uint16_t* port);

#ifdef __cplusplus
}
#endif

#endif // SOCKET_H