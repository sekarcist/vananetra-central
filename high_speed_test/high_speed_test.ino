#include <Wire.h>
#include <Adafruit_INA260.h>
#include <Adafruit_DPS310.h>

Adafruit_INA260 ina_solar; // 0x40
Adafruit_INA260 ina_load;  // 0x44
Adafruit_DPS310 dps310;    // 0x77

bool has_solar = false;
bool has_load  = false;
bool has_dps   = false;

unsigned long lastInaTime = 0;
const unsigned long INA_INTERVAL_MS = 1000; // 1 Hz (once per second)

void setupDPS310HighSpeed() {
  // 1. Put sensor into IDLE mode first so register writes are accepted by the state machine
  dps310.setMode(DPS310_IDLE);
  delay(10);

  // 2. Configure Pressure: 64 Hz rate with 1 SAMPLE (3.6ms conversion time, perfect for 14-35Hz rumbles)
  // For 64Hz: DPS310_64HZ, DPS310_1SAMPLE
  dps310.configurePressure(DPS310_64HZ, DPS310_1SAMPLE);

  // 3. Clear FIFO and enable continuous pressure mode
  // Register 0x08 (MEAS_CFG): 0b101 = Continuous Pressure
  dps310.setMode(DPS310_CONT_PRESSURE);
  delay(20);

  Serial.println("#DPS310_HIGH_SPEED_64HZ_CONFIGURED");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz Fast I2C

  Serial.println("\n#INIT_START");

  // 1. INA260 Solar (0x40)
  if (ina_solar.begin(0x40)) {
    has_solar = true;
    Serial.println("#INA260_SOLAR_OK_0x40");
  } else {
    Serial.println("#INA260_SOLAR_FAIL");
  }

  // 2. INA260 Load / Battery (0x44)
  if (ina_load.begin(0x44)) {
    has_load = true;
    Serial.println("#INA260_LOAD_OK_0x44");
  } else {
    Serial.println("#INA260_LOAD_FAIL");
  }

  // 3. DPS310 Pressure Sensor (0x77 or 0x76)
  if (dps310.begin_I2C(0x77) || dps310.begin_I2C(0x76)) {
    has_dps = true;
    Serial.println("#DPS310_OK");

    // Configure for high-speed continuous acquisition
    setupDPS310HighSpeed();
  } else {
    Serial.println("#DPS310_FAIL");
  }

  Serial.println("#DATA_START");
}

void loop() {
  unsigned long now = millis();

  // --- 1. HIGH-SPEED PRESSURE ACQUISITION ---
  if (has_dps && dps310.pressureAvailable()) {
    sensors_event_t p_event;
    if (dps310.getEvents(NULL, &p_event)) {
      if (p_event.pressure > 300.0 && p_event.pressure < 1200.0) {
        // Output: P,timestamp_ms,pressure_hPa
        Serial.printf("P,%lu,%.3f\n", now, p_event.pressure);
      }
    }
  }

  // --- 2. 1-SECOND INA260 POWER ACQUISITION ---
  if (now - lastInaTime >= INA_INTERVAL_MS) {
    lastInaTime = now;

    float s_v = has_solar ? (ina_solar.readBusVoltage() / 1000.0) : 0.0;
    float s_i = has_solar ? ina_solar.readCurrent() : 0.0;
    float s_p = has_solar ? (ina_solar.readPower() / 1000.0) : 0.0;

    float l_v = has_load ? (ina_load.readBusVoltage() / 1000.0) : 0.0;
    float l_i = has_load ? ina_load.readCurrent() : 0.0;
    float l_p = has_load ? (ina_load.readPower() / 1000.0) : 0.0;

    // Output: B,timestamp_ms,solar_v,solar_mA,solar_mW,load_v,load_mA,load_mW
    Serial.printf("B,%lu,%.2f,%.1f,%.2f,%.2f,%.1f,%.2f\n",
                  now, s_v, s_i, s_p, l_v, l_i, l_p);
  }
}
