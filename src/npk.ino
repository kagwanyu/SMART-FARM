// KYNEXT AgriIoT — NPK Sensor Node
// MCU    : ESP32
// Sensor : RS485 NPK 3-in-1 (Modbus RTU, 9600 baud, 5V)
// Radio  : EBYTE E22-868M22S (SX1262)
// Power  : Solar + 18650 LiPo, deep sleep between cycles
// Pin Assignment:
//   GPIO16 — MAX485 RX  (UART2)
//   GPIO17 — MAX485 TX  (UART2)
//   GPIO4  — MAX485 DE/RE
//   GPIO5  — E22 NSS (CS)
//   GPIO18 — E22 SCK
//   GPIO23 — E22 MOSI
//   GPIO19 — E22 MISO
//   GPIO14 — E22 RESET
//   GPIO26 — E22 BUSY
//   GPIO27 — E22 DIO1
//   GPIO34 — Battery ADC (input only pin)

#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>

// ---- Node Identity ------------------------------------------
// Change this before flashing each node
#define NODE_ID   0x01   // 1-254, must be unique per node

// ---- Pin Definitions ----------------------------------------
#define RS485_RX        16
#define RS485_TX        17
#define RS485_DE_RE     4

#define LORA_NSS        5
#define LORA_SCK        18
#define LORA_MOSI       23
#define LORA_MISO       19
#define LORA_RESET      14
#define LORA_BUSY       26
#define LORA_DIO1       27

#define BATTERY_ADC     34

// ---- LoRa Settings ------------------------------------------
#define LORA_FREQ_MHZ   868.1
#define LORA_SF         10
#define LORA_BW         4       // 4 = 125kHz
#define LORA_CR         1       // 1 = 4/5
#define LORA_TX_POWER   22      // dBm

// ---- Modbus Settings ----------------------------------------
#define MODBUS_BAUD     9600
#define MODBUS_ADDR     0x01    // default NPK sensor slave address
#define MODBUS_TIMEOUT  500     // ms to wait for sensor response

// NPK register addresses
#define REG_NITROGEN    0x0000
#define REG_PHOSPHORUS  0x0001
#define REG_POTASSIUM   0x0002

// ---- Retry Settings -----------------------------------------
#define MAX_RETRIES     7
#define ACK_TIMEOUT_MS  5000

// ---- Deep Sleep ---------------------------------------------
// 10 minutes in microseconds
#define SLEEP_DURATION_US   (10ULL * 60ULL * 1000000ULL)

#pragma pack(1)
struct Packet {
  uint8_t  node_id;
  uint16_t nitrogen;
  uint16_t phosphorus;
  uint16_t potassium;
  uint16_t battery_mv;
  uint16_t crc;
};
#pragma pack()

//ERROR DETECTION MECHANISM 
uint16_t crc16(uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else              crc <<= 1;
    }
  }
  return crc;
}
//MODBUS - ERROR DETECTION 

uint16_t modbusCRC(uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
      else              crc >>= 1;
    }
  }
  return crc;
}

//BATTERY VOLTAGE CHECK

uint16_t readBatteryMv() {
  uint32_t raw = analogRead(BATTERY_ADC);
  // multiply by 2 for voltage divider, convert to mV
  return (uint16_t)((raw * 2 * 3300) / 4095);
}

//SENSOR HELPERS

void rs485_transmitMode() {
  digitalWrite(RS485_DE_RE, HIGH); // DE=HIGH, RE=HIGH = transmit
}

void rs485_receiveMode() {
  digitalWrite(RS485_DE_RE, LOW);  // DE=LOW, RE=LOW = receive
}

// Read a single Modbus holding register
// Returns value on success, 0xFFFF on failure

uint16_t modbus_readRegister(uint8_t slaveAddr, uint16_t regAddr) {
  // Build Modbus RTU read request
  // Function code 0x03 = Read Holding Registers
  uint8_t request[8];
  request[0] = slaveAddr;
  request[1] = 0x03;                      
  request[2] = (regAddr >> 8) & 0xFF;     
  request[3] = regAddr & 0xFF;            
  request[4] = 0x00;                      
  request[5] = 0x01;                     
  uint16_t reqCRC = modbusCRC(request, 6);
  request[6] = reqCRC & 0xFF;             
  request[7] = (reqCRC >> 8) & 0xFF;     

  //TRANSMIT REQUEST

rs485_transmitMode();
  Serial2.write(request, 8);
  Serial2.flush();                        // wait until fully sent
  rs485_receiveMode();
// Wait for response (7 bytes expected)
  // Response format: [addr][0x03][byte count=2][data high][data low][CRC low][CRC high]
  uint32_t start = millis();
  while (Serial2.available() < 7) {
    if (millis() - start > MODBUS_TIMEOUT) return 0xFFFF; // timeout
  }
  uint8_t response[7];
  Serial2.readBytes(response, 7);

// Validate response CRC
  uint16_t respCRC = modbusCRC(response, 5);
  uint16_t receivedCRC = response[5] | ((uint16_t)response[6] << 8);
  if (respCRC != receivedCRC) return 0xFFFF; // CRC mismatch

  // Validate slave address and function code
  if (response[0] != slaveAddr || response[1] != 0x03) return 0xFFFF;

  // Extract 16-bit register value
  return ((uint16_t)response[3] << 8) | response[4];
}

// Read all NPK values from sensor
// Returns true if all three registers read successfully

bool readNPK(uint16_t *nitrogen, uint16_t *phosphorus, uint16_t *potassium) {
  *nitrogen   = modbus_readRegister(MODBUS_ADDR, REG_NITROGEN);
  *phosphorus = modbus_readRegister(MODBUS_ADDR, REG_PHOSPHORUS);
  *potassium  = modbus_readRegister(MODBUS_ADDR, REG_POTASSIUM);

  // 0xFFFF means failed read on any value
  if (*nitrogen   == 0xFFFF) return false;
  if (*phosphorus == 0xFFFF) return false;
  if (*potassium  == 0xFFFF) return false;

  return true;
}

//// SX1262 Helpers

void sx1262_waitBusy() {
  uint32_t start = millis();
  while (digitalRead(LORA_BUSY)) {
    if (millis() - start > 3000) break; // failsafe
  }
}

void sx1262_writeCommand(uint8_t cmd, uint8_t *data, uint8_t len) {
  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(cmd);
  for (uint8_t i = 0; i < len; i++) SPI.transfer(data[i]);
  digitalWrite(LORA_NSS, HIGH);
}

void sx1262_readCommand(uint8_t cmd, uint8_t *data, uint8_t len) {
  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(cmd);
  SPI.transfer(0x00);
  for (uint8_t i = 0; i < len; i++) data[i] = SPI.transfer(0x00);
  digitalWrite(LORA_NSS, HIGH);
}

void sx1262_writeRegister(uint16_t addr, uint8_t value) {
  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(0x0D);
  SPI.transfer((addr >> 8) & 0xFF);
  SPI.transfer(addr & 0xFF);
  SPI.transfer(value);
  digitalWrite(LORA_NSS, HIGH);
}

// SX1262 Initialisation

void sx1262_init() {
  pinMode(LORA_NSS,   OUTPUT);
  pinMode(LORA_RESET, OUTPUT);
  pinMode(LORA_BUSY,  INPUT);
  pinMode(LORA_DIO1,  INPUT);
  digitalWrite(LORA_NSS, HIGH);

  digitalWrite(LORA_RESET, LOW);
  delay(10);
  digitalWrite(LORA_RESET, HIGH);
  delay(20);
  sx1262_waitBusy();

  uint8_t standby[] = {0x00};
  sx1262_writeCommand(0x80, standby, 1);

  uint8_t pktType[] = {0x01};
  sx1262_writeCommand(0x8A, pktType, 1);

  uint32_t freq = (uint32_t)((LORA_FREQ_MHZ * (1 << 25)) / 32.0);
  uint8_t freqBytes[] = {
    (uint8_t)(freq >> 24),
    (uint8_t)(freq >> 16),
    (uint8_t)(freq >> 8),
    (uint8_t)(freq)
  };
  sx1262_writeCommand(0x86, freqBytes, 4);

  uint8_t paConfig[] = {0x04, 0x07, 0x00, 0x01};
  sx1262_writeCommand(0x95, paConfig, 4);

  uint8_t txParams[] = {(uint8_t)LORA_TX_POWER, 0x04};
  sx1262_writeCommand(0x8E, txParams, 2);

  uint8_t modParams[] = {
    (uint8_t)LORA_SF,
    (uint8_t)LORA_BW,
    (uint8_t)LORA_CR,
    0x01  // low data rate optimize ON for SF10
  };
  sx1262_writeCommand(0x8B, modParams, 4);

  uint8_t pktParams[] = {
    0x00, 0x0C,           // preamble = 12 symbols
    0x00,                 // explicit header
    sizeof(Packet),       // payload length = 11 bytes
    0x01,                 // CRC off at radio level
    0x00,                 // no IQ inversion
    0x00
  };
  sx1262_writeCommand(0x8C, pktParams, 7);

  sx1262_writeRegister(0x0889, 0x08); // sensitivity errata fix
}

// SX1262 Transmit

void sx1262_transmit(uint8_t *payload, uint8_t len) {
  uint8_t bufBase[] = {0x00, 0x00};
  sx1262_writeCommand(0x8F, bufBase, 2);

  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(0x0E);
  SPI.transfer(0x00);
  for (uint8_t i = 0; i < len; i++) SPI.transfer(payload[i]);
  digitalWrite(LORA_NSS, HIGH);

  uint8_t dioMap[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sx1262_writeCommand(0x08, dioMap, 8);

  uint8_t txCmd[] = {0x00, 0x00, 0x00};
  sx1262_writeCommand(0x83, txCmd, 3);

  uint32_t start = millis();
  while (!digitalRead(LORA_DIO1)) {
    if (millis() - start > 5000) break;
  }

  uint8_t clrIrq[] = {0xFF, 0xFF};
  sx1262_writeCommand(0x02, clrIrq, 2);
}

// SX1262 Receive (blocking with timeout)

bool sx1262_receive(uint8_t *buf, uint8_t *rxLen, uint16_t timeout_ms) {
  uint8_t rxCmd[] = {0x00, 0x00, 0x00};
  sx1262_writeCommand(0x82, rxCmd, 3);

  uint8_t dioMap[] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sx1262_writeCommand(0x08, dioMap, 8);

  uint32_t start = millis();
  while (!digitalRead(LORA_DIO1)) {
    if (millis() - start > timeout_ms) {
      uint8_t standby[] = {0x00};
      sx1262_writeCommand(0x80, standby, 1);
      return false;
    }
  }

  uint8_t rxStatus[2];
  sx1262_readCommand(0x13, rxStatus, 2);
  *rxLen = rxStatus[0];
  uint8_t startAddr = rxStatus[1];

  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(0x1E);
  SPI.transfer(startAddr);
  SPI.transfer(0x00);
  for (uint8_t i = 0; i < *rxLen; i++) buf[i] = SPI.transfer(0x00);
  digitalWrite(LORA_NSS, HIGH);

  uint8_t clrIrq[] = {0xFF, 0xFF};
  sx1262_writeCommand(0x02, clrIrq, 2);

  uint8_t standby[] = {0x00};
  sx1262_writeCommand(0x80, standby, 1);

  return true;
}

// SX1262 Sleep

void sx1262_sleep() {
  uint8_t sleepConfig[] = {0x04};
  sx1262_writeCommand(0x84, sleepConfig, 1);
}

// Build packet and transmit with retry

void sendReading(uint16_t nitrogen, uint16_t phosphorus,
                 uint16_t potassium, uint16_t batt_mv) {
  Packet pkt;
  pkt.node_id    = NODE_ID;
  pkt.nitrogen   = nitrogen;
  pkt.phosphorus = phosphorus;
  pkt.potassium  = potassium;
  pkt.battery_mv = batt_mv;
  pkt.crc        = crc16((uint8_t *)&pkt, sizeof(Packet) - 2);

  uint8_t *payload    = (uint8_t *)&pkt;
  uint8_t  payloadLen = sizeof(Packet);

  for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    if (attempt > 0) {
      sx1262_init(); // reinit radio after sleep between retries
    }

    sx1262_transmit(payload, payloadLen);

    // Wait for ACK: [0xAC, NODE_ID]
    uint8_t ackBuf[4];
    uint8_t ackLen = 0;
    bool gotAck = sx1262_receive(ackBuf, &ackLen, ACK_TIMEOUT_MS);

    if (gotAck && ackLen >= 2 &&
        ackBuf[0] == 0xAC && ackBuf[1] == NODE_ID) {
      break; // gateway confirmed receipt
    }

    // Sleep radio between retries
    sx1262_sleep();
    delay(100);
  }
}

// Setup — runs once on every wake from deep sleep
void setup() {
  // RS485
  pinMode(RS485_DE_RE, OUTPUT);
  rs485_receiveMode();
  Serial2.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  // Battery ADC
  pinMode(BATTERY_ADC, INPUT);
  analogReadResolution(12); // ESP32 12-bit ADC

  // SPI for LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  sx1262_init();

  // Read NPK sensor
  uint16_t nitrogen, phosphorus, potassium;
  bool ok = readNPK(&nitrogen, &phosphorus, &potassium);

  // If sensor read failed use 0xFFFE as error sentinel
  // (0xFFFF is reserved for internal failure flag)
  if (!ok) {
    nitrogen   = 0xFFFE;
    phosphorus = 0xFFFE;
    potassium  = 0xFFFE;
  }

  // Read battery
  uint16_t batt_mv = readBatteryMv();

  // Transmit
  sendReading(nitrogen, phosphorus, potassium, batt_mv);

  // Sleep radio then put ESP32 into deep sleep
  sx1262_sleep();
  esp_deep_sleep(SLEEP_DURATION_US);
}


void loop() {}


