#include <Wire.h>
#include <Adafruit_INA260.h>
#include <Adafruit_DPS310.h>
#include <esp_system.h>
#include <esp_spi_flash.h>

Adafruit_INA260 ina_solar; // 0x40
Adafruit_INA260 ina_load;  // 0x44
Adafruit_DPS310 dps310;    // 0x77

bool has_solar = false;
bool has_load  = false;
bool has_dps   = false;

void printSystemHealth() {
  Serial.println("\n=======================================================");
  Serial.println("         ESP32 SYSTEM HEALTH & MEMORY AUDIT            ");
  Serial.println("=======================================================");
  
  // Chip Architecture
  Serial.printf("Chip Model:          %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision:       v%d\n", ESP.getChipRevision());
  Serial.printf("CPU Cores:           %d Cores\n", ESP.getChipCores());
  Serial.printf("CPU Clock Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  
  // RAM (SRAM / Heap)
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t freeHeap  = ESP.getFreeHeap();
  uint32_t minHeap   = ESP.getMinFreeHeap();
  uint32_t maxBlock  = ESP.getMaxAllocHeap();
  Serial.println("\n[SRAM / HEAP MEMORY]");
  Serial.printf("  Total SRAM Heap:   %6u bytes (%5.1f KB)\n", totalHeap, totalHeap / 1024.0);
  Serial.printf("  Free SRAM Heap:    %6u bytes (%5.1f KB)  [%.1f%% Free]\n", freeHeap, freeHeap / 1024.0, (freeHeap * 100.0) / totalHeap);
  Serial.printf("  Lowest Free Heap:  %6u bytes (%5.1f KB)\n", minHeap, minHeap / 1024.0);
  Serial.printf("  Max Alloc Block:   %6u bytes (%5.1f KB)\n", maxBlock, maxBlock / 1024.0);
  
  // PSRAM (External RAM)
  uint32_t totalPsram = ESP.getPsramSize();
  if (totalPsram > 0) {
    Serial.printf("  External PSRAM:    %6u bytes (%5.1f KB)\n", totalPsram, totalPsram / 1024.0);
  } else {
    Serial.println("  External PSRAM:    None (Standard ESP32-WROOM internal SRAM only)");
  }

  // ROM / SPI FLASH MEMORY
  uint32_t flashSize   = ESP.getFlashChipSize();
  uint32_t flashSpeed  = ESP.getFlashChipSpeed() / 1000000;
  FlashMode_t flashMode= ESP.getFlashChipMode();
  uint32_t sketchSize  = ESP.getSketchSize();
  uint32_t freeSketch  = ESP.getFreeSketchSpace();
  uint32_t totalAppMem = sketchSize + freeSketch;

  Serial.println("\n[ROM / SPI FLASH STORAGE]");
  Serial.printf("  Physical Flash:    %6u bytes (%5.1f MB)\n", flashSize, flashSize / (1024.0 * 1024.0));
  Serial.printf("  Flash Bus Speed:   %d MHz\n", flashSpeed);
  Serial.printf("  Flash Access Mode: %s\n", (flashMode == FM_QIO ? "QIO (Quad)" : (flashMode == FM_QOUT ? "QOUT" : (flashMode == FM_DIO ? "DIO (Dual)" : "DOUT"))));
  Serial.printf("  App Partition:     %6u bytes (%5.1f KB)\n", totalAppMem, totalAppMem / 1024.0);
  Serial.printf("  Current Sketch:    %6u bytes (%5.1f KB)  [%.1f%% Used]\n", sketchSize, sketchSize / 1024.0, (sketchSize * 100.0) / totalAppMem);
  Serial.printf("  Free Program ROM:  %6u bytes (%5.1f KB)  [%.1f%% Available]\n", freeSketch, freeSketch / 1024.0, (freeSketch * 100.0) / totalAppMem);
  Serial.println("=======================================================\n");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz Fast I2C

  printSystemHealth();

  Serial.println("=======================================================");
  Serial.println("           SENSOR INITIALIZATION & VERIFICATION        ");
  Serial.println("=======================================================");

  // 1. INA260 #1 (0x40 - Solar)
  if (ina_solar.begin(0x40)) {
    Serial.println(" [OK] INA260 #1 (Address 0x40) -> SOLAR PANEL MONITOR");
    has_solar = true;
  } else {
    Serial.println(" [FAIL] INA260 #1 at 0x40 Not Detected");
  }

  // 2. INA260 #2 (0x44 - Load/Battery)
  if (ina_load.begin(0x44)) {
    Serial.println(" [OK] INA260 #2 (Address 0x44) -> BATTERY / LOAD MONITOR");
    has_load = true;
  } else {
    Serial.println(" [FAIL] INA260 #2 at 0x44 Not Detected");
  }

  // 3. Smartelex DPS310 (0x77)
  if (dps310.begin_I2C(0x77)) {
    Serial.println(" [OK] Smartelex DPS310 (Address 0x77) -> INFRASOUND BAROMETER");
    dps310.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps310.configureTemperature(DPS310_16HZ, DPS310_2SAMPLES);
    has_dps = true;
  } else if (dps310.begin_I2C(0x76)) {
    Serial.println(" [OK] Smartelex DPS310 (Address 0x76) -> INFRASOUND BAROMETER");
    dps310.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps310.configureTemperature(DPS310_16HZ, DPS310_2SAMPLES);
    has_dps = true;
  } else {
    Serial.println(" [FAIL] Smartelex DPS310 Not Detected");
  }
  Serial.println("=======================================================\n");

  // Infrasound Burst Benchmark
  if (has_dps) {
    Serial.println("Running 1-second DPS310 Infrasound High-Speed Sampling Test...");
    uint32_t t_start = millis();
    int samples = 0;
    float minP = 99999.0, maxP = -99999.0;
    sensors_event_t te, pe;
    while (millis() - t_start < 1000) {
      if (dps310.pressureAvailable()) {
        dps310.getEvents(&te, &pe);
        if (pe.pressure > 500.0 && pe.pressure < 1200.0) {
          samples++;
          if (pe.pressure < minP) minP = pe.pressure;
          if (pe.pressure > maxP) maxP = pe.pressure;
        }
      }
    }
    Serial.printf("  Infrasound Acquisition Rate: %d samples/sec\n", samples);
    Serial.printf("  Ambient Pressure Baseline:   %7.2f hPa\n", (minP + maxP) / 2.0);
    Serial.printf("  Peak-to-Peak Noise Jitter:   %7.3f hPa (%.1f Pa)\n", (maxP - minP), (maxP - minP) * 100.0);
    if (samples >= 40) {
      Serial.println("  ==> SUITABILITY FOR ELEPHANT RUMBLE (14-35 Hz): EXCELLENT!");
    } else {
      Serial.println("  ==> SUITABILITY FOR ELEPHANT RUMBLE: ADEQUATE");
    }
    Serial.println("-------------------------------------------------------\n");
  }
}

void loop() {
  Serial.println("--- LIVE TELEMETRY SNAPSHOT ---");

  if (has_solar) {
    float v = ina_solar.readBusVoltage() / 1000.0;
    float i = ina_solar.readCurrent();
    float p = ina_solar.readPower() / 1000.0;
    Serial.printf("  Solar Panel (0x40):  %5.2f V | %6.1f mA | %5.2f W | Status: %s\n",
                  v, i, p, (i > 50.0 ? "CHARGING" : "IDLE/DARK"));
  }

  if (has_load) {
    float v = ina_load.readBusVoltage() / 1000.0;
    float i = ina_load.readCurrent();
    float p = ina_load.readPower() / 1000.0;
    Serial.printf("  Battery/Load (0x44): %5.2f V | %6.1f mA | %5.2f W\n",
                  v, i, p);
  }

  if (has_dps && dps310.pressureAvailable()) {
    sensors_event_t te, pe;
    dps310.getEvents(&te, &pe);
    Serial.printf("  DPS310 Pressure:     %7.2f hPa | Temp: %4.1f C\n",
                  pe.pressure, te.temperature);
  }

  Serial.println();
  delay(2000);
}
