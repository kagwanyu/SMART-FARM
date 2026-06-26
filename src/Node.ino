// KYNEXT AgriIoT — Soil Moisture Sensor Node
// MCU    : ATtiny84
// Radio  : EBYTE E22-868M22S (SX1262)
// Sensors: 2x Capacitive Soil Moisture (analog)
// Power  : Solar + 18650 LiPo via TP4056

//   PA0 (A0)  — Moisture Sensor 1
//   PA7 (A7)  — Moisture Sensor 2
//   PA4       — SX1262 SCK
//   PA5       — SX1262 MISO
//   PA6       — SX1262 MOSI
//   PB2       — SX1262 NSS (CS)
//   PA3       — SX1262 RESET
//   PA2       — SX1262 BUSY
//   PA1       — SX1262 DIO1 (IRQ)

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SPI.h>

#define MOISTURE_1_PIN  A0
#define LORA_NSS        PB2
#define LORA_RESET      PA3
#define LORA_BUSY       PA2
#define LORA_DIO1       PA1

#define NODE-ID   0x01
   //LORA SETUP
#define LORA_FREQ_MHZ         868.1
#define LORA_SF               10
#define LORA_BW               4      // 0=7.8k ... 4=125k ... 6=250k
#define LORA_CR               1      // 1=4/5
#define LORA_TX_POWER         22     // dBm, E22 max
  //RETRY SETUP
#define MAX_RETRIES           7
#define ACK_TIMEOUT_MS        5000 

#define SLEEP_CYCLES          75 //sleep cycles
 //packet structure

 struct Packet {
  uint8_t  node_id;
  uint16_t moisture1;
  uint16_t moisture2;
  uint16_t battery_mv;
  uint16_t crc;
};

uint8_t   nodeId;
volatile bool wdtFired = false;

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

   //power reading

uint16_t readBatteryMv() {
  // Select Vbg (1.1V) as input, VCC as reference
  ADMUX = _BV(MUX5) | _BV(MUX0);  // ATtiny84 bandgap = MUX[5:0] = 100001
  delay(5);                         // let reference settle
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC));
  uint16_t adc = ADC;
  // VCC (mV) = 1100 * 1024 / adc
  return (uint16_t)(1126400UL / adc);
}
///Lora spi setup 
void sx1262_waitBusy() {
  while (digitalRead(LORA_BUSY));
}

uint8_t sx1262_spiTransfer(uint8_t data) {
  return SPI.transfer(data);
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
  SPI.transfer(0x00); // status byte
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
//LORA initialization
void sx1262_init() {
  pinMode(LORA_NSS,   OUTPUT);
  pinMode(LORA_RESET, OUTPUT);
  pinMode(LORA_BUSY,  INPUT);
  pinMode(LORA_DIO1,  INPUT);
  digitalWrite(LORA_NSS, HIGH);

  // Hardware reset
  digitalWrite(LORA_RESET, LOW);
  delay(10);
  digitalWrite(LORA_RESET, HIGH);
  delay(20);
  sx1262_waitBusy();

  // Set to standby RC mode
  uint8_t standby[] = {0x00};
  sx1262_writeCommand(0x80, standby, 1);

  // Set packet type to LoRa
  uint8_t pktType[] = {0x01};
  sx1262_writeCommand(0x8A, pktType, 1);

  // Set RF frequency: freq_reg = freq_MHz * 2^25 / 32
  uint32_t freq = (uint32_t)((LORA_FREQ_MHZ * (1 << 25)) / 32.0);
  uint8_t freqBytes[] = {
    (uint8_t)(freq >> 24),
    (uint8_t)(freq >> 16),
    (uint8_t)(freq >> 8),
    (uint8_t)(freq)
  };
  sx1262_writeCommand(0x86, freqBytes, 4);

  // Set PA config for E22 (SX1262, max 22dBm)
  // paDutyCycle=0x04, hpMax=0x07, deviceSel=0x00, paLut=0x01
  uint8_t paConfig[] = {0x04, 0x07, 0x00, 0x01};
  sx1262_writeCommand(0x95, paConfig, 4);

  // Set TX params: power=22dBm, rampTime=0x04 (200us)
  uint8_t txParams[] = {(uint8_t)LORA_TX_POWER, 0x04};
  sx1262_writeCommand(0x8E, txParams, 2);

  // Set LoRa modulation params: SF, BW, CR, lowDataRateOptimize
  // lowDataRateOptimize=1 required for SF10+ with BW125
  uint8_t modParams[] = {
    (uint8_t)LORA_SF,
    (uint8_t)LORA_BW,
    (uint8_t)LORA_CR,
    0x01  // low data rate optimize ON
  };
  sx1262_writeCommand(0x8B, modParams, 4);

  // Set packet params: preamble=12, headerType=explicit, payloadLen=9,
  //                    CRC off (we do own CRC), invertIQ=off
  uint8_t pktParams[] = {0x00, 0x0C, 0x00, 9, 0x01, 0x00, 0x00};
  sx1262_writeCommand(0x8C, pktParams, 7);

  // Fix SX1262 sensitivity errata (datasheet 15.1)
  sx1262_writeRegister(0x0889, 0x08);
}

   //LORA TRANSMIT
void sx1262_transmit(uint8_t *payload, uint8_t len) {
  // Set buffer base addresses
  uint8_t bufBase[] = {0x00, 0x00};
  sx1262_writeCommand(0x8F, bufBase, 2);

  // Write payload to buffer
  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(0x0E);  
  SPI.transfer(0x00);  
  for (uint8_t i = 0; i < len; i++) SPI.transfer(payload[i]);
  digitalWrite(LORA_NSS, HIGH);

  // Set DIO1 to TxDone
  uint8_t dioMap[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sx1262_writeCommand(0x08, dioMap, 8);

  // Start TX (timeout=0 means no timeout)
  uint8_t txCmd[] = {0x00, 0x00, 0x00};
  sx1262_writeCommand(0x83, txCmd, 3);

  // Wait for TxDone on DATA1
  uint32_t start = millis();
  while (!digitalRead(LORA_DIO1)) {
    if (millis() - start > 5000) break; // failsafe timeout
  }

  // Clear IRQ
  uint8_t clrIrq[] = {0xFF, 0xFF};
  sx1262_writeCommand(0x02, clrIrq, 2);
}

   // Returns true if packet received within timeout_ms//
bool sx1262_receive(uint8_t *buf, uint8_t *rxLen, uint16_t timeout_ms) {
  // Set to RX mode
  uint8_t rxCmd[] = {0x00, 0x00, 0x00}; // continuous until packet or timeout
  sx1262_writeCommand(0x82, rxCmd, 3);

  // Set DIO1 to RxDone
  uint8_t dioMap[] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sx1262_writeCommand(0x08, dioMap, 8);

  uint32_t start = millis();
  while (!digitalRead(LORA_DIO1)) {
    if (millis() - start > timeout_ms) {
      // Back to standby
      uint8_t standby[] = {0x00};
      sx1262_writeCommand(0x80, standby, 1);
      return false;
    }
  }

  // Get RX buffer status
  uint8_t rxStatus[2];
  sx1262_readCommand(0x13, rxStatus, 2);
  *rxLen = rxStatus[0];
  uint8_t startAddr = rxStatus[1];

  // Read buffer
  sx1262_waitBusy();
  digitalWrite(LORA_NSS, LOW);
  SPI.transfer(0x1E);       // ReadBuffer
  SPI.transfer(startAddr);
  SPI.transfer(0x00);       // status
  for (uint8_t i = 0; i < *rxLen; i++) buf[i] = SPI.transfer(0x00);
  digitalWrite(LORA_NSS, HIGH);

  // Clear IRQ
  uint8_t clrIrq[] = {0xFF, 0xFF};
  sx1262_writeCommand(0x02, clrIrq, 2);

  // Back to standby
  uint8_t standby[] = {0x00};
  sx1262_writeCommand(0x80, standby, 1);

  return true;
}

//LORA SLEEP

void sx1262_sleep() {
  uint8_t sleepConfig[] = {0x04}; // cold start, RTC off
  sx1262_writeCommand(0x84, sleepConfig, 1);
}

 // WATCHDOG//

ISR(WDT_vect) {
  wdtFired = true;
}

// MCU SLEEP

void mcuSleep8s() {
  wdtFired = false;

  // Configure watchdog for 8s interrupt (not reset)
  MCUSR &= ~_BV(WDRF);
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR  = _BV(WDIE) | _BV(WDP3) | _BV(WDP0); // 8s, interrupt mode

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();

  wdt_disable();
}
  // Sleep for ~10 minutes (75 x 8s cycles)
void sleepTenMinutes() {
  sx1262_sleep();
  for (uint8_t i = 0; i < SLEEP_CYCLES; i++) {
    mcuSleep8s();
  }
}
 //BUILD AND SEND DATA WITH RETRY LOGIC

 void sendReading(uint16_t m1, uint16_t m2, uint16_t batt_mv) {
  Packet pkt;
  pkt.node_id    = nodeId;
  pkt.moisture1  = m1;
  pkt.moisture2  = m2;
  pkt.battery_mv = batt_mv;

  pkt.crc = crc16((uint8_t *)&pkt, sizeof(Packet) - 2);

  uint8_t *payload = (uint8_t *)&pkt;
  uint8_t  payloadLen = sizeof(Packet);

  for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    if (attempt > 0) {
      sx1262_init();
    }

    sx1262_transmit(payload, payloadLen);

    // Wait for ACK
    uint8_t ackBuf[4];
    uint8_t ackLen = 0;
    bool gotAck = sx1262_receive(ackBuf, &ackLen, ACK_TIMEOUT_MS);

    if (gotAck && ackLen >= 2) {
      
      if (ackBuf[0] == 0xAC && ackBuf[1] == nodeId) {
        break; // success
      }
    }
sx1262_sleep();
    delay(100);
  }
}

  ////SETUP///
  void setup() {
  // Read node ID from EEPROM
  nodeId = EEPROM.read(EEPROM_NODE_ID_ADDR);
  // Safety: if EEPROM unwritten (0xFF), default to 1
  if (nodeId == 0xFF) nodeId = 0x01;

  // Init SPI
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV16); // 500kHz @ 8MHz

  // Init LoRa
  sx1262_init();
}

///MAIN LOOP///
void loop() {
  // Read sensors
  uint16_t m1      = analogRead(MOISTURE_1_PIN);
  
  uint16_t batt_mv = readBatteryMv();

  // Transmit with retry
  sendReading(m1, batt_mv);

  // Sleep ~10 minutes
  sleepTenMinutes();
}
