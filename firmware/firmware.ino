#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_INA260.h>
#include <Adafruit_DPS310.h>

// ==========================================
// 1. CONFIGURATION & NETWORK CREDENTIALS
// ==========================================
const char* WIFI_SSID       = "Paari";
const char* WIFI_PASS       = "Pandi@24";

// High-performance Cloud Broker (supports WebSockets for vnc.inayat.net & localhost)
const char* MQTT_BROKER     = "broker.hivemq.com";
const int   MQTT_PORT       = 1883;

// Telemetry MQTT Topics
const char* TOPIC_POWER     = "vananetra/VN-01/power";
const char* TOPIC_RUMBLES   = "vananetra/VN-01/rumbles";
const char* TOPIC_ALERTS    = "vananetra/VN-01/alerts";

const char* NODE_ID         = "VN-01";

// ==========================================
// 2. HARDWARE OBJECTS & PERSISTENT STORAGE
// ==========================================
Adafruit_INA260 ina_solar; // 0x40 - Solar generation
Adafruit_INA260 ina_load;  // 0x44 - Battery & Load draw
Adafruit_DPS310 dps310;    // 0x77 - High-speed Infrasound barometer

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer localServer(80); // Local REST API on port 80
Preferences prefs;         // Non-volatile Flash storage (survives reboots)

bool has_solar = false;
bool has_load  = false;
bool has_dps   = false;

// Energy Accumulators (Watt-Hours)
float total_solar_wh = 0.0f;
float total_load_wh  = 0.0f;
unsigned long lastNvsSaveTime = 0;

// 1-second power timer
unsigned long lastPowerTime = 0;
const unsigned long POWER_INTERVAL_MS = 1000;

// High-speed 64Hz Pressure Ring Buffer for Rumbles
const int BUFFER_SIZE = 64;
float pressureBuffer[BUFFER_SIZE];
int bufferIndex = 0;
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_US = 15400; // ~15.4ms (64.9 Hz)

// Infrasound Rumble Detection Thresholds
float baselinePressure = 996.75;
float rumbleThresholdPa = 8.0; // 8 Pa anomaly threshold
bool rumbleTriggered = false;

// Global latest telemetry cache for local HTTP API
StaticJsonDocument<512> cachedPowerDoc;

// ==========================================
// 3. PERSISTENT STORAGE (NVS FLASH)
// ==========================================
void loadEnergyFromFlash() {
  prefs.begin("vnc_energy", false);
  total_solar_wh = prefs.getFloat("sol_wh", 0.0f);
  total_load_wh  = prefs.getFloat("load_wh", 0.0f);
  Serial.printf("💾 [NVS Flash] Loaded Persistent Totals -> Solar: %.3f Wh | Load: %.3f Wh\n",
                total_solar_wh, total_load_wh);
}

void saveEnergyToFlash() {
  prefs.putFloat("sol_wh", total_solar_wh);
  prefs.putFloat("load_wh", total_load_wh);
  Serial.printf("💾 [NVS Flash] Saved Totals -> Solar: %.3f Wh | Load: %.3f Wh\n",
                total_solar_wh, total_load_wh);
}

// ==========================================
// 4. LOCAL REST API SERVER (Direct sync with Dashboard)
// ==========================================
void setupLocalServer() {
  localServer.on("/api/telemetry", HTTP_GET, []() {
    localServer.sendHeader("Access-Control-Allow-Origin", "*");
    localServer.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    localServer.sendHeader("Access-Control-Allow-Headers", "*");

    String output;
    serializeJson(cachedPowerDoc, output);
    localServer.send(200, "application/json", output);
  });

  localServer.on("/api/reset_energy", HTTP_POST, []() {
    localServer.sendHeader("Access-Control-Allow-Origin", "*");
    total_solar_wh = 0.0f;
    total_load_wh  = 0.0f;
    saveEnergyToFlash();
    localServer.send(200, "application/json", "{\"status\":\"energy_reset_ok\"}");
  });

  localServer.begin();
  Serial.println("🌐 [Local HTTP API] Active at http://192.168.1.38/api/telemetry");
}

// ==========================================
// 5. NETWORK MANAGEMENT
// ==========================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\n[WiFi] Connection timeout, will retry in background.");
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;

  static unsigned long lastMqttAttempt = 0;
  if (millis() - lastMqttAttempt < 4000) return;
  lastMqttAttempt = millis();

  Serial.printf("[MQTT] Connecting to broker %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  String clientId = "Vananetra-Sentry-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("✅ [MQTT] Connected to Broker (Synced with WebSockets)!");
    mqttClient.publish(TOPIC_ALERTS, "{\"status\":\"online\",\"node\":\"VN-01\"}");
  } else {
    Serial.printf("❌ [MQTT] Connection failed, state: %d\n", mqttClient.state());
  }
}

int calculateLiFePO4SoC(float v) {
  if (v >= 13.6) return 100;
  if (v >= 13.4) return 99;
  if (v >= 13.3) return 90;
  if (v >= 13.2) return 70;
  if (v >= 13.1) return 40;
  if (v >= 13.0) return 20;
  if (v >= 12.8) return 10;
  return 0;
}

void setupDPS310HighSpeed() {
  dps310.setMode(DPS310_IDLE);
  delay(10);
  dps310.configurePressure(DPS310_64HZ, DPS310_1SAMPLE);
  dps310.setMode(DPS310_CONT_PRESSURE);
  delay(20);
  Serial.println("✅ [DPS310] High-speed continuous 64Hz acquisition active.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==================================================");
  Serial.println("   VANANETRA ELEPHANT SENTRY & SOLAR FIRMWARE     ");
  Serial.println("==================================================");

  Wire.begin(21, 22);
  Wire.setClock(400000);

  // Load persistent energy counters from flash
  loadEnergyFromFlash();

  // Sensors
  if (ina_solar.begin(0x40)) has_solar = true;
  if (ina_load.begin(0x44))  has_load  = true;
  if (dps310.begin_I2C(0x77) || dps310.begin_I2C(0x76)) {
    has_dps = true;
    setupDPS310HighSpeed();
  }

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(2048);

  setupLocalServer();
  Serial.println("==================================================\n");
}

void loop() {
  unsigned long now = millis();
  unsigned long nowMicros = micros();

  // Network and Local API handling
  if (WiFi.status() == WL_CONNECTED) {
    reconnectMQTT();
    mqttClient.loop();
    localServer.handleClient();
  } else {
    static unsigned long lastWifiCheck = 0;
    if (now - lastWifiCheck > 10000) {
      lastWifiCheck = now;
      connectWiFi();
    }
  }

  // --- TASK 1: HIGH-SPEED PRESSURE ACQUISITION (64 HZ) ---
  if (has_dps && (nowMicros - lastSampleTime >= SAMPLE_INTERVAL_US)) {
    lastSampleTime = nowMicros;

    if (dps310.pressureAvailable()) {
      sensors_event_t p_event;
      if (dps310.getEvents(NULL, &p_event)) {
        float p = p_event.pressure;
        if (p > 300.0 && p < 1200.0) {
          pressureBuffer[bufferIndex++] = p;
          baselinePressure = (baselinePressure * 0.999f) + (p * 0.001f);
        }
      }
    }

    if (bufferIndex >= BUFFER_SIZE) {
      bufferIndex = 0;

      float sumSquares = 0.0f;
      float maxDevPa = 0.0f;
      for (int i = 0; i < BUFFER_SIZE; i++) {
        float devPa = (pressureBuffer[i] - baselinePressure) * 100.0f;
        sumSquares += (devPa * devPa);
        if (abs(devPa) > maxDevPa) maxDevPa = abs(devPa);
      }
      float rmsEnergy = sqrt(sumSquares / BUFFER_SIZE);

      rumbleTriggered = (rmsEnergy >= rumbleThresholdPa || maxDevPa >= (rumbleThresholdPa * 1.5f));

      if (mqttClient.connected()) {
        StaticJsonDocument<1536> rDoc;
        rDoc["node_id"]         = NODE_ID;
        rDoc["ts_ms"]           = now;
        rDoc["rms_energy"]      = round(rmsEnergy * 100.0f) / 100.0f;
        rDoc["peak_pa"]         = round(maxDevPa * 100.0f) / 100.0f;
        rDoc["rumble_detected"] = rumbleTriggered;

        JsonArray arr = rDoc.createNestedArray("samples");
        for (int i = 0; i < BUFFER_SIZE; i++) {
          arr.add(round(pressureBuffer[i] * 1000.0f) / 1000.0f);
        }

        char rBuf[1536];
        serializeJson(rDoc, rBuf);
        mqttClient.publish(TOPIC_RUMBLES, rBuf);
      }
    }
  }

  // --- TASK 2: 1-SECOND POWER, ENERGY ACCUMULATION & TELEMETRY ---
  if (now - lastPowerTime >= POWER_INTERVAL_MS) {
    float elapsedSec = (now - lastPowerTime) / 1000.0f;
    lastPowerTime = now;

    float s_v = has_solar ? (ina_solar.readBusVoltage() / 1000.0f) : 0.0f;
    float s_i = has_solar ? ina_solar.readCurrent() : 0.0f;
    float s_p = (s_v * s_i) / 1000.0f;
    if (s_p < 0.0f) s_p = 0.0f;
    bool  is_charging = (s_i > 50.0f);

    float b_v = has_load  ? (ina_load.readBusVoltage() / 1000.0f) : 0.0f;
    float l_i = has_load  ? ina_load.readCurrent() : 0.0f;
    // Clean threshold: If load is turned off, filter out small shunt noise (-1mA to 2mA)
    if (l_i < 5.0f && l_i > -5.0f) l_i = 0.0f;
    float l_p = (b_v * l_i) / 1000.0f;
    if (l_p < 0.0f) l_p = 0.0f;

    int soc = calculateLiFePO4SoC(b_v);

    // Integrate Total Energy (Watt-hours)
    total_solar_wh += (s_p * (elapsedSec / 3600.0f));
    total_load_wh  += (l_p * (elapsedSec / 3600.0f));

    // Save to Flash every 60 seconds
    if (now - lastNvsSaveTime >= 60000) {
      lastNvsSaveTime = now;
      saveEnergyToFlash();
    }

    // Cache latest power document for local REST API
    cachedPowerDoc.clear();
    cachedPowerDoc["node_id"]        = NODE_ID;
    cachedPowerDoc["uptime_s"]       = now / 1000;
    cachedPowerDoc["wifi_rssi"]      = WiFi.RSSI();
    cachedPowerDoc["solar_v"]        = round(s_v * 100.0f) / 100.0f;
    cachedPowerDoc["solar_i"]        = round(s_i * 10.0f) / 10.0f;
    cachedPowerDoc["solar_p"]        = round(s_p * 100.0f) / 100.0f;
    cachedPowerDoc["total_solar_wh"] = round(total_solar_wh * 100.0f) / 100.0f;
    cachedPowerDoc["solar_charging"] = is_charging;
    cachedPowerDoc["battery_v"]      = round(b_v * 100.0f) / 100.0f;
    cachedPowerDoc["battery_soc"]    = soc;
    cachedPowerDoc["load_i"]         = round(l_i * 10.0f) / 10.0f;
    cachedPowerDoc["load_p"]         = round(l_p * 100.0f) / 100.0f;
    cachedPowerDoc["total_load_wh"]  = round(total_load_wh * 100.0f) / 100.0f;
    cachedPowerDoc["ambient_pres"]   = round(baselinePressure * 100.0f) / 100.0f;
    cachedPowerDoc["rumble_alert"]   = rumbleTriggered;

    // Print to Local Serial
    Serial.printf("[1s Power] Bat: %5.2fV (%3d%%) | Load: %5.1fmA | Fan: %4.2fW (Tot: %.2fWh) | Sol: %4.2fW (Tot: %.2fWh)\n",
                  b_v, soc, l_i, l_p, total_load_wh, s_p, total_solar_wh);

    // Publish to MQTT
    if (mqttClient.connected()) {
      char pBuf[512];
      serializeJson(cachedPowerDoc, pBuf);
      mqttClient.publish(TOPIC_POWER, pBuf);
    }
  }
}
