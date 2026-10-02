# Vananetra Central (VNC) — Solar, Battery & Infrasonic Sentry Operations

**Production Web Endpoint:** [`https://vnc.inayat.net/`](https://vnc.inayat.net/)  
**MQTT Broker:** `mqtt.ezhililabs.com:1883`  
**Node ID:** `VN-01`

---

## 1. Overview
**Vananetra Central** is an edge-sentinel wildlife corridor monitoring station engineered to operate continuously in remote forest corridors under solar and LiFePO4 battery power. 

The station performs two continuous mission-critical roles:
1. **Power Management:** High-precision dual-channel energy monitoring (Solar Generation vs. LiFePO4 Battery Storage vs. Load Demand) using dual **Texas Instruments INA260** sensors over $I^2C$.
2. **Elephant Rumble Sentinel:** High-speed **64.9 Hz** acoustic micro-barometric sampling via the **Smartelex / Infineon DPS310** sensor to capture and classify low-frequency (14–35 Hz) elephant infrasound rumbles.

---

## 2. Hardware Architecture & Pinout

### ESP32-WROOM-32 Wiring:
* **SDA:** `GPIO 21` (Connected in parallel to both INA260 boards and DPS310)
* **SCL:** `GPIO 22` (Connected in parallel to both INA260 boards and DPS310)
* **3V3:** Powers all three sensors
* **GND:** Common Ground (Tied to CM-D20 Battery Negative)

### $I^2C$ Addressing:
* **INA260 #1 (`0x40`):** Solar Panel input ($V_{solar}, I_{solar}, P_{solar}$).
* **INA260 #2 (`0x44`):** Battery & Load demand ($V_{bat}, I_{load}, P_{load}$).
* **Smartelex DPS310 (`0x77`):** Continuous 64.9 Hz barometric pressure.

---

## 3. Firmware Features
* **Wi-Fi Auto-Reconnect:** Connects automatically to forest station Wi-Fi (`Paari`).
* **MQTT Telemetry:** Publishes every second to:
  * `vananetra/VN-01/power`: Battery voltage, SoC %, fan current draw, solar wattage.
  * `vananetra/VN-01/rumbles`: 64-sample waveform buffer, RMS energy in Pascals, rumble alert status.
* **Non-blocking Continuous Pressure Mode:** Temperature conversions disabled to maintain a uniform **$15.4\text{ ms}$ cadence**.

---

## 4. Web Dashboard (`vnc.inayat.net`)
* **Security Lock:** Modern PIN / Passcode screen (Default PIN: `2026`).
* **Solar & Battery KPI Cards:** Real-time generation, consumption, and LiFePO4 health.
* **Infrasonic Oscillogram:** Dynamic 64 Hz pressure waveform visualizer.
* **Alert System:** Visual banner and audio chime when infrasound energy exceeds 8 Pa.
* **Vercel Ready:** Fully configured with `vercel.json` for one-click deployment.
