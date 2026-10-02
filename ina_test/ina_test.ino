#include <Wire.h>
#include <Adafruit_INA260.h>
#include <Adafruit_DPS310.h>

Adafruit_INA260 ina1; // 0x40
Adafruit_INA260 ina2; // 0x44
Adafruit_DPS310 dps;  // 0x77 / 0x76

bool has_ina1 = false;
bool has_ina2 = false;
bool has_dps  = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(21, 22);

  Serial.println("\n================================================");
  Serial.println("  ESP32 SENSOR DIAGNOSTIC: INA260 & DPS310");
  Serial.println("================================================");

  // Check INA260 #1 (0x40)
  if (ina1.begin(0x40)) {
    Serial.println("✅ INA260 #1 detected at 0x40 (Solar Input)");
    has_ina1 = true;
  } else {
    Serial.println("ℹ️  INA260 #1 at 0x40: Not connected");
  }

  // Check INA260 #2 (0x44)
  if (ina2.begin(0x44)) {
    Serial.println("✅ INA260 #2 detected at 0x44 (Load / Battery)");
    has_ina2 = true;
  } else {
    Serial.println("ℹ️  INA260 #2 at 0x44: Not connected");
  }

  // Check Smartelex DPS310 (0x77 or 0x76)
  if (dps.begin_I2C(0x77)) {
    Serial.println("✅ Smartelex DPS310 detected at 0x77");
    dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps.configureTemperature(DPS310_16HZ, DPS310_2SAMPLES);
    has_dps = true;
  } else if (dps.begin_I2C(0x76)) {
    Serial.println("✅ Smartelex DPS310 detected at 0x76");
    dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps.configureTemperature(DPS310_16HZ, DPS310_2SAMPLES);
    has_dps = true;
  } else {
    Serial.println("ℹ️  DPS310 Pressure Sensor: Not connected");
  }

  Serial.println("------------------------------------------------\n");
}

void loop() {
  if (has_ina2) {
    float volt = ina2.readBusVoltage() / 1000.0; // In Volts
    float curr = ina2.readCurrent();             // In mA
    float pwr  = ina2.readPower() / 1000.0;      // In Watts

    Serial.printf("[INA260 @ 0x44] Bus Voltage: %6.2f V | Current: %7.1f mA | Power: %6.2f W\n",
                  volt, curr, pwr);
  }

  if (has_ina1) {
    float volt = ina1.readBusVoltage() / 1000.0;
    float curr = ina1.readCurrent();
    float pwr  = ina1.readPower() / 1000.0;

    Serial.printf("[INA260 @ 0x40] Bus Voltage: %6.2f V | Current: %7.1f mA | Power: %6.2f W\n",
                  volt, curr, pwr);
  }

  if (has_dps && dps.pressureAvailable()) {
    sensors_event_t temp_evt, pres_evt;
    dps.getEvents(&temp_evt, &pres_evt);
    Serial.printf("[DPS310]       Pressure: %7.2f hPa | Temp: %5.1f °C\n",
                  pres_evt.pressure, temp_evt.temperature);
  }

  delay(1500);
}
