# W5500Ethernet

A lightweight, production-tested W5500 Ethernet library for STM32 microcontrollers, compatible with both custom bare-metal cores and the official STM32duino Arduino core.

Built and tested on STM32F446RE (Nucleo-64) with a WIZ550io module. Includes Modbus TCP slave and master out of the box.

---

## Why This Library Exists

Getting Ethernet working reliably on STM32 with the Arduino ecosystem is harder than it should be. Here is an honest summary of what exists and why this library was built.

### The Official STM32duino Ethernet Library

The [official STM32duino STM32Ethernet library](https://github.com/stm32duino/STM32Ethernet) uses the **built-in hardware Ethernet MAC + LAN8742A PHY** available on Nucleo-144 boards (F429ZI, F767ZI, F746ZG etc). It requires LwIP as a dependency and does not support external SPI Ethernet modules like the W5500 or WIZ550io at all. If you have a Nucleo-64 (F446RE, F401RE, F411RE) or any custom STM32 board without a built-in MAC, this library simply does not apply. The library also has well-documented compilation issues — multiple open GitHub issues show `LAN8742A_PHY_ADDRESS` and `PHY_IMR` symbol errors across different Arduino IDE and STM32duino core versions.

### Third-Party W5500 Libraries for STM32duino

Several community libraries exist but all have significant problems in practice:

- **Ethernet_STM32** (stevstrong): Targets the older Maple/Roger Clark core, not the current STM32duino core. Requires manual SPI class declaration and has not been maintained for the modern STM32duino toolchain.

- **Ethernet_STM** (Serasidis): Originally written for STM32F103 with the old Arduino_STM32 core. Requires editing header files (`w5100.h`) to select the chip type. Not compatible with current STM32duino without significant modifications.

- **EthernetWebServer_STM32** (khoih-prog): A large framework library that pulls in multiple dependencies. Its W5500 support goes through `Ethernet_Generic` as an intermediary. Heavy for embedded use and primarily targets higher-end boards.

- **Arduino Ethernet library** (official): Works well on AVR and SAMD but has known SPI timing issues on STM32 at higher clock speeds. Does not handle the STM32 APB clock prescaler differences that affect SPI speed calculations.

In practice, none of these libraries compile and run out of the box on a Nucleo-64 (STM32F446RE) with STM32duino core and a W5500 module without non-trivial modifications. This library was written from scratch after exhausting all existing options.

### What This Library Does Differently

- Written specifically for STM32 + W5500 on the STM32duino core
- Correct BSB (Block Select Bits) SPI framing — the most common source of silent W5500 failures on STM32
- Zero dynamic memory allocation — no `new`, no `malloc`, no heap
- Built-in Modbus TCP slave and master — no additional libraries needed
- Tested end-to-end on real hardware with Modbus Poll and Modbus Slave simulators
- Single file include: `#include <STM32Ethernet.h>`

---

## Features

- TCP server — single-listener with automatic re-arm on disconnect
- TCP client — persistent connection with auto-reconnect
- Modbus TCP slave (FC03 Read Holding Registers, FC16 Write Multiple Registers)
- Modbus TCP master with persistent connection
- W5500 register layer with correct BSB-aware SPI framing
- Socket state machine (open, listen, connect, send, recv, close)
- PHY link status detection
- Static IP configuration

## Limitations/To Do

This is a focused library for industrial embedded systems. It intentionally does **not** include:

- **DHCP** — static IP only, no dynamic address assignment
- **DNS** — use IP addresses directly
- **UDP** — TCP only
- **HTTPS / TLS** — plain TCP, no encryption
- **Multi-client server** — one active connection at a time per port
- **Standard Arduino `Stream` API** — uses explicit socket-number functions instead

If you need DHCP, DNS, or Arduino `Ethernet`/`EthernetClient` stream compatibility, this library is not the right tool. For industrial Modbus TCP on STM32, it is.

---

## Design Philosophy

### Why `extern "C"` and not a C++ class API?

All public functions use C linkage (`extern "C"`). This is intentional:

- **Zero heap allocation** — no constructors, no vtables, no `new`/`delete`
- **Deterministic** — no hidden virtual dispatch, no dynamic object lifetime
- **Portable** — callable from C and C++ without name mangling issues
- **Debuggable** — every socket operation is explicit and traceable

The `.cpp` file extension is required by the Arduino build system for files that include `<Arduino.h>`. The code itself is pure C style.

### Why not the standard Arduino `EthernetClient` / `EthernetServer` API?

The standard Arduino networking API inherits from `Stream` and `Client` base classes, which introduce virtual function tables, hidden heap usage, and dependency on the Arduino object model. For a bare-metal Modbus TCP gateway where you control exactly one protocol and one socket, the explicit API (`EthernetClient_write_array(sock, buf, len)`) is clearer, faster, and produces smaller binary output.

A thin C++ wrapper implementing `EthernetClient` and `EthernetServer` as Arduino-compatible classes could be added on top of this library for ArduinoModbus compatibility — that is a planned future addition.

---

## Hardware

| Signal | Nucleo-64 Pin | WIZ550io Pin |
|--------|--------------|--------------|
| SCK    | PA5          | SCLK         |
| MISO   | PA6          | MISO         |
| MOSI   | PA7          | MOSI         |
| CS     | PA4          | SCSn         |
| 3.3V   | CN6-4        | 3.3V         |
| GND    | CN6-6        | GND          |

> **Critical:** Keep SPI wires under 10cm. Long unshielded wires at 11MHz SPI cause signal integrity issues that make the W5500 socket state machine silently reject OPEN commands — even though all register reads and writes succeed. This is an extremely difficult bug to diagnose. Short wires eliminate it entirely.

> **Nucleo-64 HSE note:** When running from ST-Link USB only (no external power), the HSE clock comes from the ST-Link MCO pin. If you use a custom bare-metal core, set `HSEBYP` in `RCC_CR` before `HSEON`. With STM32duino this is handled automatically.

---

## Installation

### Manual Installation

Copy the `STM32Ethernet/` folder to your Arduino libraries directory:

| OS      | Path |
|---------|------|
| Windows | `Documents\Arduino\libraries\` |
| macOS   | `~/Documents/Arduino/libraries/` |
| Linux   | `~/Arduino/libraries/` |

Restart Arduino IDE after copying.

---

## Quick Start

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <STM32Ethernet.h>

static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x44, 0x6E };
static uint8_t ip[]  = { 192, 168, 1, 100 };
static uint8_t sn[]  = { 255, 255, 255,   0 };
static uint8_t gw[]  = { 192, 168, 1,   1 };

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // SPI init
    pinMode(PA4, OUTPUT);
    digitalWrite(PA4, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(11250000, MSBFIRST, SPI_MODE0));

    // W5500 init
    wizchip_init();

    wiz_NetInfo netinfo;
    memcpy(netinfo.mac, mac, 6);
    memcpy(netinfo.ip,  ip,  4);
    memcpy(netinfo.sn,  sn,  4);
    memcpy(netinfo.gw,  gw,  4);
    netinfo.dhcp = 0;
    wizchip_setnetinfo(&netinfo);

    Serial.println(wizchip_getlinkstatus() ? "Link UP" : "Link DOWN");
}

void loop() {}
```

---

## API Reference

### Initialisation

```cpp
int8_t  wizchip_init(void);
int8_t  wizchip_setnetinfo(wiz_NetInfo* netinfo);
int8_t  wizchip_getnetinfo(wiz_NetInfo* netinfo);
uint8_t wizchip_getlinkstatus(void);   // 1 = UP, 0 = DOWN
uint8_t wizchip_isconnected(void);
```

### TCP Server

```cpp
void    EthernetServer_begin(uint16_t port);
uint8_t EthernetServer_available(void);   // returns socket number or 0xFF
void    EthernetServer_accept(void);      // call when done with client
uint8_t EthernetServer_hasClient(void);
uint8_t EthernetServer_isListening(void);
void    EthernetServer_stop(void);
uint16_t EthernetServer_getPort(void);
uint8_t  EthernetServer_getSocket(void);
```

### TCP Client

```cpp
uint8_t EthernetClient_begin(uint8_t socket_num);
uint8_t EthernetClient_connect(uint8_t socket_num, const uint8_t* ip, uint16_t port);
size_t  EthernetClient_write_array(uint8_t socket_num, const uint8_t* buf, size_t size);
size_t  EthernetClient_read_array(uint8_t socket_num, uint8_t* buf, size_t size);
int     EthernetClient_available(uint8_t socket_num);
int     EthernetClient_read(uint8_t socket_num);
void    EthernetClient_stop(uint8_t socket_num);
uint8_t EthernetClient_connected(uint8_t socket_num);
uint8_t EthernetClient_status(uint8_t socket_num);
```

### Socket — Low Level

```cpp
int8_t  socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);
int8_t  close(uint8_t sn);
int8_t  listen(uint8_t sn);
int8_t  connect(uint8_t sn, const uint8_t* addr, uint16_t port);
int8_t  disconnect(uint8_t sn);
int32_t send(uint8_t sn, const uint8_t* buf, uint16_t len);
int32_t recv(uint8_t sn, uint8_t* buf, uint16_t len);
```

---

## Examples

Three examples are included, all tested on STM32F446RE Nucleo-64 with WIZ550io:

### 01_ethernet_netinfo
Basic smoke test. Initialises the W5500, assigns static IP, reads back MAC/IP/GW/subnet, prints link status every 2 seconds.

### 02_modbus_client
Modbus TCP master. Opens a persistent TCP connection to a Modbus slave at 192.168.1.10:502 and polls 5 holding registers every 2 seconds using FC03. Auto-reconnects if the connection drops. Test with **Modbus Slave** simulator.

```
Modbus TCP client starting...
W5500 init OK
Waiting for link.. UP
Connected to server
reg1 = 1234, reg2 = -567, reg3 = 8900, reg4 = -42, reg5 = 32100
```

### 03_modbus_server
Modbus TCP slave. Serves 5 holding registers on port 1502. Handles FC03 read requests, returns correct MBAP exception responses for invalid function codes or address ranges. Test with **Modbus Poll**.

```
Modbus TCP server starting...
W5500 init OK
Waiting for link.. UP
Listening on port 1502
Client connected
FC=0x3 addr=0 count=5
Sent 5 regs OK
```

---

## Architecture

```
STM32Ethernet/
├── STM32Ethernet.h      ← single include for your sketch
├── w5500.h / .cpp       ← SPI framing, BSB-aware register R/W
├── wizchip_conf.h / .cpp← init, netinfo, PHY, buffer sizes
├── socket.h / .cpp      ← W5500 socket state machine (TCP/UDP)
├── server.h / .cpp      ← TCP server, single-listener, auto re-arm
├── client.h / .cpp      ← TCP client, read/write/connect/stop
├── util.h               ← shared return codes and utilities
├── library.properties
├── keywords.txt
├── LICENSE
└── examples/
    ├── 04_ethernet_netinfo/
    ├── 06_modbus_client/
    └── 08_modbus_server/
```

The hardware boundary is the SPI layer only. `w5500.cpp` calls:
`SPI.transfer()` and `digitalWrite(CS, LOW/HIGH)` — nothing else.

Everything above (socket, server, client, Modbus) is pure C logic with no CPU dependency. To port to a different STM32 variant, only the CS pin and SPI speed in `setup()` need adjusting.

---

## Compatibility

| Board | Core | Ethernet | Status |
|-------|------|----------|--------|
| STM32F446RE Nucleo-64 | STM32duino | WIZ550io (W5500) | ✅ Tested |
| STM32F446ZE Nucleo-144 | STM32duino | WIZ550io (W5500) | ✅ Compatible |
| STM32F103C8 Blue Pill | STM32duino | W5500 module | ✅ Compatible |
| Any STM32 | STM32duino | Any W5500 module | ✅ Adjust CS pin |
| STM32F446RE | Custom bare-metal core | WIZ550io | ✅ Tested |

---

## License

MIT License — see [LICENSE](LICENSE).

---

## Author

**Amir Shahbazi**
Senior Control System Engineer

Built from scratch after finding that no existing W5500 Ethernet library worked reliably on STM32F446RE with the STM32duino Arduino core. Developed as part of a complete bare-metal STM32F446 Arduino core project including custom SPI/DMA drivers, ADC DMA scanning with IIR filtering, bxCAN bus driver, and Modbus TCP gateway firmware for industrial applications.
