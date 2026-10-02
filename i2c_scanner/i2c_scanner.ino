#include <Wire.h>

void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 I2C Scanner Started ---");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning I2C bus (SDA: GPIO 21, SCL: GPIO 22)...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(">>> FOUND I2C DEVICE AT 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      
      if (address == 0x40) {
        Serial.print("  [INA260 Default (A0=GND, A1=GND)]");
      } else if (address == 0x41) {
        Serial.print("  [INA260 (A0=VS+, A1=GND)]");
      } else if (address == 0x44) {
        Serial.print("  [INA260 (A0=GND, A1=VS+)]");
      } else if (address == 0x76 || address == 0x77) {
        Serial.print("  [Smartelex DPS310 / BME280]");
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("❌ No I2C devices found! Please check SDA (21), SCL (22), 3V3, and GND wires.");
  } else {
    Serial.printf("Scan complete. Total devices detected: %d\n", nDevices);
  }
  Serial.println("--------------------------------------------------");

  delay(3000);
}
