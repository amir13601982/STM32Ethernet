// =============================================================
// 02_modbus_client.ino
// Modbus TCP client — persistent connection, reads 5 registers
//
// Compatible with: STM32duino core
// Hardware: WIZ550io or W5500 module on SPI1
//   CS = PA4, SCK = PA5, MISO = PA6, MOSI = PA7
//
// Server: 192.168.1.10:502
// Registers: 40001-40005 (address 0x0000, count 5)
// =============================================================

#include <Arduino.h>
#include <SPI.h>
#include <W5500Ethernet.h> 

// ---- Network config ----
static uint8_t mac[]    = { 0xDE, 0xAD, 0xBE, 0xEF, 0x44, 0x6E };
static uint8_t ip[]     = { 192, 168, 1, 100 };
static uint8_t sn[]     = { 255, 255, 255, 0 };
static uint8_t gw[]     = { 192, 168, 1, 1 };
static uint8_t srv_ip[] = { 192, 168, 1, 10 };

#define MODBUS_PORT  502
#define MODBUS_SOCK  0

// ---- Connection state ----
static bool     connected = false;
static uint16_t src_port  = 49152;

// ---- Modbus TCP FC03 request — read 5 registers from address 0 ----
static const uint8_t mb_request[12] = {
    0x00, 0x01,   // Transaction ID
    0x00, 0x00,   // Protocol ID
    0x00, 0x06,   // PDU length
    0x01,         // Unit ID
    0x03,         // FC03 Read Holding Registers
    0x00, 0x00,   // Start address 0x0000
    0x00, 0x05    // Register count = 5
};

#define RESP_LEN  19
static uint8_t resp_buf[RESP_LEN];

// ---- Ethernet init ----
static void ethernet_init(void)
{
    pinMode(PA4, OUTPUT);
    digitalWrite(PA4, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(11250000, MSBFIRST, SPI_MODE0));

    if (wizchip_init() != WIZCHIP_OK) {
        Serial.println("W5500 init FAILED");
        while (1);
    }

    wiz_NetInfo netinfo;
    for (int i = 0; i < 6; i++) netinfo.mac[i] = mac[i];
    for (int i = 0; i < 4; i++) netinfo.ip[i]  = ip[i];
    for (int i = 0; i < 4; i++) netinfo.sn[i]  = sn[i];
    for (int i = 0; i < 4; i++) netinfo.gw[i]  = gw[i];
    netinfo.dhcp = 0;

    wizchip_setnetinfo(&netinfo);
    Serial.println("W5500 init OK");
}

// ---- Print signed int16 ----
static void print_int16(int16_t val)
{
    if (val < 0) {
        Serial.print("-");
        Serial.print((uint32_t)(-(int32_t)val));
    } else {
        Serial.print((uint32_t)val);
    }
}

// ---- Check if socket is still alive ----
static bool socket_alive(void)
{
    uint8_t sr = WIZCHIP_READ_B(W5500_SOCKET_REG(MODBUS_SOCK), Sn_SR);
    return (sr == SOCK_ESTABLISHED);
}

// ---- Connect or reconnect to server ----
static bool ensure_connected(void)
{
    if (connected && socket_alive()) return true;

    uint8_t sr = WIZCHIP_READ_B(W5500_SOCKET_REG(MODBUS_SOCK), Sn_SR);
    if (sr != SOCK_CLOSED) close(MODBUS_SOCK);
    connected = false;

    if (++src_port > 65535) src_port = 49152;

    if (socket(MODBUS_SOCK, Sn_MR_TCP, src_port, 0) != SOCK_OK) {
        Serial.println("Socket open failed");
        return false;
    }

    if (connect(MODBUS_SOCK, srv_ip, MODBUS_PORT) != SOCK_OK) {
        Serial.println("Connect failed");
        close(MODBUS_SOCK);
        return false;
    }

    Serial.println("Connected to server");
    connected = true;
    return true;
}

// ---- Modbus read transaction ----
static void modbus_read_regs(void)
{
    if (!ensure_connected()) return;

    if (EthernetClient_write_array(MODBUS_SOCK,
            mb_request, sizeof(mb_request)) != sizeof(mb_request)) {
        Serial.println("Send failed — will reconnect");
        close(MODBUS_SOCK);
        connected = false;
        return;
    }

    // Wait for full response (500ms timeout)
    uint32_t t_start = millis();
    while (EthernetClient_available(MODBUS_SOCK) < (int)RESP_LEN) {
        if ((millis() - t_start) > 500) {
            Serial.println("Response timeout — will reconnect");
            close(MODBUS_SOCK);
            connected = false;
            return;
        }
    }

    size_t n = EthernetClient_read_array(MODBUS_SOCK, resp_buf, RESP_LEN);
    if (n < RESP_LEN) {
        Serial.println("Short response — will reconnect");
        close(MODBUS_SOCK);
        connected = false;
        return;
    }

    if (resp_buf[7] != 0x03 || resp_buf[8] != 10) {
        Serial.print("Bad response FC=0x");
        Serial.print(resp_buf[7], HEX);
        Serial.print(" BC=");
        Serial.println(resp_buf[8]);
        close(MODBUS_SOCK);
        connected = false;
        return;
    }

    // Extract 5 signed int16 registers (big-endian, starts at byte 9)
    int16_t regs[5];
    for (int i = 0; i < 5; i++) {
        uint16_t raw = ((uint16_t)resp_buf[9 + i*2] << 8)
                      | (uint16_t)resp_buf[10 + i*2];
        regs[i] = (int16_t)raw;
    }

    for (int i = 0; i < 5; i++) {
        Serial.print("reg");
        Serial.print(i + 1);
        Serial.print(" = ");
        print_int16(regs[i]);
        if (i < 4) Serial.print(", ");
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Modbus TCP client starting...");

    ethernet_init();
    delay(1000);

    Serial.print("Waiting for link");
    while (!wizchip_getlinkstatus()) {
        Serial.print(".");
        delay(500);
    }
    Serial.println(" UP");
}

void loop()
{
    modbus_read_regs();
    delay(2000);
}
