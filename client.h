#ifndef ETHERNET_446_CLIENT_H
#define ETHERNET_446_CLIENT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


// Initialisation & Connection

/**
 * @brief Initialises a W5500 socket for use as a TCP client.
 *
 * Closes the socket if already open, then opens it in TCP mode
 * with an auto-assigned local port.
 *
 * @param socket_num Socket number to use (0–7)
 * @return 1 on success, 0 on failure
 */
uint8_t EthernetClient_begin(uint8_t socket_num);

/**
 * @brief Connects to a remote TCP server.
 *
 * The socket must be in SOCK_INIT state (call EthernetClient_begin()
 * first). Blocks until SOCK_ESTABLISHED or timeout. The timeout
 * duration is derived from the W5500 RTR/RCR registers (at least 1s).
 *
 * @param socket_num Socket number (0–7)
 * @param ip         4-byte array: destination IP address
 * @param port       Destination TCP port
 * @return 1 on success, 0 on failure or timeout
 *
 * @code
 * uint8_t ip[] = {192, 168, 1, 100};
 * EthernetClient_begin(0);
 * if (EthernetClient_connect(0, ip, 80)) {
 *     // connected
 * }
 * @endcode
 */
uint8_t EthernetClient_connect(uint8_t socket_num, const uint8_t* ip, uint16_t port);


// Data Transmission


/**
 * @brief Sends a single byte over an established TCP connection.
 * @return 1 on success, 0 on failure
 */
size_t EthernetClient_write(uint8_t socket_num, uint8_t data);

/**
 * @brief Sends a byte array over an established TCP connection.
 *
 * Waits up to 2 seconds for the full requested TX buffer space to
 * become available before sending. Does NOT silently truncate —
 * returns 0 if space does not become available in time.
 *
 * @param socket_num Socket number (0–7)
 * @param buf        Data to send
 * @param size       Number of bytes to send
 * @return Bytes actually sent, or 0 on failure/timeout
 *
 * @code
 * const char* req = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
 * EthernetClient_write_array(0, (const uint8_t*)req, strlen(req));
 * @endcode
 */
size_t EthernetClient_write_array(uint8_t socket_num, const uint8_t* buf, size_t size);


// Data Reception

/**
 * @brief Reads up to `size` bytes from the RX buffer into `buf`.
 *
 * Returns immediately with however many bytes are available —
 * does not block waiting for more data. Check EthernetClient_available()
 * first if you need a specific amount.
 *
 * @param socket_num Socket number (0–7)
 * @param buf        Buffer to receive data into
 * @param size       Maximum bytes to read
 * @return Bytes actually read, or 0 if no data available
 *
 * @code
 * uint8_t buf[256];
 * int avail = EthernetClient_available(0);
 * if (avail > 0) {
 *     size_t n = EthernetClient_read_array(0, buf, sizeof(buf));
 *     // process buf[0..n-1]
 * }
 * @endcode
 */
size_t EthernetClient_read_array(uint8_t socket_num, uint8_t* buf, size_t size);

/**
 * @brief Returns the number of bytes waiting in the RX buffer.
 *
 * @param socket_num Socket number (0–7)
 * @return Byte count (≥ 0), or -1 if the socket is closed
 */
int EthernetClient_available(uint8_t socket_num);

/**
 * @brief Reads and returns a single byte from the RX buffer.
 * @return Byte value (0–255), or -1 if no data or error
 */
int EthernetClient_read(uint8_t socket_num);


// Connection Management


/**
 * @brief Gracefully closes the TCP connection, then force-closes if needed.
 *
 * Attempts a graceful FIN handshake (up to 500ms), then force-closes
 * the socket if the remote side has not responded. Safe to call even
 * if the socket is already closed.
 *
 * @param socket_num Socket number (0–7)
 */
void EthernetClient_stop(uint8_t socket_num);

/**
 * @brief Returns the raw W5500 socket status register value.
 *
 * Useful for debugging. Common values:
 *   SOCK_CLOSED      (0x00)
 *   SOCK_INIT        (0x13)
 *   SOCK_ESTABLISHED (0x17)
 *   SOCK_CLOSE_WAIT  (0x1C)
 *
 * @param socket_num Socket number (0–7)
 * @return Sn_SR register value
 */
uint8_t EthernetClient_status(uint8_t socket_num);

/**
 * @brief Returns 1 if the socket is in SOCK_ESTABLISHED state.
 * @param socket_num Socket number (0–7)
 * @return 1 if connected, 0 otherwise
 */
uint8_t EthernetClient_connected(uint8_t socket_num);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_446_CLIENT_H