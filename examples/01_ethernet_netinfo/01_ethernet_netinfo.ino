// =============================================================
// 01_ethernet_netinfo.ino
// Smoke test: W5500 full init + MAC/IP/GW/subnet assignment
//
// Compatible with: STM32duino core
// Hardware: WIZ550io or W5500 module on SPI1
//   CS  = PA4
//   SCK = PA5   MISO = PA6   MOSI = PA7
//
// Expected output on Serial (115200):
//   W5500 init OK
//   MAC: DE:AD:BE:EF:44:6E
//   IP:  192.168.1.100
//   Sub: 255.255.255.0
//   GW:  192.168.1.1
//   Link: UP
// =============================================================

#include <Arduino.h>
#include <SPI.h>
#include <STM32Ethernet.h> 

// ---- Network configuration — edit to match your network ----
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x44, 0x6E };
static uint8_t ip[]  = { 192, 168, 1, 100 };
static uint8_t sn[]  = { 255, 255, 255, 0 };
static uint8_t gw[]  = { 192, 168, 1, 1 };

// ---- Print helpers ----
static void print_ip(const char* label, uint8_t* addr)
{
    Serial.print(label);
    for (int i = 0; i < 4; i++) {
        Serial.print(addr[i]);
        if (i < 3) Serial.print(".");
    }
    Serial.println();
}

static void print_mac(const char* label, uint8_t* m)
{
    const char* h = "0123456789ABCDEF";
    Serial.print(label);
    for (int i = 0; i < 6; i++) {
        Serial.print(h[m[i] >> 4]);
        Serial.print(h[m[i] & 0xF]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Ethernet init starting...");

    // SPI init — STM32duino style
    pinMode(PA4, OUTPUT);
    digitalWrite(PA4, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(11250000, MSBFIRST, SPI_MODE0));

    if (wizchip_init() != WIZCHIP_OK) {
        Serial.println("wizchip_init FAILED");
        return;
    }
    Serial.println("W5500 init OK");

    wiz_NetInfo netinfo;
    for (int i = 0; i < 6; i++) netinfo.mac[i] = mac[i];
    for (int i = 0; i < 4; i++) netinfo.ip[i]  = ip[i];
    for (int i = 0; i < 4; i++) netinfo.sn[i]  = sn[i];
    for (int i = 0; i < 4; i++) netinfo.gw[i]  = gw[i];
    netinfo.dhcp = 0;

    if (wizchip_setnetinfo(&netinfo) != WIZCHIP_OK) {
        Serial.println("setnetinfo FAILED");
        return;
    }

    wiz_NetInfo readback;
    wizchip_getnetinfo(&readback);
    print_mac("MAC: ", readback.mac);
    print_ip ("IP:  ", readback.ip);
    print_ip ("Sub: ", readback.sn);
    print_ip ("GW:  ", readback.gw);

    Serial.print("Link: ");
    Serial.println(wizchip_getlinkstatus() ? "UP" : "DOWN");
}

void loop()
{
    static uint32_t last  = 0;
    static uint8_t  state = 0;

    if ((millis() - last) >= 2000) {
        last  = millis();
        state = !state;
        digitalWrite(LED_BUILTIN, state);   // built-in LED — adjust pin if needed
        Serial.print("Link: ");
        Serial.println(wizchip_getlinkstatus() ? "UP" : "DOWN");
    }
}
