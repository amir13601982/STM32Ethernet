# STM32Ethernet

A lightweight W5500 Ethernet library for STM32 microcontrollers, compatible with the official STM32duino Arduino core.

Built and tested on STM32F446RE (Nucleo-64) with a WIZ550io module.

---

## Features

- TCP server and client (single-listener architecture)
- Modbus TCP slave (FC03 Read Holding Registers, FC16 Write Multiple Registers)
- Modbus TCP master (persistent connection, auto-reconnect)
- W5500 register layer with correct BSB-aware SPI framing
- Socket state machine (open, listen, connect, send, recv, close)
- PHY link status detection
- Clean re-arm on client disconnect — server never misses a new connection

## Limitations

This is a focused, production-grade library for industrial embedded systems. It intentionally does **not** include:

- **DHCP** — static IP only
- **DNS** — use raw IP addresses
- **UDP** — TCP only
- **HTTPS / TLS** — plain TCP only
- **Multi-client server** — single listener per port (one active connection at a time)


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

> **Important:** Keep SPI wires under 10cm. Signal integrity issues on long wires will cause the W5500 socket state machine to silently reject OPEN commands — even though register reads/writes succeed.

---

## Installation

### Via Arduino IDE (recommended)

1. Download or clone this repository as a ZIP file
2. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library**
3. Select the downloaded ZIP
4. Restart Arduino IDE
5. The library appears under **File → Examples → STM32Ethernet**

### Manual Installation

Copy the `STM32Ethernet/` folder to your Arduino libraries directory:

| OS      | Path |
|---------|------|
| Windows | `Documents\Arduino\libraries\` |
| macOS   | `~/Documents/Arduino/libraries/` |
| Linux   | `~/Arduino/libraries/` |

---

## Quick Start

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <STM32Ethernet.h>

static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x44, 0x6E };
static uint8_t ip[]  = { 192, 168, 1, 100 };
static uint8_t sn[]  = { 255, 255, 255, 0 };
static uint8_t gw[]  = { 192, 168, 1,   1 };

void setup()
{
    Serial.begin(115200);

    pinMode(PA4, OUTPUT);
    digitalWrite(PA4, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(11250000, MSBFIRST, SPI_MODE0));

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
```

### TCP Server

```cpp
void    EthernetServer_begin(uint16_t port);
uint8_t EthernetServer_available(void);   // returns socket num or 0xFF
void    EthernetServer_accept(void);      // call when done with client
uint8_t EthernetServer_hasClient(void);
uint8_t EthernetServer_isListening(void);
void    EthernetServer_stop(void);
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

### Socket (low-level)

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

### TCP Server (`08_modbus_server`)

Modbus TCP slave serving 5 holding registers on port 1502.
Test with **Modbus Poll**: TCP connection, IP 192.168.1.100, port 1502, FC03.

```
Modbus TCP server starting...
W5500 init OK
Waiting for link.. UP
Listening on port 1502
Client connected
FC=0x3 addr=0 count=5
Sent 5 regs OK
```

### Modbus TCP Client (`06_modbus_client`)

Persistent connection Modbus TCP master. Connects once, keeps connection open,
only reconnects if the link drops. Test with **Modbus Slave** simulator.

```
Modbus TCP client starting...
W5500 init OK
Waiting for link.. UP
Connected to server
reg1 = 1234, reg2 = -567, reg3 = 8900, reg4 = -42, reg5 = 32100
```

---

## Architecture

```
STM32Ethernet/
├── STM32Ethernet.h      ← single include for your sketch
├── w5500.h / .cpp       ← SPI framing, BSB-aware register access
├── wizchip_conf.h / .cpp← init, netinfo, PHY, buffer config
├── socket.h / .cpp      ← W5500 socket state machine
├── server.h / .cpp      ← TCP server (single-listener, auto re-arm)
├── client.h / .cpp      ← TCP client (read, write, connect, stop)
├── util.h               ← shared utilities
├── library.properties
├── keywords.txt
├── LICENSE
└── examples/
    ├── 04_ethernet_netinfo/
    ├── 06_modbus_client/
    └── 08_modbus_server/
```

The hardware boundary is exactly the SPI layer — `w5500.cpp` calls only:
`SPI.begin()`, `SPI.transfer()`, `digitalWrite(CS, LOW/HIGH)`.

Everything above (`socket`, `server`, `client`, Modbus) is pure C logic with no CPU dependency. To port to a different STM32 variant, only the SPI initialisation in your sketch needs changing.

---

## Compatibility

| Board | Core | Status |
|-------|------|--------|
| STM32F446RE (Nucleo-64) | STM32duino | ✅ Tested |
| STM32F446ZE (Nucleo-144) | STM32duino | ✅ Compatible (different UART/CAN pins) |
| STM32F103C8 (Blue Pill) | STM32duino | ✅ Compatible |
| Any STM32 | STM32duino | ✅ Should work — adjust CS pin |
| STM32F446RE | Custom bare-metal core | ✅ Tested |

---

## Publishing to the Official Arduino Library Registry

To have your library appear in the Arduino IDE Library Manager (searchable by everyone):

### Step 1 — Prepare GitHub Repository

1. Create a new **public** repository on GitHub named `STM32Ethernet`
2. Push your library folder contents (not the folder itself) to the root:
   ```
   /README.md
   /LICENSE
   /library.properties
   /keywords.txt
   /STM32Ethernet.h
   /w5500.h  /w5500.cpp  ...
   /examples/...
   ```
3. Create a **release** with a version tag matching `library.properties`:
   - GitHub → Releases → Draft a new release
   - Tag: `1.0.0`
   - Title: `STM32Ethernet 1.0.0`
   - Click **Publish release**

### Step 2 — Submit to Arduino Library Registry

1. Go to: https://github.com/arduino/library-registry
2. Click **Add your library**
3. Open `repositories.txt` and add your repository URL:
   ```
   https://github.com/YOUR_USERNAME/STM32Ethernet
   ```
4. Submit a Pull Request
5. Arduino's automated checks will validate your `library.properties`
6. Once the PR is merged (usually 1-7 days), the library appears in Arduino IDE Library Manager

### Requirements Checklist

- [x] `library.properties` present and valid
- [x] `examples/` folder with at least one valid `.ino` example
- [x] Example `.ino` filename matches its parent folder name
- [x] `LICENSE` file present
- [x] Repository is public
- [x] At least one GitHub release with a version tag

---

## License

MIT License — see `LICENSE` file.

---

## Author

**Amir Shahbazi**
Senior Control Engineer — Perth, Western Australia

Built from scratch as part of a bare-metal STM32F446 Arduino core development project,
including custom SPI/DMA drivers, ADC DMA scanning, CAN bus, and Modbus TCP gateway
for industrial applications.
