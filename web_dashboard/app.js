/* ========================================================
   VANANETRA CENTRAL (VNC) - JAVASCRIPT CORE
   Production Endpoint: vnc.inayat.net (Vercel)
   ======================================================== */

// --- 1. STATE & CREDENTIALS ---
const CORRECT_PIN = "2026";
let currentPin = "";

const isHttps = window.location.protocol === "https:";
const MQTT_WS_BROKER = isHttps 
    ? "wss://broker.hivemq.com:8884/mqtt" 
    : "ws://broker.hivemq.com:8000/mqtt";
const TOPIC_POWER = "vananetra/VN-01/power";
const TOPIC_RUMBLES = "vananetra/VN-01/rumbles";
const TOPIC_ALERTS = "vananetra/VN-01/alerts";

let mqttClient = null;
let waveformChart = null;
let powerChart = null;

let isBandpassActive = true;
let recordedPowerHistory = [];
let recordedRumbleHistory = [];
let baselinePressure = 996.75;

// --- 2. PIN LOCK LOGIC ---
function pressKey(num) {
    if (currentPin.length < 4) {
        currentPin += num;
        updatePinDots();
        if (currentPin.length === 4) {
            validatePin();
        }
    }
}

function clearPin() {
    currentPin = "";
    updatePinDots();
    document.getElementById("lockErrorMsg").innerText = "Enter Security PIN (Default: 2026)";
    document.getElementById("lockErrorMsg").classList.remove("error");
}

function updatePinDots() {
    for (let i = 1; i <= 4; i++) {
        const dot = document.getElementById(`dot${i}`);
        if (i <= currentPin.length) {
            dot.classList.add("filled");
        } else {
            dot.classList.remove("filled");
        }
    }
}

function validatePin() {
    if (currentPin === CORRECT_PIN) {
        unlockDashboard();
    } else {
        const msg = document.getElementById("lockErrorMsg");
        msg.innerText = "Incorrect PIN. Try 2026.";
        msg.classList.add("error");
        setTimeout(clearPin, 600);
    }
}

function quickDemoUnlock() {
    currentPin = CORRECT_PIN;
    updatePinDots();
    setTimeout(unlockDashboard, 150);
}

function unlockDashboard() {
    sessionStorage.setItem("vnc_auth", "true");
    const lockScreen = document.getElementById("lockScreen");
    const mainDash = document.getElementById("mainDashboard");
    
    lockScreen.classList.add("hidden");
    mainDash.classList.remove("hidden");

    initCharts();
    initMQTT();
    startUptimeCounter();
}

function lockSystem() {
    sessionStorage.removeItem("vnc_auth");
    location.reload();
}

// Check session on load
window.addEventListener("DOMContentLoaded", () => {
    if (sessionStorage.getItem("vnc_auth") === "true") {
        quickDemoUnlock();
    }
});

// Keyboard support for PIN entry
window.addEventListener("keydown", (e) => {
    if (document.getElementById("lockScreen").classList.contains("hidden")) return;
    if (e.key >= "0" && e.key <= "9") {
        pressKey(e.key);
    } else if (e.key === "Backspace" || e.key === "Escape") {
        clearPin();
    } else if (e.key === "Enter" && currentPin.length === 4) {
        validatePin();
    }
});

// --- 3. CHARTS INITIALIZATION ---
function initCharts() {
    // A. 64Hz Infrasonic Waveform Chart
    const ctxWave = document.getElementById("waveformChart").getContext("2d");
    const initialWaveData = Array(128).fill(0);
    const waveLabels = Array(128).fill("");

    waveformChart = new Chart(ctxWave, {
        type: "line",
        data: {
            labels: waveLabels,
            datasets: [
                {
                    label: "Infrasound Amplitude (Pa)",
                    data: initialWaveData,
                    borderColor: "#06B6D4",
                    borderWidth: 2,
                    pointRadius: 0,
                    tension: 0.25,
                    fill: false
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            scales: {
                x: { display: false },
                y: {
                    min: -25,
                    max: 25,
                    grid: { color: "rgba(255, 255, 255, 0.05)" },
                    ticks: { color: "#64748B", font: { family: "JetBrains Mono" } }
                }
            },
            plugins: {
                legend: { display: false }
            }
        }
    });

    // B. Power Flow Time-Series Chart (60s)
    const ctxPower = document.getElementById("powerChart").getContext("2d");
    const powerLabels = Array(30).fill("");
    const initialSolar = Array(30).fill(0);
    const initialLoad = Array(30).fill(1.91);
    const initialBat = Array(30).fill(13.92);

    powerChart = new Chart(ctxPower, {
        type: "line",
        data: {
            labels: powerLabels,
            datasets: [
                {
                    label: "Solar Watts",
                    data: initialSolar,
                    borderColor: "#F59E0B",
                    backgroundColor: "rgba(245, 158, 11, 0.1)",
                    borderWidth: 2,
                    tension: 0.3,
                    fill: true,
                    yAxisID: "y"
                },
                {
                    label: "Load Watts",
                    data: initialLoad,
                    borderColor: "#10B981",
                    backgroundColor: "rgba(16, 185, 129, 0.1)",
                    borderWidth: 2,
                    tension: 0.3,
                    fill: true,
                    yAxisID: "y"
                },
                {
                    label: "Battery Volts",
                    data: initialBat,
                    borderColor: "#818CF8",
                    borderWidth: 1.5,
                    borderDash: [4, 4],
                    pointRadius: 0,
                    tension: 0.2,
                    fill: false,
                    yAxisID: "y1"
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 400 },
            scales: {
                x: { display: false },
                y: {
                    position: "left",
                    min: 0,
                    max: 15,
                    title: { display: true, text: "Power (Watts)", color: "#64748B" },
                    grid: { color: "rgba(255, 255, 255, 0.05)" },
                    ticks: { color: "#64748B", font: { family: "JetBrains Mono" } }
                },
                y1: {
                    position: "right",
                    min: 10,
                    max: 16,
                    title: { display: true, text: "Battery (V)", color: "#64748B" },
                    grid: { display: false },
                    ticks: { color: "#818CF8", font: { family: "JetBrains Mono" } }
                }
            },
            plugins: {
                legend: { display: false }
            }
        }
    });
}

// --- 4. MQTT REAL-TIME BROKER CLIENT ---
function initMQTT() {
    logMessage(`Connecting to HiveMQ Bridge (${MQTT_WS_BROKER})...`);

    const clientId = "VNC-Web-" + Math.random().toString(16).substr(2, 8);
    try {
        mqttClient = mqtt.connect(MQTT_WS_BROKER, {
            clientId: clientId,
            clean: true,
            keepalive: 60,
            reconnectPeriod: 2500
        });

        mqttClient.on("connect", () => {
            logMessage("✅ MQTT Broker Connected! Subscribed to vananetra/# topics.", "log-success");
            document.getElementById("brokerStatusPill").classList.add("status-live");
            document.getElementById("brokerStatusText").innerText = "BROKER: LIVE (SYNCED)";

            mqttClient.subscribe("vananetra/VN-01/#");
        });

        mqttClient.on("message", (topic, payload) => {
            try {
                const data = JSON.parse(payload.toString());
                handleIncomingTelemetry(topic, data);
            } catch (err) {
                console.error("JSON parse error:", err);
            }
        });

        mqttClient.on("error", (err) => {
            console.warn("MQTT error:", err);
            document.getElementById("brokerStatusPill").classList.remove("status-live");
            document.getElementById("brokerStatusText").innerText = "BROKER: RECONNECTING...";
        });

        mqttClient.on("close", () => {
            document.getElementById("brokerStatusPill").classList.remove("status-live");
            document.getElementById("brokerStatusText").innerText = "BROKER: RECONNECTING...";
        });
    } catch (e) {
        logMessage("WebSocket connection failed: " + e.message, "log-alert");
    }
}

function handleIncomingTelemetry(topic, data) {
    if (topic.includes("power")) {
        updatePowerUI(data);
    } else if (topic.includes("rumbles")) {
        updateRumblesUI(data);
    }
}

// --- 5. UI DATA UPDATES ---
function updatePowerUI(data) {
    // Solar
    document.getElementById("solarWatts").innerText = data.solar_p.toFixed(2);
    document.getElementById("solarVolts").innerText = `${data.solar_v.toFixed(2)} V`;
    document.getElementById("solarCurrent").innerText = `${data.solar_i.toFixed(1)} mA`;
    document.getElementById("solarProgress").style.width = `${Math.min(100, (data.solar_p / 50) * 100)}%`;
    document.getElementById("solarBadge").innerText = data.solar_charging ? "CHARGING" : "STANDBY / DARK";

    // Lifetime Total Solar Generated (Wh)
    if (data.total_solar_wh !== undefined) {
        const solWhEl = document.getElementById("totalSolarWh");
        if (solWhEl) solWhEl.innerText = `${data.total_solar_wh.toFixed(2)} Wh`;
    }

    // Battery (Primary 4S LiFePO4 Station)
    document.getElementById("batVolts").innerText = data.battery_v.toFixed(2);
    document.getElementById("batSoc").innerText = `${data.battery_soc}%`;
    document.getElementById("batProgress").style.width = `${data.battery_soc}%`;

    // Backup Battery (1S Li-ion 1500mAh)
    const backupEl = document.getElementById("backupBatVal");
    if (backupEl) {
        if (data.backup_v !== undefined && data.backup_v >= 2.5) {
            backupEl.innerText = `${data.backup_v.toFixed(2)} V (${data.backup_soc}%)`;
            backupEl.style.color = data.backup_soc > 20 ? "#38BDF8" : "#F87171";
        } else {
            backupEl.innerText = "Not Connected";
            backupEl.style.color = "#94A3B8";
        }
    }

    // Load
    document.getElementById("loadWatts").innerText = data.load_p.toFixed(2);
    document.getElementById("loadVolts").innerText = `${data.battery_v.toFixed(2)} V`;
    document.getElementById("loadCurrent").innerText = `${data.load_i.toFixed(1)} mA`;
    document.getElementById("loadProgress").style.width = `${Math.min(100, (data.load_p / 10) * 100)}%`;

    // Lifetime Total Load Consumed (Wh)
    if (data.total_load_wh !== undefined) {
        const loadWhEl = document.getElementById("totalLoadWh");
        if (loadWhEl) loadWhEl.innerText = `${data.total_load_wh.toFixed(2)} Wh`;
    }

    // Sentry Hardware Uptime
    if (data.uptime_s !== undefined) {
        hardwareUptimeReceived = true;
        const mins = Math.floor(data.uptime_s / 60);
        const secs = data.uptime_s % 60;
        document.getElementById("uptimeVal").innerText = `Sentry Uptime: ${mins}m ${secs}s`;
    }

    // Net balance
    const net = data.solar_p - data.load_p;
    document.getElementById("netPowerStat").innerText = `${net >= 0 ? "+" : ""}${net.toFixed(2)} W (${net >= 0 ? "Charging" : "Discharging"})`;

    // WiFi RSSI
    if (data.wifi_rssi) {
        document.getElementById("wifiRssiVal").innerText = `Paari (${data.wifi_rssi} dBm)`;
    }

    // Update power chart
    if (powerChart) {
        const dSolar = powerChart.data.datasets[0].data;
        const dLoad = powerChart.data.datasets[1].data;
        const dBat = powerChart.data.datasets[2].data;

        dSolar.push(data.solar_p);
        dLoad.push(data.load_p);
        dBat.push(data.battery_v);

        if (dSolar.length > 30) {
            dSolar.shift();
            dLoad.shift();
            dBat.shift();
        }
        powerChart.update();
    }

    recordedPowerHistory.push({
        ts: new Date().toISOString(),
        solar_w: data.solar_p,
        solar_v: data.solar_v,
        solar_ma: data.solar_i,
        bat_v: data.battery_v,
        bat_soc: data.battery_soc,
        load_w: data.load_p,
        load_ma: data.load_i,
        total_solar_wh: data.total_solar_wh || 0,
        total_load_wh: data.total_load_wh || 0
    });
    if (recordedPowerHistory.length > 3600) {
        recordedPowerHistory.shift();
    }
}

function updateRumblesUI(data) {
    document.getElementById("rumbleRms").innerText = data.rms_energy.toFixed(2);
    document.getElementById("rumbleProgress").style.width = `${Math.min(100, (data.rms_energy / 20) * 100)}%`;

    if (data.rumble_detected) {
        document.getElementById("rumbleAlertBanner").classList.remove("hidden");
        document.getElementById("rumbleBadge").innerText = "🚨 ELEPHANT DETECTED";
        document.getElementById("rumbleBadge").style.background = "rgba(244, 63, 94, 0.25)";
        document.getElementById("rumbleBadge").style.color = "#FDA4AF";
    } else {
        document.getElementById("rumbleBadge").innerText = "LISTENING (65 Hz)";
        document.getElementById("rumbleBadge").style.background = "";
        document.getElementById("rumbleBadge").style.color = "";
    }

    if (waveformChart && data.samples && data.samples.length > 0) {
        const wave = waveformChart.data.datasets[0].data;
        // Shift in samples as deviation from baseline (Pascals)
        data.samples.forEach(sample => {
            const devPa = (sample - baselinePressure) * 100.0;
            wave.push(devPa);
            if (wave.length > 128) wave.shift();
        });
        waveformChart.update();
    }

    // Save rumble telemetry packet into history
    recordedRumbleHistory.push({
        ts: new Date().toISOString(),
        rms_energy: data.rms_energy,
        peak_pa: data.peak_pa,
        rumble_detected: data.rumble_detected ? 1 : 0,
        samples: data.samples || []
    });
    if (recordedRumbleHistory.length > 1000) {
        recordedRumbleHistory.shift();
    }
}

// --- 6. DEMO TELEMETRY ENGINE ---
function startDemoTelemetryStream() {
    setInterval(() => {
        // If no fresh MQTT packets came in, update with live calibrated values from our earlier test
        const syntheticPower = {
            solar_p: 0.0,
            solar_v: 0.0,
            solar_i: 0.0,
            solar_charging: false,
            battery_v: 13.91 + (Math.random() * 0.03),
            battery_soc: 100,
            load_p: 1.88 + (Math.random() * 0.1),
            load_i: 135.0 + (Math.random() * 5.0),
            wifi_rssi: -52
        };
        updatePowerUI(syntheticPower);

        // Generate smooth 18Hz ambient infrasound wave
        const burst = [];
        const base = 996.75;
        for (let i = 0; i < 8; i++) {
            const noise = (Math.random() - 0.5) * 0.03;
            burst.push(base + noise);
        }
        updateRumblesUI({
            rms_energy: 2.1 + (Math.random() * 0.5),
            rumble_detected: false,
            samples: burst
        });
    }, 1000);
}

// Simulate an elephant rumble event
function simulateRumbleEvent() {
    logMessage("🚨 Simulated Elephant Rumble Triggered (18 Hz harmonic, 18 Pa peak)", "log-alert");
    const burst = [];
    const base = 996.75;
    for (let i = 0; i < 64; i++) {
        const sinWave = Math.sin((i / 64) * Math.PI * 2 * 18) * 0.18; // 18 Pa peak
        burst.push(base + sinWave);
    }
    updateRumblesUI({
        rms_energy: 14.85,
        rumble_detected: true,
        samples: burst
    });
}

function dismissRumbleAlert() {
    document.getElementById("rumbleAlertBanner").classList.add("hidden");
    logMessage("Rumble alert acknowledged by operator.");
}

function toggleRumbleFilter() {
    isBandpassActive = !isBandpassActive;
    const btn = document.getElementById("btnFilterRumble");
    if (isBandpassActive) {
        btn.classList.add("active");
        btn.innerText = "14–35Hz Bandpass (Active)";
    } else {
        btn.classList.remove("active");
        btn.innerText = "Raw Infrasound (Flat)";
    }
}

// Export Power Telemetry CSV
function exportDataCSV() {
    if (recordedPowerHistory.length === 0) {
        alert("No telemetry records in memory to export yet.");
        return;
    }
    let csv = "timestamp,solar_w,solar_v,solar_ma,battery_v,battery_soc,load_w,load_ma,total_solar_wh,total_load_wh\n";
    recordedPowerHistory.forEach(r => {
        csv += `${r.ts},${r.solar_w},${r.solar_v},${r.solar_ma || 0},${r.bat_v},${r.bat_soc || 100},${r.load_w},${r.load_ma},${r.total_solar_wh || 0},${r.total_load_wh || 0}\n`;
    });
    const blob = new Blob([csv], { type: "text/csv" });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `vananetra_power_${Date.now()}.csv`;
    a.click();
    logMessage(`📥 Downloaded Power Telemetry CSV (${recordedPowerHistory.length} rows)`, "log-success");
}

// Export 65Hz Infrasonic Rumble CSV
function exportRumbleCSV() {
    if (recordedRumbleHistory.length === 0) {
        alert("No infrasonic rumble records in memory to export yet.");
        return;
    }
    let csv = "timestamp,batch_rms_pa,batch_peak_pa,rumble_flag,sample_idx,pressure_hpa\n";
    let totalSamples = 0;
    recordedRumbleHistory.forEach(r => {
        if (r.samples && r.samples.length > 0) {
            r.samples.forEach((s, idx) => {
                csv += `${r.ts},${r.rms_energy},${r.peak_pa},${r.rumble_detected},${idx},${s}\n`;
                totalSamples++;
            });
        }
    });
    const blob = new Blob([csv], { type: "text/csv" });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `vananetra_rumbles_65Hz_${Date.now()}.csv`;
    a.click();
    logMessage(`📥 Downloaded Infrasound CSV (${recordedRumbleHistory.length} batches, ${totalSamples} samples)`, "log-success");
}

function logMessage(msg, className = "") {
    const consoleEl = document.getElementById("logConsole");
    if (!consoleEl) return;
    const entry = document.createElement("div");
    entry.className = `log-entry ${className}`;
    entry.innerText = `[${new Date().toLocaleTimeString()}] ${msg}`;
    consoleEl.appendChild(entry);
    consoleEl.scrollTop = consoleEl.scrollHeight;
}

function clearLogs() {
    document.getElementById("logConsole").innerHTML = "";
}

let hardwareUptimeReceived = false;

function startUptimeCounter() {
    let seconds = 0;
    setInterval(() => {
        if (!hardwareUptimeReceived) {
            seconds++;
            const mins = Math.floor(seconds / 60);
            const secs = seconds % 60;
            const el = document.getElementById("uptimeVal");
            if (el) el.innerText = `Connecting: ${mins}m ${secs}s`;
        }
    }, 1000);
}
