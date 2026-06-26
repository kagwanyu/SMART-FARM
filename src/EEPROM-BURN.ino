// KYNEXT — Node ID Burner
// Flash this sketch ONCE per node to set its ID in EEPROM
// Then reflash with kynext_node.ino

#include <EEPROM.h>

// *** SET THIS PER NODE BEFORE FLASHING ***
#define THIS_NODE_ID  0x01   // 1–254. Each node must be unique.

#define EEPROM_NODE_ID_ADDR 0x00

void setup() {
  EEPROM.write(EEPROM_NODE_ID_ADDR, THIS_NODE_ID);
  // Blink built-in or any LED to confirm write
  // (wire an LED to PB2 with a 330R resistor for visual confirmation)
  pinMode(PB2, OUTPUT);
  for (uint8_t i = 0; i < THIS_NODE_ID; i++) {
    digitalWrite(PB2, HIGH); delay(200);
    digitalWrite(PB2, LOW);  delay(200);
  }
}

void loop() {
  // blink slowly to show done
  digitalWrite(PB2, HIGH); delay(1000);
  digitalWrite(PB2, LOW);  delay(1000);
}