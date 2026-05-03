#ifndef ETHERNET_446_SERVER_H
#define ETHERNET_446_SERVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// Server Lifecycle


/**
 * @brief Opens a TCP socket and starts listening on the given port.
 *
 * Finds the first free W5500 socket, opens it in TCP mode, and issues
 * the LISTEN command. Safe to call again after EthernetServer_accept()
 * or after a connection drops — it will reuse the same port.
 *
 * @param port TCP port to listen on (e.g. 80 for HTTP)
 */
void EthernetServer_begin(uint16_t port);

/**
 * @brief Polls for an active client connection.
 *
 * Returns the socket number whenever a client is in SOCK_ESTABLISHED
 * state. Continues returning the same socket on every call until
 * EthernetServer_accept() is called to close it.
 *
 * Handles state transitions automatically:
 *   - SOCK_CLOSE_WAIT → closes socket, calls EthernetServer_begin()
 *   - SOCK_CLOSED     → calls EthernetServer_begin() to restart
 *
 * @return Socket number (0–7) while a client is connected, 0xFF otherwise.
 *
 * Typical main-loop pattern:
 * @code
 *   uint8_t client = EthernetServer_available();
 *   if (client != 0xFF) {
 *       // handle client: recv() / send()
 *       EthernetServer_accept(); // done — close and re-listen
 *   }
 * @endcode
 */
uint8_t EthernetServer_available(void);

/**
 * @brief Closes the current client connection and restarts listening.
 *
 * Call this after you have finished reading and writing to the client.
 * It performs a graceful disconnect if the socket is still ESTABLISHED,
 * then force-closes and calls EthernetServer_begin() to re-arm the
 * listener for the next incoming connection.
 */
void EthernetServer_accept(void);
void EthernetServer_prearm(void);

/**
 * @brief Stops the server completely and frees the socket.
 *
 * After this call, EthernetServer_available() will return 0xFF until
 * EthernetServer_begin() is called again.
 */
void EthernetServer_stop(void);


// Status Queries


/**
 * @brief Returns 1 if a client is currently in SOCK_ESTABLISHED state.
 * Non-destructive — does not change socket state.
 */
uint8_t EthernetServer_hasClient(void);

/**
 * @brief Returns the port the server is configured to listen on.
 * Returns 0 if EthernetServer_begin() has not been called.
 */
uint16_t EthernetServer_getPort(void);

/**
 * @brief Returns the W5500 socket number allocated to this server.
 * Returns 0xFF if no socket has been assigned.
 */
uint8_t EthernetServer_getSocket(void);

/**
 * @brief Returns 1 if the server socket is in LISTEN or ESTABLISHED state.
 * Useful for health checks / reconnect logic.
 */
uint8_t EthernetServer_isListening(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_446_SERVER_H