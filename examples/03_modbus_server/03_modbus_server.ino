// =============================================================
// 03_modbus_server.ino
// Modbus TCP server — serves 5 holding registers on port 1502
//
// Compatible with: STM32duino core
// Hardware: WIZ550io or W5500 module on SPI1
//   CS = PA4, SCK = PA5, MISO = PA6, MOSI = PA7
//
// Test with Modbus Poll:
//   Connection: Modbus TCP
//   IP: 192.168.1.100   Port: 1502
//   Function: FC03 Read Holding Registers
//   Start address: 40001   Length: 5
//
// Register map:
//   40001 =  1234
//   40002 = -567
//   40003 =  8900
//   40004 = -42
//   40005 =  32100
// =============================================================

#include <Arduino.h>
#include <SPI.h>
#include <STM32Ethernet.h>

// ---- Network config ----
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x44, 0x6E };
static uint8_t ip[]  = { 192, 168, 1, 100 };
static uint8_t sn[]  = { 255, 255, 255, 0 };
static uint8_t gw[]  = { 192, 168, 1, 1 };

#define MODBUS_SERVER_PORT  1502

// ---- Holding registers (address 0 = 40001) ----
static const int16_t holding_regs[5] = {
     1234,   // 40001
     -567,   // 40002
     8900,   // 40003
      -42,   // 40004
    32100    // 40005
};
#define NUM_REGS  5

// ---- Request / response buffers ----
#define REQ_LEN   12
#define RESP_MAX  256
static uint8_t req_buf[REQ_LEN];
static uint8_t resp_buf[RESP_MAX];

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

//Build and send Modbus exception response
static void send_exception(uint8_t sock, uint8_t* mbap,
                            uint8_t fc, uint8_t code)
{
    uint8_t exc[9];
    exc[0] = mbap[0]; exc[1] = mbap[1];  // Transaction ID
    exc[2] = 0x00;    exc[3] = 0x00;     // Protocol ID
    exc[4] = 0x00;    exc[5] = 0x03;     // Length = 3
    exc[6] = mbap[6];                    // Unit ID
    exc[7] = fc | 0x80;                  // FC with error bit
    exc[8] = code;                       // Exception code
    EthernetClient_write_array(sock, exc, 9);
}

//Process one Modbus TCP request
static bool process_request(uint8_t sock)
{
    // Wait for at least 12 bytes (500ms timeout)
    uint32_t t = millis();
    while (EthernetClient_available(sock) < REQ_LEN) {
        if ((millis() - t) > 500) {
            Serial.println("Request timeout");
            return false;
        }
        uint8_t sr = WIZCHIP_READ_B(W5500_SOCKET_REG(sock), Sn_SR);
        if (sr != SOCK_ESTABLISHED) return false;
    }

    size_t n = EthernetClient_read_array(sock, req_buf, REQ_LEN);
    if (n < REQ_LEN) return false;

    uint8_t  fc         = req_buf[7];
    uint16_t start_addr = ((uint16_t)req_buf[8]  << 8) | req_buf[9];
    uint16_t reg_count  = ((uint16_t)req_buf[10] << 8) | req_buf[11];

    Serial.print("FC=0x"); Serial.print(fc, HEX);
    Serial.print(" addr="); Serial.print(start_addr);
    Serial.print(" count="); Serial.println(reg_count);

    // Only FC03 supported
    if (fc != 0x03) {
        send_exception(sock, req_buf, fc, 0x01);  // illegal function
        return true;
    }

    // Validate address range
    if (start_addr >= NUM_REGS || (start_addr + reg_count) > NUM_REGS) {
        send_exception(sock, req_buf, fc, 0x02);  // illegal address
        return true;
    }

    // Validate count
    if (reg_count == 0 || reg_count > 125) {
        send_exception(sock, req_buf, fc, 0x03);  // illegal value
        return true;
    }

    // Build FC03 response
    uint8_t  byte_count = (uint8_t)(reg_count * 2);
    uint16_t mbap_len   = 1 + 1 + 1 + byte_count;  // UID+FC+BC+data

    resp_buf[0] = req_buf[0];                    // Transaction ID high
    resp_buf[1] = req_buf[1];                    // Transaction ID low
    resp_buf[2] = 0x00;                          // Protocol ID
    resp_buf[3] = 0x00;
    resp_buf[4] = (uint8_t)(mbap_len >> 8);      // Length high
    resp_buf[5] = (uint8_t)(mbap_len & 0xFF);    // Length low
    resp_buf[6] = req_buf[6];                    // Unit ID
    resp_buf[7] = 0x03;                          // FC03
    resp_buf[8] = byte_count;                    // Byte count

    // Register data — big-endian
    for (uint16_t i = 0; i < reg_count; i++) {
        uint16_t raw = (uint16_t)holding_regs[start_addr + i];
        resp_buf[9 + i*2]     = (uint8_t)(raw >> 8);
        resp_buf[9 + i*2 + 1] = (uint8_t)(raw & 0xFF);
    }

    EthernetClient_write_array(sock, resp_buf, 9 + byte_count);

    Serial.print("Sent "); Serial.print(reg_count);
    Serial.println(" regs OK");

    return true;
}

//Server state machine
typedef enum { SRV_IDLE, SRV_CONNECTED } SrvState;
static SrvState srv_state = SRV_IDLE;

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Modbus TCP server starting...");

    ethernet_init();
    delay(1000);

    Serial.print("Waiting for link");
    while (!wizchip_getlinkstatus()) {
        Serial.print(".");
        delay(500);
    }
    Serial.println(" UP");

    EthernetServer_begin(MODBUS_SERVER_PORT);
    Serial.println("Listening on port 1502");
    Serial.println("Registers:");
    Serial.println("  40001 =  1234");
    Serial.println("  40002 = -567");
    Serial.println("  40003 =  8900");
    Serial.println("  40004 = -42");
    Serial.println("  40005 =  32100");
}

void loop()
{
    uint8_t client = EthernetServer_available();

    switch (srv_state) {

        case SRV_IDLE:
            if (client != 0xFF) {
                Serial.println("Client connected");
                srv_state = SRV_CONNECTED;
            }
            break;

        case SRV_CONNECTED:
            if (client == 0xFF) {
                Serial.println("Client disconnected");
                EthernetServer_accept();
                srv_state = SRV_IDLE;
                break;
            }
            if (EthernetClient_available(client) >= REQ_LEN) {
                if (!process_request(client)) {
                    Serial.println("Closing connection");
                    EthernetServer_accept();
                    srv_state = SRV_IDLE;
                }
            }
            break;
    }
}
